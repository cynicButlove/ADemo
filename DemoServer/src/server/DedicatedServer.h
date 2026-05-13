#pragma once

#include "server/MatchInstance.h"

namespace demo
{
class DedicatedServer : public std::enable_shared_from_this<DedicatedServer>
{
public:
	bool Start(const std::uint16_t Port)
	{
		ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (ListenSocket == INVALID_SOCKET)
		{
			std::cerr << "Failed to create listening socket.\n";
			return false;
		}

		sockaddr_in Address{};
		Address.sin_family = AF_INET;
		Address.sin_addr.s_addr = htonl(INADDR_ANY);
		Address.sin_port = htons(Port);

		if (bind(ListenSocket, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) == SOCKET_ERROR)
		{
			std::cerr << "Bind failed with error " << WSAGetLastError() << ".\n";
			closesocket(ListenSocket);
			ListenSocket = INVALID_SOCKET;
			return false;
		}

		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::cerr << "Listen failed with error " << WSAGetLastError() << ".\n";
			closesocket(ListenSocket);
			ListenSocket = INVALID_SOCKET;
			return false;
		}

		Running.store(true, std::memory_order_release);
		AcceptThread = std::thread(&DedicatedServer::AcceptLoop, this);
		TickThread = std::thread(&DedicatedServer::TickLoop, this);
		std::cout << "DemoServer listening on TCP " << Port << '\n';
		return true;
	}

	void Stop()
	{
		if (!Running.exchange(false, std::memory_order_acq_rel))
		{
			return;
		}

		if (ListenSocket != INVALID_SOCKET)
		{
			closesocket(ListenSocket);
			ListenSocket = INVALID_SOCKET;
		}

		if (AcceptThread.joinable())
		{
			AcceptThread.join();
		}
		if (TickThread.joinable())
		{
			TickThread.join();
		}

		std::vector<std::shared_ptr<ClientSession>> Sessions;
		{
			std::lock_guard<std::mutex> Lock(SessionMutex);
			for (const auto& Pair : SessionsByConnection)
			{
				Sessions.push_back(Pair.second);
			}
			SessionsByConnection.clear();
		}

		for (const std::shared_ptr<ClientSession>& Session : Sessions)
		{
			if (Session)
			{
				Session->Close();
			}
		}
	}

	void HandleCommand(const std::shared_ptr<ClientSession>& Session, const std::string& RawLine)
	{
		const std::string Line = Trim(RawLine);
		if (Line.empty())
		{
			return;
		}

		const std::size_t SpaceIndex = Line.find(' ');
		const std::string Command = ToUpperCopy(Line.substr(0, SpaceIndex));
		const std::string Arguments = SpaceIndex == std::string::npos ? std::string() : Trim(Line.substr(SpaceIndex + 1));

		if (Command == "AUTH")
		{
			const std::string DisplayName = SanitizeDisplayName(Arguments);
			Session->Authenticated = true;
			Session->DisplayName = DisplayName;
			if (Session->PlayerId.empty())
			{
				Session->PlayerId = GenerateId("player");
			}

			Session->SendLine("AUTH_OK " + Session->PlayerId + ' ' + DisplayName);
			return;
		}

		if (!Session->Authenticated)
		{
			Session->SendLine("ERROR not_authenticated Please AUTH <DisplayName> first.");
			return;
		}

		if (Command == "JOIN")
		{
			JoinMatch(Session, Arguments);
			return;
		}

		if (Command == "LEAVE")
		{
			LeaveMatch(Session, "client_requested");
			return;
		}

		if (Command == "INPUT")
		{
			PlayerInputState Input{};
			int Sprint = 0;
			int Crouch = 0;
			std::istringstream Stream(Arguments);
			if (!(Stream >> Input.Sequence >> Input.MoveX >> Input.MoveY >> Input.Yaw >> Input.Pitch >> Sprint >> Crouch))
			{
				Session->SendLine("ERROR invalid_input Format: INPUT <Seq> <MoveX> <MoveY> <Yaw> <Pitch> <Sprint0or1> <Crouch0or1>");
				return;
			}
			Input.Sprint = Sprint != 0;
			Input.Crouch = Crouch != 0;

			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before sending INPUT.");
				return;
			}
			Match->UpdateInput(Session->PlayerId, Input);
			return;
		}

