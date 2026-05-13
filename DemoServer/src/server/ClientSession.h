#pragma once

#include "server/DemoServerCommon.h"

namespace demo
{
class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
	ClientSession(SOCKET InSocket, std::weak_ptr<DedicatedServer> InOwner)
		: Socket(InSocket)
		, Owner(std::move(InOwner))
		, ConnectionId(GenerateId("conn"))
	{
	}

	~ClientSession()
	{
		Close();
	}

	void Start();
	void Close();
	void SendLine(const std::string& Line);

	SOCKET Socket = INVALID_SOCKET;
	std::weak_ptr<DedicatedServer> Owner;
	std::string ConnectionId;
	std::string PlayerId;
	std::string DisplayName = "Player";
	std::string CurrentMatchId;
	bool Authenticated = false;

private:
	void ReceiveLoop();

	std::atomic<bool> Connected{true};
	std::mutex SendMutex;
	std::thread ReceiveThread;
	std::string ReceiveBuffer;
};

} // namespace demo