		if (Command == "FIRE")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before sending FIRE.");
				return;
			}
			Match->QueueFire(Session->PlayerId);
			return;
		}

		if (Command == "RELOAD")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before sending RELOAD.");
				return;
			}
			Match->QueueReload(Session->PlayerId);
			return;
		}

		if (Command == "INTERACT_CONTAINER")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before interacting with containers.");
				return;
			}

			std::vector<std::shared_ptr<ClientSession>> Sessions;
			std::string ContainerStateLine;
			std::string ErrorCode;
			if (!Match->InteractContainer(Session->PlayerId, Arguments, ContainerStateLine, Sessions, ErrorCode))
			{
				Session->SendLine("ERROR " + ErrorCode + " Failed to interact with the requested container.");
				return;
			}

			for (const std::shared_ptr<ClientSession>& TargetSession : Sessions)
			{
				if (TargetSession)
				{
					TargetSession->SendLine(ContainerStateLine);
				}
			}
			return;
		}

		if (Command == "TAKE_CONTAINER_ITEM")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before looting containers.");
				return;
			}

			std::string ContainerId;
			int SlotIndex = -1;
			std::istringstream Stream(Arguments);
			if (!(Stream >> ContainerId >> SlotIndex))
			{
				Session->SendLine("ERROR invalid_take_container_item Format: TAKE_CONTAINER_ITEM <ContainerId> <SlotIndex>");
				return;
			}

			std::vector<std::shared_ptr<ClientSession>> Sessions;
			std::string InventoryLine;
			std::string ContainerStateLine;
			std::string ErrorCode;
			if (!Match->TakeContainerItem(
					Session->PlayerId,
					ContainerId,
					SlotIndex,
					InventoryLine,
					ContainerStateLine,
					Sessions,
					ErrorCode))
			{
				Session->SendLine("ERROR " + ErrorCode + " Failed to take the requested container item.");
				return;
			}

			Session->SendLine(InventoryLine);
			for (const std::shared_ptr<ClientSession>& TargetSession : Sessions)
			{
				if (TargetSession)
				{
					TargetSession->SendLine(ContainerStateLine);
				}
			}
			return;
		}

		if (Command == "PICKUP_LOOT")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before picking up loot.");
				return;
			}

			std::string InventoryLine;
			std::string ErrorCode;
			if (!Match->PickupWorldLoot(Session->PlayerId, Arguments, InventoryLine, ErrorCode))
			{
				Session->SendLine("ERROR " + ErrorCode + " Failed to pick up the requested loot.");
				return;
			}

			Session->SendLine(InventoryLine);
			return;
		}

		if (Command == "DROP_ITEM")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before dropping items.");
				return;
			}

			int SlotIndex = -1;
			std::istringstream Stream(Arguments);
			if (!(Stream >> SlotIndex))
			{
				Session->SendLine("ERROR invalid_drop_item Format: DROP_ITEM <SlotIndex>");
				return;
			}

			std::string InventoryLine;
			std::string ErrorCode;
			if (!Match->DropInventoryItem(Session->PlayerId, SlotIndex, InventoryLine, ErrorCode))
			{
				Session->SendLine("ERROR " + ErrorCode + " Failed to drop the requested item.");
				return;
			}

			Session->SendLine(InventoryLine);
			return;
		}

		if (Command == "USE_ITEM")
		{
			const std::shared_ptr<MatchInstance> Match = FindMatch(Session->CurrentMatchId);
			if (!Match)
			{
				Session->SendLine("ERROR not_in_match Join a match before using items.");
				return;
			}

			int SlotIndex = -1;
			std::istringstream Stream(Arguments);
			if (!(Stream >> SlotIndex))
			{
				Session->SendLine("ERROR invalid_use_item Format: USE_ITEM <SlotIndex>");
				return;
			}

			std::string InventoryLine;
			std::string ErrorCode;
			if (!Match->UseInventoryItem(Session->PlayerId, SlotIndex, InventoryLine, ErrorCode))
			{
				Session->SendLine("ERROR " + ErrorCode + " Failed to use the requested item.");
				return;
			}

			Session->SendLine(InventoryLine);
			return;
		}

		if (Command == "PING")
		{
			Session->SendLine("PONG " + Arguments + ' ' + std::to_string(NowMs()));
			return;
		}

		if (Command == "STATUS")
		{
			Session->SendLine("STATUS " + BuildStatusString());
			return;
		}

		Session->SendLine("ERROR unknown_command Unsupported command: " + Command);
	}

	void HandleDisconnect(const std::shared_ptr<ClientSession>& Session)
	{
		LeaveMatch(Session, "disconnected");
		std::lock_guard<std::mutex> Lock(SessionMutex);
		SessionsByConnection.erase(Session->ConnectionId);
	}

	void PrintStatus() const
	{
		std::cout << BuildStatusString() << '\n';
	}

private:
	void AcceptLoop()
	{
		while (Running.load(std::memory_order_acquire))
		{
			sockaddr_in ClientAddress{};
			int Length = sizeof(ClientAddress);
			SOCKET ClientSocket = accept(ListenSocket, reinterpret_cast<sockaddr*>(&ClientAddress), &Length);
			if (ClientSocket == INVALID_SOCKET)
			{
				if (Running.load(std::memory_order_acquire))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
				}
				continue;
			}

			auto Session = std::make_shared<ClientSession>(ClientSocket, weak_from_this());
			{
				std::lock_guard<std::mutex> Lock(SessionMutex);
				SessionsByConnection.emplace(Session->ConnectionId, Session);
			}

			Session->SendLine("WELCOME " + Session->ConnectionId + " 1 " + std::to_string(NowMs()));
			Session->Start();
		}
	}

	void TickLoop()
	{
		const auto TickDuration = std::chrono::milliseconds(1000 / kTickRate);
		while (Running.load(std::memory_order_acquire))
		{
			const std::uint64_t TickNow = NowMs();
			std::vector<std::shared_ptr<MatchInstance>> Matches;
			{
				std::lock_guard<std::mutex> Lock(MatchMutex);
				for (const auto& Pair : MatchesById)
				{
					Matches.push_back(Pair.second);
				}
			}

			for (const std::shared_ptr<MatchInstance>& Match : Matches)
			{
				if (Match)
				{
					Match->TickAndBroadcast(TickNow, 1.0f / static_cast<float>(kTickRate));
				}
			}

			RemoveEmptyMatches();
			std::this_thread::sleep_for(TickDuration);
		}
	}

	void JoinMatch(const std::shared_ptr<ClientSession>& Session, const std::string& RequestedMatchId)
	{
		if (!Session->CurrentMatchId.empty())
		{
			Session->SendLine("ERROR already_in_match Leave the current match before joining another.");
			return;
		}

		std::shared_ptr<MatchInstance> Match;
		{
			std::lock_guard<std::mutex> Lock(MatchMutex);
			if (!RequestedMatchId.empty())
			{
				auto It = MatchesById.find(RequestedMatchId);
				if (It != MatchesById.end())
				{
					Match = It->second;
				}
			}

			if (!Match)
			{
				for (const auto& Pair : MatchesById)
				{
					if (Pair.second && Pair.second->HasCapacity())
					{
						Match = Pair.second;
						break;
					}
				}
			}

			if (!Match)
			{
				const std::string NewMatchId = RequestedMatchId.empty() ? GenerateId("match") : RequestedMatchId;
				Match = std::make_shared<MatchInstance>(NewMatchId);
				MatchesById.emplace(NewMatchId, Match);
			}
		}

		if (!Match->AddPlayer(Session))
		{
			Session->SendLine("ERROR match_full The requested match is already full.");
			return;
		}
		Session->CurrentMatchId = Match->GetMatchId();
		Session->SendLine(Match->BuildJoinedLine());
		Session->SendLine(Match->BuildSnapshotLine());
		Session->SendLine(Match->BuildInventoryLine(Session->PlayerId));
	}

	void LeaveMatch(const std::shared_ptr<ClientSession>& Session, const std::string& Reason)
	{
		if (Session->CurrentMatchId.empty())
		{
			return;
		}

		const std::string MatchId = Session->CurrentMatchId;
		const std::shared_ptr<MatchInstance> Match = FindMatch(MatchId);
		if (Match)
		{
			Match->RemovePlayer(Session->PlayerId);
		}

		Session->CurrentMatchId.clear();
		Session->SendLine("MATCH_LEFT " + MatchId + ' ' + Reason);
	}

	std::shared_ptr<MatchInstance> FindMatch(const std::string& MatchId) const
	{
		if (MatchId.empty())
		{
			return nullptr;
		}

		std::lock_guard<std::mutex> Lock(MatchMutex);
		auto It = MatchesById.find(MatchId);
		return It == MatchesById.end() ? nullptr : It->second;
	}

	void RemoveEmptyMatches()
	{
		std::lock_guard<std::mutex> Lock(MatchMutex);
		for (auto It = MatchesById.begin(); It != MatchesById.end();)
		{
			if (!It->second || It->second->IsEmpty())
			{
				It = MatchesById.erase(It);
			}
			else
			{
				++It;
			}
		}
	}

	std::string BuildStatusString() const
	{
		std::ostringstream Stream;
		std::size_t SessionCount = 0;
		{
			std::lock_guard<std::mutex> Lock(SessionMutex);
			SessionCount = SessionsByConnection.size();
		}

		std::vector<std::string> MatchDescriptions;
		{
			std::lock_guard<std::mutex> Lock(MatchMutex);
			for (const auto& Pair : MatchesById)
			{
				if (Pair.second)
				{
					MatchDescriptions.push_back(Pair.second->Describe());
				}
			}
		}

		Stream << "{\"connectedSessions\":" << SessionCount << ",\"activeMatches\":" << MatchDescriptions.size()
			   << ",\"matches\":[";
		for (std::size_t Index = 0; Index < MatchDescriptions.size(); ++Index)
		{
			if (Index > 0)
			{
				Stream << ',';
			}
			Stream << '\"' << EscapeJson(MatchDescriptions[Index]) << '\"';
		}
		Stream << "]}";
		return Stream.str();
	}

	std::atomic<bool> Running{false};
	SOCKET ListenSocket = INVALID_SOCKET;
	mutable std::mutex SessionMutex;
	mutable std::mutex MatchMutex;
	std::unordered_map<std::string, std::shared_ptr<ClientSession>> SessionsByConnection;
	std::unordered_map<std::string, std::shared_ptr<MatchInstance>> MatchesById;
	std::thread AcceptThread;
	std::thread TickThread;
};

inline void ClientSession::Start()
{
	ReceiveThread = std::thread(&ClientSession::ReceiveLoop, this);
}

inline void ClientSession::Close()
{
	const bool WasConnected = Connected.exchange(false, std::memory_order_acq_rel);
	if (WasConnected && Socket != INVALID_SOCKET)
	{
		shutdown(Socket, SD_BOTH);
		closesocket(Socket);
		Socket = INVALID_SOCKET;
	}

	if (ReceiveThread.joinable() && ReceiveThread.get_id() != std::this_thread::get_id())
	{
		ReceiveThread.join();
	}
}

inline void ClientSession::SendLine(const std::string& Line)
{
	if (!Connected.load(std::memory_order_acquire) || Socket == INVALID_SOCKET)
	{
		return;
	}

	const std::string Payload = Line + "\n";
	std::lock_guard<std::mutex> Lock(SendMutex);
	std::size_t SentBytes = 0;
	while (SentBytes < Payload.size())
	{
		const int Result = send(Socket, Payload.data() + SentBytes, static_cast<int>(Payload.size() - SentBytes), 0);
		if (Result == SOCKET_ERROR || Result == 0)
		{
			break;
		}
		SentBytes += static_cast<std::size_t>(Result);
	}
}

inline void ClientSession::ReceiveLoop()
{
	char Buffer[4096];
	while (Connected.load(std::memory_order_acquire))
	{
		const int ReceivedBytes = recv(Socket, Buffer, static_cast<int>(sizeof(Buffer)), 0);
		if (ReceivedBytes <= 0)
		{
			break;
		}

		ReceiveBuffer.append(Buffer, Buffer + ReceivedBytes);
		if (ReceiveBuffer.size() > 64 * 1024)
		{
			SendLine("ERROR inbound_too_large Disconnecting because a command exceeded 64KB.");
			break;
		}

		std::size_t NewLineIndex = std::string::npos;
		while ((NewLineIndex = ReceiveBuffer.find('\n')) != std::string::npos)
		{
			std::string Line = Trim(ReceiveBuffer.substr(0, NewLineIndex));
			ReceiveBuffer.erase(0, NewLineIndex + 1);
			if (Line.empty())
			{
				continue;
			}

			if (auto Server = Owner.lock())
			{
				Server->HandleCommand(shared_from_this(), Line);
			}
		}
	}

	Connected.store(false, std::memory_order_release);
	if (Socket != INVALID_SOCKET)
	{
		closesocket(Socket);
		Socket = INVALID_SOCKET;
	}

	if (auto Server = Owner.lock())
	{
		Server->HandleDisconnect(shared_from_this());
	}
}

} // namespace demo
