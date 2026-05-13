#if 0
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace demo
{
constexpr std::uint16_t kDefaultPort = 7777;
constexpr int kTickRate = 20;
constexpr int kMaxPlayersPerMatch = 8;
constexpr std::uint64_t kFireCooldownMs = 120;
constexpr std::uint64_t kRespawnDelayMs = 4000;
constexpr float kMaxHealth = 100.0f;
constexpr float kMaxArmor = 50.0f;
constexpr float kMaxStamina = 100.0f;
constexpr float kBaseMoveSpeed = 650.0f;
constexpr float kSprintMultiplier = 1.35f;
constexpr float kCrouchMultiplier = 0.55f;
constexpr float kStaminaDrainPerSecond = 22.0f;
constexpr float kStaminaRegenPerSecond = 16.0f;
constexpr float kShotRange = 1800.0f;
constexpr float kShotHitRadius = 70.0f;
constexpr float kShotDamage = 34.0f;
constexpr float kArmorAbsorbFraction = 0.5f;
constexpr float kInteractRange = 260.0f;
constexpr float kMatchDurationSeconds = 900.0f;
constexpr float kExtractionActivationDelaySeconds = 20.0f;
constexpr float kExtractionZoneRadius = 280.0f;
constexpr float kExtractionDurationSeconds = 5.0f;
constexpr int kDedicatedBotCount = 5;
constexpr float kBotPreferredRange = 900.0f;
constexpr float kBotRetreatRange = 375.0f;
constexpr float kBotFireRange = 1300.0f;
constexpr float kBotSprintDistance = 1100.0f;
constexpr float kBotStrafeStrength = 0.35f;
constexpr const char* kMapName = "L_DemoExtractionGraybox";
constexpr const char* kDefaultLootItemId = "Item_DemoValuable";
constexpr int kDefaultLootItemValue = 1000;

struct Vec3
{
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
};

struct Rotation
{
	float Yaw = 0.0f;
	float Pitch = 0.0f;
};

struct PlayerInputState
{
	std::uint32_t Sequence = 0;
	float MoveX = 0.0f;
	float MoveY = 0.0f;
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	bool Sprint = false;
	bool Crouch = false;
};

struct PlayerRuntimeState
{
	std::string PlayerId;
	std::string DisplayName;
	Vec3 Position;
	Vec3 Velocity;
	Rotation Look;
	float Health = kMaxHealth;
	float Armor = kMaxArmor;
	float Stamina = kMaxStamina;
	bool Alive = true;
	bool IsBot = false;
	bool Extracted = false;
	int Kills = 0;
	int Deaths = 0;
	int ExtractedValue = 0;
	std::uint32_t LastProcessedInputSequence = 0;
	std::uint64_t RespawnAtMs = 0;
};

struct SpawnPoint
{
	float X;
	float Y;
	float Z;
	float Yaw;
};

constexpr SpawnPoint kSpawnPoints[] = {
	{-900.0f, -900.0f, 100.0f, 45.0f},
	{900.0f, -900.0f, 100.0f, 135.0f},
	{-900.0f, 900.0f, 100.0f, -45.0f},
	{900.0f, 900.0f, 100.0f, -135.0f},
	{0.0f, -1200.0f, 100.0f, 90.0f},
	{0.0f, 1200.0f, 100.0f, -90.0f},
	{-1200.0f, 0.0f, 100.0f, 0.0f},
	{1200.0f, 0.0f, 100.0f, 180.0f},
};

constexpr SpawnPoint kBotSpawnPoints[] = {
	{-350.0f, -300.0f, 100.0f, 35.0f},
	{450.0f, -420.0f, 100.0f, 160.0f},
	{-520.0f, 520.0f, 100.0f, -20.0f},
	{560.0f, 460.0f, 100.0f, -155.0f},
	{0.0f, 0.0f, 100.0f, 90.0f},
};

struct InventoryEntry
{
	std::string ItemId;
	int Quantity = 0;
};

struct WorldContainerState
{
	std::string ContainerId;
	Vec3 Position;
	bool Opened = false;
	std::vector<InventoryEntry> Contents;
};

struct WorldLootState
{
	std::string LootId;
	Vec3 Position;
	InventoryEntry Entry;
};

enum class ExtractionZoneStatus
{
	Inactive,
	Available,
	Extracting,
	Completed
};

struct ExtractionZoneState
{
	std::string ZoneId;
	std::string DisplayName;
	Vec3 Position;
	float Radius = kExtractionZoneRadius;
	ExtractionZoneStatus Status = ExtractionZoneStatus::Inactive;
	std::string ExtractingPlayerId;
	float Progress = 0.0f;
};

template <typename T>
T Clamp(const T Value, const T MinValue, const T MaxValue)
{
	return std::max(MinValue, std::min(MaxValue, Value));
}

std::uint64_t NowMs()
{
	return static_cast<std::uint64_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count());
}

float NormalizeDegrees(float Value)
{
	while (Value > 180.0f)
	{
		Value -= 360.0f;
	}
	while (Value <= -180.0f)
	{
		Value += 360.0f;
	}
	return Value;
}

std::string Trim(const std::string& Value)
{
	const std::size_t First = Value.find_first_not_of(" \t\r\n");
	if (First == std::string::npos)
	{
		return {};
	}
	const std::size_t Last = Value.find_last_not_of(" \t\r\n");
	return Value.substr(First, Last - First + 1);
}

std::string ToUpperCopy(std::string Value)
{
	std::transform(Value.begin(), Value.end(), Value.begin(), [](const unsigned char Character) {
		return static_cast<char>(std::toupper(Character));
	});
	return Value;
}

std::string EscapeJson(const std::string& Value)
{
	std::ostringstream Stream;
	for (const char Character : Value)
	{
		switch (Character)
		{
		case '\\':
			Stream << "\\\\";
			break;
		case '"':
			Stream << "\\\"";
			break;
		case '\n':
			Stream << "\\n";
			break;
		case '\r':
			Stream << "\\r";
			break;
		case '\t':
			Stream << "\\t";
			break;
		default:
			Stream << Character;
			break;
		}
	}
	return Stream.str();
}

std::string GenerateId(const char* Prefix)
{
	static std::atomic<std::uint64_t> Counter{1};
	std::ostringstream Stream;
	Stream << Prefix << '_' << Counter.fetch_add(1, std::memory_order_relaxed);
	return Stream.str();
}

std::string SanitizeDisplayName(const std::string& Raw)
{
	std::string Result = Trim(Raw);
	if (Result.empty())
	{
		Result = "Player";
	}
	if (Result.size() > 48)
	{
		Result.resize(48);
	}
	return Result;
}

const char* ToString(const ExtractionZoneStatus Status)
{
	switch (Status)
	{
	case ExtractionZoneStatus::Inactive:
		return "Inactive";
	case ExtractionZoneStatus::Available:
		return "Available";
	case ExtractionZoneStatus::Extracting:
		return "Extracting";
	case ExtractionZoneStatus::Completed:
		return "Completed";
	default:
		return "Inactive";
	}
}

int GetItemBaseValue(const std::string& ItemId)
{
	if (ItemId == kDefaultLootItemId)
	{
		return kDefaultLootItemValue;
	}
	return 100;
}

float GetItemHealAmount(const std::string& ItemId)
{
	const std::string UpperItemId = ToUpperCopy(ItemId);
	if (UpperItemId.find("BANDAGE") != std::string::npos)
	{
		return 20.0f;
	}
	if (UpperItemId.find("MEDKIT") != std::string::npos)
	{
		return 55.0f;
	}
	if (UpperItemId.find("MED") != std::string::npos ||
		UpperItemId.find("STIM") != std::string::npos ||
		UpperItemId.find("PAIN") != std::string::npos)
	{
		return 35.0f;
	}
	return 0.0f;
}

float GetItemArmorAmount(const std::string& ItemId)
{
	const std::string UpperItemId = ToUpperCopy(ItemId);
	if (UpperItemId.find("PLATE") != std::string::npos)
	{
		return 25.0f;
	}
	if (UpperItemId.find("ARMOR") != std::string::npos ||
		UpperItemId.find("VEST") != std::string::npos)
	{
		return 20.0f;
	}
	return 0.0f;
}

class DedicatedServer;

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

class MatchInstance
{
public:
	explicit MatchInstance(std::string InId)
		: MatchId(std::move(InId))
	{
		InitializeWorldContainers();
		InitializeExtractionZones();
		InitializeBots();
	}

	std::string GetMatchId() const
	{
		return MatchId;
	}

	bool HasCapacity() const
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		return GetHumanParticipantCountLocked() < static_cast<std::size_t>(kMaxPlayersPerMatch);
	}

	bool IsEmpty() const
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		return GetHumanParticipantCountLocked() == 0;
	}

	bool AddPlayer(const std::shared_ptr<ClientSession>& Session)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		if (Participants.find(Session->PlayerId) != Participants.end())
		{
			return true;
		}
		const std::size_t HumanParticipantCount = GetHumanParticipantCountLocked();
		if (HumanParticipantCount >= static_cast<std::size_t>(kMaxPlayersPerMatch))
		{
			return false;
		}

		const SpawnPoint& Spawn = kSpawnPoints[HumanParticipantCount % std::size(kSpawnPoints)];
		Participant NewParticipant;
		NewParticipant.Session = Session;
		NewParticipant.State.PlayerId = Session->PlayerId;
		NewParticipant.State.DisplayName = Session->DisplayName;
		NewParticipant.State.IsBot = false;
		NewParticipant.State.Position = {Spawn.X, Spawn.Y, Spawn.Z};
		NewParticipant.State.Look = {Spawn.Yaw, 0.0f};
		NewParticipant.Input.Yaw = Spawn.Yaw;
		Participants.emplace(Session->PlayerId, std::move(NewParticipant));

		RecentEvents.push_back("{\"type\":\"player_joined\",\"playerId\":\"" + EscapeJson(Session->PlayerId) +
							  "\",\"displayName\":\"" + EscapeJson(Session->DisplayName) + "\"}");
		return true;
	}

	void RemovePlayer(const std::string& PlayerId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto It = Participants.find(PlayerId);
		if (It == Participants.end())
		{
			return;
		}

		RecentEvents.push_back("{\"type\":\"player_left\",\"playerId\":\"" + EscapeJson(PlayerId) + "\"}");
		Participants.erase(It);
	}

	void UpdateInput(const std::string& PlayerId, const PlayerInputState& Input)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto It = Participants.find(PlayerId);
		if (It == Participants.end())
		{
			return;
		}
		if (Input.Sequence < It->second.Input.Sequence)
		{
			return;
		}
		It->second.Input = Input;
	}

	void QueueFire(const std::string& PlayerId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto It = Participants.find(PlayerId);
		if (It != Participants.end())
		{
			It->second.PendingFire = true;
		}
	}

	bool InteractContainer(
		const std::string& PlayerId,
		const std::string& ContainerId,
		std::string& OutContainerStateLine,
		std::vector<std::shared_ptr<ClientSession>>& OutSessions,
		std::string& OutError)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto PlayerIt = Participants.find(PlayerId);
		if (PlayerIt == Participants.end())
		{
			OutError = "not_in_match";
			return false;
		}
		if (!PlayerIt->second.State.Alive || PlayerIt->second.State.Extracted)
		{
			OutError = "player_unavailable";
			return false;
		}

		WorldContainerState* Container = FindContainerByIdLocked(ContainerId);
		if (Container == nullptr)
		{
			OutError = "container_not_found";
			return false;
		}

		if (!PlayerNearContainerLocked(PlayerIt->second, *Container))
		{
			OutError = "container_out_of_range";
			return false;
		}

		Container->Opened = true;
		RecentEvents.push_back("{\"type\":\"container_opened\",\"containerId\":\"" + EscapeJson(Container->ContainerId) +
							  "\",\"playerId\":\"" + EscapeJson(PlayerId) + "\"}");

		OutContainerStateLine = "CONTAINER_STATE " + BuildContainerStateJsonLocked(*Container);
		for (auto& Pair : Participants)
		{
			OutSessions.push_back(Pair.second.Session);
		}
		return true;
	}

	bool TakeContainerItem(
		const std::string& PlayerId,
		const std::string& ContainerId,
		const int SlotIndex,
		std::string& OutInventoryLine,
		std::string& OutContainerStateLine,
		std::vector<std::shared_ptr<ClientSession>>& OutSessions,
		std::string& OutError)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto PlayerIt = Participants.find(PlayerId);
		if (PlayerIt == Participants.end())
		{
			OutError = "not_in_match";
			return false;
		}
		if (!PlayerIt->second.State.Alive || PlayerIt->second.State.Extracted)
		{
			OutError = "player_unavailable";
			return false;
		}

		WorldContainerState* Container = FindContainerByIdLocked(ContainerId);
		if (Container == nullptr)
		{
			OutError = "container_not_found";
			return false;
		}

		if (!PlayerNearContainerLocked(PlayerIt->second, *Container))
		{
			OutError = "container_out_of_range";
			return false;
		}

		if (!Container->Opened)
		{
			OutError = "container_closed";
			return false;
		}

		if (SlotIndex < 0 || SlotIndex >= static_cast<int>(Container->Contents.size()))
		{
			OutError = "invalid_slot";
			return false;
		}

		InventoryEntry& Entry = Container->Contents[SlotIndex];
		if (Entry.Quantity <= 0 || Entry.ItemId.empty())
		{
			OutError = "slot_empty";
			return false;
		}

		AddInventoryEntryLocked(PlayerIt->second.Inventory, Entry.ItemId, Entry.Quantity);
		RecentEvents.push_back("{\"type\":\"item_looted\",\"containerId\":\"" + EscapeJson(Container->ContainerId) +
							  "\",\"playerId\":\"" + EscapeJson(PlayerId) + "\",\"itemId\":\"" +
							  EscapeJson(Entry.ItemId) + "\",\"quantity\":" + std::to_string(Entry.Quantity) + "}");
		Entry.ItemId.clear();
		Entry.Quantity = 0;
		Container->Contents.erase(
			std::remove_if(
				Container->Contents.begin(),
				Container->Contents.end(),
				[](const InventoryEntry& ContainerEntry) {
					return ContainerEntry.ItemId.empty() || ContainerEntry.Quantity <= 0;
				}),
			Container->Contents.end());

		OutInventoryLine = "INVENTORY " + BuildInventoryJsonLocked(PlayerIt->second);
		OutContainerStateLine = "CONTAINER_STATE " + BuildContainerStateJsonLocked(*Container);
		for (auto& Pair : Participants)
		{
			OutSessions.push_back(Pair.second.Session);
		}
		return true;
	}

	bool PickupWorldLoot(
		const std::string& PlayerId,
		const std::string& LootId,
		std::string& OutInventoryLine,
		std::string& OutError)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto PlayerIt = Participants.find(PlayerId);
		if (PlayerIt == Participants.end())
		{
			OutError = "not_in_match";
			return false;
		}
		if (!PlayerIt->second.State.Alive || PlayerIt->second.State.Extracted)
		{
			OutError = "player_unavailable";
			return false;
		}

		const int LootIndex = FindWorldLootIndexByIdLocked(LootId);
		if (LootIndex < 0)
		{
			OutError = "loot_not_found";
			return false;
		}

		const WorldLootState& Loot = WorldLoot[LootIndex];
		if (!PlayerNearLootLocked(PlayerIt->second, Loot))
		{
			OutError = "loot_out_of_range";
			return false;
		}

		AddInventoryEntryLocked(PlayerIt->second.Inventory, Loot.Entry.ItemId, Loot.Entry.Quantity);
		RecentEvents.push_back("{\"type\":\"item_picked_up\",\"playerId\":\"" + EscapeJson(PlayerId) +
							  "\",\"lootId\":\"" + EscapeJson(Loot.LootId) + "\",\"itemId\":\"" +
							  EscapeJson(Loot.Entry.ItemId) + "\",\"quantity\":" +
							  std::to_string(Loot.Entry.Quantity) + "}");
		WorldLoot.erase(WorldLoot.begin() + LootIndex);
		OutInventoryLine = "INVENTORY " + BuildInventoryJsonLocked(PlayerIt->second);
		return true;
	}

	bool DropInventoryItem(
		const std::string& PlayerId,
		const int SlotIndex,
		std::string& OutInventoryLine,
		std::string& OutError)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto PlayerIt = Participants.find(PlayerId);
		if (PlayerIt == Participants.end())
		{
			OutError = "not_in_match";
			return false;
		}
		if (!PlayerIt->second.State.Alive || PlayerIt->second.State.Extracted)
		{
			OutError = "player_unavailable";
			return false;
		}
		if (SlotIndex < 0 || SlotIndex >= static_cast<int>(PlayerIt->second.Inventory.size()))
		{
			OutError = "invalid_slot";
			return false;
		}

		const InventoryEntry DroppedEntry = PlayerIt->second.Inventory[SlotIndex];
		if (DroppedEntry.ItemId.empty() || DroppedEntry.Quantity <= 0)
		{
			OutError = "slot_empty";
			return false;
		}

		const float Radians = PlayerIt->second.State.Look.Yaw * 3.14159265f / 180.0f;
		const Vec3 DropPosition{
			PlayerIt->second.State.Position.X + std::cos(Radians) * 120.0f,
			PlayerIt->second.State.Position.Y + std::sin(Radians) * 120.0f,
			40.0f};
		CreateWorldLootLocked(DropPosition, DroppedEntry);
		RecentEvents.push_back("{\"type\":\"item_dropped\",\"playerId\":\"" + EscapeJson(PlayerId) +
							  "\",\"itemId\":\"" + EscapeJson(DroppedEntry.ItemId) + "\",\"quantity\":" +
							  std::to_string(DroppedEntry.Quantity) + "}");
		PlayerIt->second.Inventory.erase(PlayerIt->second.Inventory.begin() + SlotIndex);
		OutInventoryLine = "INVENTORY " + BuildInventoryJsonLocked(PlayerIt->second);
		return true;
	}

	bool UseInventoryItem(
		const std::string& PlayerId,
		const int SlotIndex,
		std::string& OutInventoryLine,
		std::string& OutError)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto PlayerIt = Participants.find(PlayerId);
		if (PlayerIt == Participants.end())
		{
			OutError = "not_in_match";
			return false;
		}
		if (!PlayerIt->second.State.Alive || PlayerIt->second.State.Extracted)
		{
			OutError = "player_unavailable";
			return false;
		}
		if (SlotIndex < 0 || SlotIndex >= static_cast<int>(PlayerIt->second.Inventory.size()))
		{
			OutError = "invalid_slot";
			return false;
		}

		InventoryEntry& Entry = PlayerIt->second.Inventory[SlotIndex];
		if (Entry.ItemId.empty() || Entry.Quantity <= 0)
		{
			OutError = "slot_empty";
			return false;
		}

		const std::string ItemId = Entry.ItemId;
		const float HealAmount = GetItemHealAmount(Entry.ItemId);
		const float ArmorAmount = GetItemArmorAmount(Entry.ItemId);
		if (HealAmount <= 0.0f && ArmorAmount <= 0.0f)
		{
			OutError = "item_not_usable";
			return false;
		}

		if (HealAmount > 0.0f)
		{
			PlayerIt->second.State.Health = std::min(kMaxHealth, PlayerIt->second.State.Health + HealAmount);
		}
		if (ArmorAmount > 0.0f)
		{
			PlayerIt->second.State.Armor = std::min(kMaxArmor, PlayerIt->second.State.Armor + ArmorAmount);
		}
		Entry.Quantity -= 1;
		if (Entry.Quantity <= 0)
		{
			PlayerIt->second.Inventory.erase(PlayerIt->second.Inventory.begin() + SlotIndex);
		}

		RecentEvents.push_back("{\"type\":\"item_used\",\"playerId\":\"" + EscapeJson(PlayerId) +
							  "\",\"itemId\":\"" + EscapeJson(ItemId) + "\",\"heal\":" +
							  std::to_string(static_cast<int>(HealAmount)) + ",\"armor\":" +
							  std::to_string(static_cast<int>(ArmorAmount)) + "}");
		OutInventoryLine = "INVENTORY " + BuildInventoryJsonLocked(PlayerIt->second);
		return true;
	}

	std::string BuildInventoryLine(const std::string& PlayerId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto It = Participants.find(PlayerId);
		if (It == Participants.end())
		{
			return "INVENTORY {\"items\":[]}";
		}
		return "INVENTORY " + BuildInventoryJsonLocked(It->second);
	}

	std::string BuildJoinedLine()
	{
		std::ostringstream Stream;
		Stream << "MATCH_JOINED " << MatchId << ' ' << kTickRate << ' ' << kMaxPlayersPerMatch << ' ' << kMapName;
		return Stream.str();
	}

	std::string BuildSnapshotLine()
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		return "SNAPSHOT " + BuildSnapshotJsonLocked(NowMs(), {});
	}

	std::string Describe() const
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		std::ostringstream Stream;
		Stream << MatchId << " players=" << GetHumanParticipantCountLocked()
			   << " bots=" << (Participants.size() - GetHumanParticipantCountLocked())
			   << " tick=" << TickCounter;
		return Stream.str();
	}

	void TickAndBroadcast(const std::uint64_t Now, const float DeltaSeconds)
	{
		std::vector<std::shared_ptr<ClientSession>> Sessions;
		std::string SnapshotLine;

		{
			std::lock_guard<std::mutex> Lock(Mutex);
			++TickCounter;
			MatchElapsedSeconds += DeltaSeconds;
			std::vector<std::string> Events;
			Events.swap(RecentEvents);

			UpdateExtractionAvailabilityLocked();
			UpdateExtractionZonesLocked(DeltaSeconds, Events);
			UpdateBotsLocked(Now);

			if (!bMatchExpired && MatchElapsedSeconds >= kMatchDurationSeconds)
			{
				bMatchExpired = true;
				Events.push_back("{\"type\":\"match_expired\"}");
			}

			for (auto& Pair : Participants)
			{
				Participant& Entry = Pair.second;
				if (!Entry.State.Alive)
				{
					if (Entry.State.RespawnAtMs > 0 && Now >= Entry.State.RespawnAtMs)
					{
						RespawnParticipantLocked(Entry);
						Events.push_back("{\"type\":\"player_respawned\",\"playerId\":\"" +
										EscapeJson(Entry.State.PlayerId) + "\"}");
					}
					continue;
				}

				if (Entry.State.Extracted || bMatchExpired)
				{
					Entry.State.Velocity = {};
					continue;
				}
				ApplyMovementLocked(Entry, DeltaSeconds);
			}

			if (!bMatchExpired)
			{
				for (auto& Pair : Participants)
				{
					Participant& Entry = Pair.second;
					if (!Entry.PendingFire || !Entry.State.Alive || Entry.State.Extracted)
					{
						Entry.PendingFire = false;
						continue;
					}

					Entry.PendingFire = false;
					if (Now - Entry.LastFireAtMs < kFireCooldownMs)
					{
						continue;
					}

					Entry.LastFireAtMs = Now;
					ResolveShotLocked(Entry, Now, Events);
				}
			}

			SnapshotLine = "SNAPSHOT " + BuildSnapshotJsonLocked(Now, Events);
			for (auto& Pair : Participants)
			{
				Sessions.push_back(Pair.second.Session);
			}
		}

		for (const std::shared_ptr<ClientSession>& Session : Sessions)
		{
			if (Session)
			{
				Session->SendLine(SnapshotLine);
			}
		}
	}

private:
	struct Participant
	{
		std::shared_ptr<ClientSession> Session;
		PlayerRuntimeState State;
		PlayerInputState Input;
		std::vector<InventoryEntry> Inventory;
		std::string CurrentTargetId;
		float StrafingDirection = 1.0f;
		bool PendingFire = false;
		std::uint64_t LastFireAtMs = 0;
	};

	std::size_t GetHumanParticipantCountLocked() const
	{
		std::size_t Count = 0;
		for (const auto& Pair : Participants)
		{
			if (!Pair.second.State.IsBot)
			{
				++Count;
			}
		}
		return Count;
	}

	void InitializeWorldContainers()
	{
		Containers.clear();

		auto AddContainer = [this](const float X, const float Y, const std::initializer_list<int> Quantities) {
			WorldContainerState Container;
			Container.ContainerId = GenerateId("container");
			Container.Position = {X, Y, 50.0f};
			for (const int Quantity : Quantities)
			{
				InventoryEntry Entry;
				Entry.ItemId = kDefaultLootItemId;
				Entry.Quantity = Quantity;
				Container.Contents.push_back(std::move(Entry));
			}
			Containers.push_back(std::move(Container));
		};

		AddContainer(-780.0f, -660.0f, {1, 2, 1});
		AddContainer(760.0f, -640.0f, {1, 1});
		AddContainer(-820.0f, 700.0f, {2, 1});
		AddContainer(780.0f, 760.0f, {1, 3});
		AddContainer(0.0f, -260.0f, {1, 1, 2});
		AddContainer(0.0f, 320.0f, {2, 2});
	}

	void InitializeExtractionZones()
	{
		ExtractionZones.clear();

		auto AddZone = [this](const char* Name, const float X, const float Y) {
			ExtractionZoneState Zone;
			Zone.ZoneId = GenerateId("extract");
			Zone.DisplayName = Name;
			Zone.Position = {X, Y, 50.0f};
			Zone.Status = ExtractionZoneStatus::Inactive;
			ExtractionZones.push_back(std::move(Zone));
		};

		AddZone("North Gate", 0.0f, 1780.0f);
		AddZone("South Tunnel", 0.0f, -1780.0f);
		AddZone("West Checkpoint", -1820.0f, 0.0f);
	}

	void InitializeBots()
	{
		for (int Index = 0; Index < kDedicatedBotCount; ++Index)
		{
			const SpawnPoint& Spawn = kBotSpawnPoints[Index % static_cast<int>(std::size(kBotSpawnPoints))];
			Participant BotParticipant;
			BotParticipant.State.PlayerId = GenerateId("bot");
			BotParticipant.State.DisplayName = "Bandit " + std::to_string(Index + 1);
			BotParticipant.State.IsBot = true;
			BotParticipant.State.Position = {Spawn.X, Spawn.Y, Spawn.Z};
			BotParticipant.State.Look = {Spawn.Yaw, 0.0f};
			BotParticipant.Input.Yaw = Spawn.Yaw;
			BotParticipant.StrafingDirection = (Index % 2 == 0) ? 1.0f : -1.0f;
			Participants.emplace(BotParticipant.State.PlayerId, std::move(BotParticipant));
		}
	}

	static void AddInventoryEntryLocked(
		std::vector<InventoryEntry>& Inventory,
		const std::string& ItemId,
		const int Quantity)
	{
		if (ItemId.empty() || Quantity <= 0)
		{
			return;
		}

		for (InventoryEntry& Entry : Inventory)
		{
			if (Entry.ItemId == ItemId)
			{
				Entry.Quantity += Quantity;
				return;
			}
		}

		Inventory.push_back({ItemId, Quantity});
	}

	WorldContainerState* FindContainerByIdLocked(const std::string& ContainerId)
	{
		for (WorldContainerState& Container : Containers)
		{
			if (Container.ContainerId == ContainerId)
			{
				return &Container;
			}
		}
		return nullptr;
	}

	bool PlayerNearContainerLocked(const Participant& Player, const WorldContainerState& Container) const
	{
		const float DeltaX = Player.State.Position.X - Container.Position.X;
		const float DeltaY = Player.State.Position.Y - Container.Position.Y;
		const float DeltaZ = Player.State.Position.Z - Container.Position.Z;
		return (DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ) <= (kInteractRange * kInteractRange);
	}

	static int CountRemainingContainerItemsLocked(const WorldContainerState& Container)
	{
		int Count = 0;
		for (const InventoryEntry& Entry : Container.Contents)
		{
			if (!Entry.ItemId.empty() && Entry.Quantity > 0)
			{
				++Count;
			}
		}
		return Count;
	}

	int FindWorldLootIndexByIdLocked(const std::string& LootId) const
	{
		for (int Index = 0; Index < static_cast<int>(WorldLoot.size()); ++Index)
		{
			if (WorldLoot[Index].LootId == LootId)
			{
				return Index;
			}
		}
		return -1;
	}

	bool PlayerNearLootLocked(const Participant& Player, const WorldLootState& Loot) const
	{
		const float DeltaX = Player.State.Position.X - Loot.Position.X;
		const float DeltaY = Player.State.Position.Y - Loot.Position.Y;
		const float DeltaZ = Player.State.Position.Z - Loot.Position.Z;
		return (DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ) <= (kInteractRange * kInteractRange);
	}

	void CreateWorldLootLocked(const Vec3& Position, const InventoryEntry& Entry)
	{
		if (Entry.ItemId.empty() || Entry.Quantity <= 0)
		{
			return;
		}

		WorldLootState Loot;
		Loot.LootId = GenerateId("loot");
		Loot.Position = Position;
		Loot.Entry = Entry;
		WorldLoot.push_back(std::move(Loot));
	}

	void DropInventoryForParticipantLocked(Participant& ParticipantEntry)
	{
		if (ParticipantEntry.Inventory.empty())
		{
			return;
		}

		const Vec3 BasePosition = ParticipantEntry.State.Position;
		int OffsetIndex = 0;
		for (const InventoryEntry& Entry : ParticipantEntry.Inventory)
		{
			if (Entry.ItemId.empty() || Entry.Quantity <= 0)
			{
				continue;
			}

			const float Angle = static_cast<float>(OffsetIndex) * 1.25663706f;
			Vec3 DropPosition{
				BasePosition.X + std::cos(Angle) * 70.0f,
				BasePosition.Y + std::sin(Angle) * 70.0f,
				40.0f};
			CreateWorldLootLocked(DropPosition, Entry);
			++OffsetIndex;
		}
		ParticipantEntry.Inventory.clear();
	}

	int CalculateInventoryTotalValueLocked(const Participant& ParticipantEntry) const
	{
		int TotalValue = 0;
		for (const InventoryEntry& Entry : ParticipantEntry.Inventory)
		{
			if (!Entry.ItemId.empty() && Entry.Quantity > 0)
			{
				TotalValue += GetItemBaseValue(Entry.ItemId) * Entry.Quantity;
			}
		}
		return TotalValue;
	}

	int CalculateInventoryItemCountLocked(const Participant& ParticipantEntry) const
	{
		int TotalCount = 0;
		for (const InventoryEntry& Entry : ParticipantEntry.Inventory)
		{
			if (!Entry.ItemId.empty() && Entry.Quantity > 0)
			{
				TotalCount += Entry.Quantity;
			}
		}
		return TotalCount;
	}

	void UpdateExtractionAvailabilityLocked()
	{
		if (bExtractionZonesActivated || MatchElapsedSeconds < kExtractionActivationDelaySeconds)
		{
			return;
		}

		const std::size_t ActiveZoneCount = std::min<std::size_t>(2, ExtractionZones.size());
		for (std::size_t Index = 0; Index < ActiveZoneCount; ++Index)
		{
			ExtractionZones[Index].Status = ExtractionZoneStatus::Available;
			ExtractionZones[Index].Progress = 0.0f;
			ExtractionZones[Index].ExtractingPlayerId.clear();
		}

		bExtractionZonesActivated = true;
	}

	void UpdateExtractionZonesLocked(const float DeltaSeconds, std::vector<std::string>& Events)
	{
		for (ExtractionZoneState& Zone : ExtractionZones)
		{
			if (Zone.Status == ExtractionZoneStatus::Inactive || Zone.Status == ExtractionZoneStatus::Completed)
			{
				continue;
			}

			Participant* CapturingParticipant = nullptr;
			for (auto& Pair : Participants)
			{
				Participant& Entry = Pair.second;
				if (Entry.State.IsBot || !Entry.State.Alive || Entry.State.Extracted)
				{
					continue;
				}

				const float DeltaX = Entry.State.Position.X - Zone.Position.X;
				const float DeltaY = Entry.State.Position.Y - Zone.Position.Y;
				const float DeltaZ = Entry.State.Position.Z - Zone.Position.Z;
				if ((DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ) <= (Zone.Radius * Zone.Radius))
				{
					CapturingParticipant = &Entry;
					break;
				}
			}

			if (CapturingParticipant == nullptr)
			{
				if (Zone.Status == ExtractionZoneStatus::Extracting)
				{
					Zone.Status = ExtractionZoneStatus::Available;
					Zone.Progress = 0.0f;
					Zone.ExtractingPlayerId.clear();
				}
				continue;
			}

			if (Zone.Status != ExtractionZoneStatus::Extracting ||
				Zone.ExtractingPlayerId != CapturingParticipant->State.PlayerId)
			{
				Zone.Status = ExtractionZoneStatus::Extracting;
				Zone.Progress = 0.0f;
				Zone.ExtractingPlayerId = CapturingParticipant->State.PlayerId;
			}

			Zone.Progress = Clamp(Zone.Progress + (DeltaSeconds / kExtractionDurationSeconds), 0.0f, 1.0f);
			if (Zone.Progress >= 1.0f)
			{
				ExtractParticipantLocked(*CapturingParticipant, Zone, Events);
			}
		}
	}

	void UpdateBotsLocked(const std::uint64_t Now)
	{
		for (auto& Pair : Participants)
		{
			Participant& Bot = Pair.second;
			if (!Bot.State.IsBot)
			{
				continue;
			}

			Bot.PendingFire = false;
			Bot.Input.MoveX = 0.0f;
			Bot.Input.MoveY = 0.0f;
			Bot.Input.Sprint = false;
			Bot.Input.Crouch = false;

			if (!Bot.State.Alive || Bot.State.Extracted || bMatchExpired)
			{
				continue;
			}

			Participant* Target = nullptr;
			float BestDistanceSquared = 0.0f;
			for (auto& CandidatePair : Participants)
			{
				Participant& Candidate = CandidatePair.second;
				if (Candidate.State.IsBot || !Candidate.State.Alive || Candidate.State.Extracted)
				{
					continue;
				}

				const float DeltaX = Candidate.State.Position.X - Bot.State.Position.X;
				const float DeltaY = Candidate.State.Position.Y - Bot.State.Position.Y;
				const float DeltaZ = Candidate.State.Position.Z - Bot.State.Position.Z;
				const float DistanceSquared = DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ;
				if (Target == nullptr || DistanceSquared < BestDistanceSquared)
				{
					Target = &Candidate;
					BestDistanceSquared = DistanceSquared;
				}
			}

			if (Target == nullptr)
			{
				Bot.CurrentTargetId.clear();
				continue;
			}

			Bot.CurrentTargetId = Target->State.PlayerId;

			const float DeltaX = Target->State.Position.X - Bot.State.Position.X;
			const float DeltaY = Target->State.Position.Y - Bot.State.Position.Y;
			const float Distance = std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
			const float SafeDistance = std::max(Distance, 1.0f);
			const float DirectionX = DeltaX / SafeDistance;
			const float DirectionY = DeltaY / SafeDistance;
			const float PerpendicularX = -DirectionY * Bot.StrafingDirection;
			const float PerpendicularY = DirectionX * Bot.StrafingDirection;

			Bot.Input.Yaw = std::atan2(DirectionY, DirectionX) * 180.0f / 3.14159265f;
			Bot.Input.Pitch = 0.0f;

			float DesiredMoveX = PerpendicularX * kBotStrafeStrength;
			float DesiredMoveY = PerpendicularY * kBotStrafeStrength;
			if (Distance > kBotPreferredRange)
			{
				DesiredMoveX += DirectionX;
				DesiredMoveY += DirectionY;
			}
			else if (Distance < kBotRetreatRange)
			{
				DesiredMoveX -= DirectionX;
				DesiredMoveY -= DirectionY;
			}

			const float DesiredLength = std::sqrt(DesiredMoveX * DesiredMoveX + DesiredMoveY * DesiredMoveY);
			if (DesiredLength > 1.0f)
			{
				DesiredMoveX /= DesiredLength;
				DesiredMoveY /= DesiredLength;
			}

			Bot.Input.MoveX = DesiredMoveX;
			Bot.Input.MoveY = DesiredMoveY;
			Bot.Input.Sprint = Distance > kBotSprintDistance;
			Bot.Input.Sequence += 1;
			Bot.PendingFire = Distance <= kBotFireRange && (Now - Bot.LastFireAtMs) >= kFireCooldownMs;
		}
	}

	void ExtractParticipantLocked(
		Participant& ParticipantEntry,
		ExtractionZoneState& Zone,
		std::vector<std::string>& Events)
	{
		ParticipantEntry.State.Extracted = true;
		ParticipantEntry.State.Velocity = {};
		ParticipantEntry.State.ExtractedValue = CalculateInventoryTotalValueLocked(ParticipantEntry);

		const int ItemCount = CalculateInventoryItemCountLocked(ParticipantEntry);

		Zone.Status = ExtractionZoneStatus::Completed;
		Zone.Progress = 1.0f;
		Zone.ExtractingPlayerId = ParticipantEntry.State.PlayerId;

		std::ostringstream EventStream;
		EventStream << "{\"type\":\"player_extracted\",\"playerId\":\""
					<< EscapeJson(ParticipantEntry.State.PlayerId) << "\",\"zoneId\":\""
					<< EscapeJson(Zone.ZoneId) << "\",\"totalValue\":"
					<< ParticipantEntry.State.ExtractedValue << ",\"itemCount\":" << ItemCount << '}';
		Events.push_back(EventStream.str());
	}

	std::string BuildInventoryJsonLocked(const Participant& ParticipantEntry) const
	{
		std::ostringstream Stream;
		Stream << "{\"items\":[";

		bool First = true;
		for (const InventoryEntry& Entry : ParticipantEntry.Inventory)
		{
			if (Entry.ItemId.empty() || Entry.Quantity <= 0)
			{
				continue;
			}

			if (!First)
			{
				Stream << ',';
			}
			First = false;

			Stream << "{\"itemId\":\"" << EscapeJson(Entry.ItemId) << "\",\"quantity\":" << Entry.Quantity << "}";
		}

		Stream << "]}";
		return Stream.str();
	}

	std::string BuildContainerStateJsonLocked(const WorldContainerState& Container) const
	{
		std::ostringstream Stream;
		Stream << "{\"containerId\":\"" << EscapeJson(Container.ContainerId) << "\",\"position\":{\"x\":"
			   << std::fixed << std::setprecision(2) << Container.Position.X << ",\"y\":" << Container.Position.Y
			   << ",\"z\":" << Container.Position.Z << "},\"opened\":" << (Container.Opened ? "true" : "false")
			   << ",\"remainingItemCount\":" << CountRemainingContainerItemsLocked(Container) << ",\"contents\":[";

		bool First = true;
		for (const InventoryEntry& Entry : Container.Contents)
		{
			if (!First)
			{
				Stream << ',';
			}
			First = false;

			Stream << "{\"itemId\":\"" << EscapeJson(Entry.ItemId) << "\",\"quantity\":" << Entry.Quantity << "}";
		}

		Stream << "]}";
		return Stream.str();
	}

	void ApplyMovementLocked(Participant& Entry, const float DeltaSeconds)
	{
		PlayerRuntimeState& State = Entry.State;
		const PlayerInputState& Input = Entry.Input;

		State.Look.Yaw = NormalizeDegrees(Input.Yaw);
		State.Look.Pitch = Clamp(Input.Pitch, -89.0f, 89.0f);
		State.LastProcessedInputSequence = Input.Sequence;

		float MoveX = Clamp(Input.MoveX, -1.0f, 1.0f);
		float MoveY = Clamp(Input.MoveY, -1.0f, 1.0f);
		const float Length = std::sqrt(MoveX * MoveX + MoveY * MoveY);
		if (Length > 1.0f)
		{
			MoveX /= Length;
			MoveY /= Length;
		}

		float Speed = kBaseMoveSpeed;
		if (Input.Crouch)
		{
			Speed *= kCrouchMultiplier;
		}

		if (Input.Sprint && !Input.Crouch && State.Stamina > 0.0f)
		{
			Speed *= kSprintMultiplier;
			State.Stamina = std::max(0.0f, State.Stamina - kStaminaDrainPerSecond * DeltaSeconds);
		}
		else
		{
			State.Stamina = std::min(kMaxStamina, State.Stamina + kStaminaRegenPerSecond * DeltaSeconds);
		}

		State.Velocity.X = MoveX * Speed;
		State.Velocity.Y = MoveY * Speed;
		State.Velocity.Z = 0.0f;
		State.Position.X = Clamp(State.Position.X + State.Velocity.X * DeltaSeconds, -2500.0f, 2500.0f);
		State.Position.Y = Clamp(State.Position.Y + State.Velocity.Y * DeltaSeconds, -2500.0f, 2500.0f);
		State.Position.Z = 100.0f;
	}

	void ResolveShotLocked(Participant& Shooter, const std::uint64_t Now, std::vector<std::string>& Events)
	{
		Events.push_back("{\"type\":\"player_fired\",\"playerId\":\"" + EscapeJson(Shooter.State.PlayerId) + "\"}");

		const float Radians = Shooter.State.Look.Yaw * 3.14159265f / 180.0f;
		const float DirX = std::cos(Radians);
		const float DirY = std::sin(Radians);

		Participant* BestTarget = nullptr;
		float BestDistance = kShotRange + 1.0f;

		for (auto& Pair : Participants)
		{
			Participant& Target = Pair.second;
			if (Target.State.PlayerId == Shooter.State.PlayerId || !Target.State.Alive || Target.State.Extracted)
			{
				continue;
			}
			if (Shooter.State.IsBot && Target.State.IsBot)
			{
				continue;
			}

			const float OffsetX = Target.State.Position.X - Shooter.State.Position.X;
			const float OffsetY = Target.State.Position.Y - Shooter.State.Position.Y;
			const float ProjectedDistance = OffsetX * DirX + OffsetY * DirY;
			if (ProjectedDistance <= 0.0f || ProjectedDistance > kShotRange)
			{
				continue;
			}

			const float PerpendicularDistance = std::fabs(OffsetX * DirY - OffsetY * DirX);
			if (PerpendicularDistance > kShotHitRadius)
			{
				continue;
			}

			if (ProjectedDistance < BestDistance)
			{
				BestDistance = ProjectedDistance;
				BestTarget = &Target;
			}
		}

		if (BestTarget == nullptr)
		{
			Events.push_back("{\"type\":\"player_shot_missed\",\"playerId\":\"" +
							  EscapeJson(Shooter.State.PlayerId) + "\"}");
			return;
		}

		const float ArmorAbsorb = std::min(BestTarget->State.Armor, kShotDamage * kArmorAbsorbFraction);
		const float HealthDamage = kShotDamage - ArmorAbsorb;
		BestTarget->State.Armor = std::max(0.0f, BestTarget->State.Armor - ArmorAbsorb);
		BestTarget->State.Health = std::max(0.0f, BestTarget->State.Health - HealthDamage);

		std::ostringstream HitEvent;
		HitEvent << "{\"type\":\"player_hit\",\"sourcePlayerId\":\"" << EscapeJson(Shooter.State.PlayerId)
				 << "\",\"targetPlayerId\":\"" << EscapeJson(BestTarget->State.PlayerId)
				 << "\",\"health\":" << std::fixed << std::setprecision(2) << BestTarget->State.Health
				 << ",\"armor\":" << BestTarget->State.Armor << "}";
		Events.push_back(HitEvent.str());

		if (BestTarget->State.Health <= 0.0f)
		{
			BestTarget->State.Alive = false;
			BestTarget->State.Deaths += 1;
			BestTarget->State.RespawnAtMs = Now + kRespawnDelayMs;
			DropInventoryForParticipantLocked(*BestTarget);
			Shooter.State.Kills += 1;
			Events.push_back("{\"type\":\"player_killed\",\"sourcePlayerId\":\"" +
							  EscapeJson(Shooter.State.PlayerId) + "\",\"targetPlayerId\":\"" +
							  EscapeJson(BestTarget->State.PlayerId) + "\"}");
		}
	}

	void RespawnParticipantLocked(Participant& Entry)
	{
		const SpawnPoint& Spawn = kSpawnPoints[(TickCounter + static_cast<std::uint64_t>(Entry.State.Deaths)) %
											   std::size(kSpawnPoints)];
		Entry.State.Position = {Spawn.X, Spawn.Y, Spawn.Z};
		Entry.State.Velocity = {};
		Entry.State.Look = {Spawn.Yaw, 0.0f};
		Entry.State.Health = kMaxHealth;
		Entry.State.Armor = kMaxArmor;
		Entry.State.Stamina = kMaxStamina;
		Entry.State.Alive = true;
		Entry.State.Extracted = false;
		Entry.State.ExtractedValue = 0;
		Entry.State.RespawnAtMs = 0;
	}

	std::string BuildSnapshotJsonLocked(const std::uint64_t Now, const std::vector<std::string>& Events) const
	{
		std::ostringstream Stream;
		Stream << "{\"matchId\":\"" << EscapeJson(MatchId) << "\",\"tick\":" << TickCounter
			   << ",\"serverTime\":" << Now << ",\"mapName\":\"" << kMapName
			   << "\",\"matchRemainingSeconds\":"
			   << std::fixed << std::setprecision(2)
			   << std::max(0.0f, kMatchDurationSeconds - MatchElapsedSeconds)
			   << ",\"matchExpired\":" << (bMatchExpired ? "true" : "false") << ",\"players\":[";

		bool First = true;
		for (const auto& Pair : Participants)
		{
			const PlayerRuntimeState& State = Pair.second.State;
			if (!First)
			{
				Stream << ',';
			}
			First = false;

			Stream << "{\"playerId\":\"" << EscapeJson(State.PlayerId) << "\",\"displayName\":\""
				   << EscapeJson(State.DisplayName) << "\",\"position\":{\"x\":" << std::fixed
				   << std::setprecision(2) << State.Position.X << ",\"y\":" << State.Position.Y
				   << ",\"z\":" << State.Position.Z << "},\"rotation\":{\"yaw\":" << State.Look.Yaw
				   << ",\"pitch\":" << State.Look.Pitch << "},\"velocity\":{\"x\":" << State.Velocity.X
				   << ",\"y\":" << State.Velocity.Y << ",\"z\":" << State.Velocity.Z
				   << "},\"health\":" << State.Health << ",\"armor\":" << State.Armor
				   << ",\"stamina\":" << State.Stamina << ",\"alive\":"
				   << (State.Alive ? "true" : "false") << ",\"kills\":" << State.Kills
				   << ",\"deaths\":" << State.Deaths << ",\"isBot\":"
				   << (State.IsBot ? "true" : "false") << ",\"extracted\":"
				   << (State.Extracted ? "true" : "false")
				   << ",\"extractedValue\":" << State.ExtractedValue
				   << ",\"lastProcessedInputSequence\":" << State.LastProcessedInputSequence << "}";
		}

		Stream << "],\"containers\":[";
		for (std::size_t Index = 0; Index < Containers.size(); ++Index)
		{
			if (Index > 0)
			{
				Stream << ',';
			}

			const WorldContainerState& Container = Containers[Index];
			Stream << "{\"containerId\":\"" << EscapeJson(Container.ContainerId) << "\",\"position\":{\"x\":"
				   << std::fixed << std::setprecision(2) << Container.Position.X << ",\"y\":" << Container.Position.Y
				   << ",\"z\":" << Container.Position.Z << "},\"opened\":"
				   << (Container.Opened ? "true" : "false") << ",\"remainingItemCount\":"
				   << CountRemainingContainerItemsLocked(Container) << "}";
		}

		Stream << "],\"worldLoot\":[";
		for (std::size_t Index = 0; Index < WorldLoot.size(); ++Index)
		{
			if (Index > 0)
			{
				Stream << ',';
			}

			const WorldLootState& Loot = WorldLoot[Index];
			Stream << "{\"lootId\":\"" << EscapeJson(Loot.LootId) << "\",\"position\":{\"x\":"
				   << std::fixed << std::setprecision(2) << Loot.Position.X << ",\"y\":" << Loot.Position.Y
				   << ",\"z\":" << Loot.Position.Z << "},\"itemId\":\"" << EscapeJson(Loot.Entry.ItemId)
				   << "\",\"quantity\":" << Loot.Entry.Quantity << "}";
		}

		Stream << "],\"extractionZones\":[";
		for (std::size_t Index = 0; Index < ExtractionZones.size(); ++Index)
		{
			if (Index > 0)
			{
				Stream << ',';
			}

			const ExtractionZoneState& Zone = ExtractionZones[Index];
			Stream << "{\"zoneId\":\"" << EscapeJson(Zone.ZoneId) << "\",\"displayName\":\""
				   << EscapeJson(Zone.DisplayName) << "\",\"position\":{\"x\":" << std::fixed
				   << std::setprecision(2) << Zone.Position.X << ",\"y\":" << Zone.Position.Y
				   << ",\"z\":" << Zone.Position.Z << "},\"radius\":" << Zone.Radius
				   << ",\"status\":\"" << ToString(Zone.Status) << "\",\"progress\":"
				   << Zone.Progress << ",\"extractingPlayerId\":\""
				   << EscapeJson(Zone.ExtractingPlayerId) << "\"}";
		}

		Stream << "],\"events\":[";
		for (std::size_t Index = 0; Index < Events.size(); ++Index)
		{
			if (Index > 0)
			{
				Stream << ',';
			}
			Stream << Events[Index];
		}
		Stream << "]}";
		return Stream.str();
	}

	mutable std::mutex Mutex;
	std::string MatchId;
	std::unordered_map<std::string, Participant> Participants;
	std::vector<WorldContainerState> Containers;
	std::vector<WorldLootState> WorldLoot;
	std::vector<ExtractionZoneState> ExtractionZones;
	std::vector<std::string> RecentEvents;
	std::uint64_t TickCounter = 0;
	float MatchElapsedSeconds = 0.0f;
	bool bExtractionZonesActivated = false;
	bool bMatchExpired = false;
};

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
			Stream << '"' << EscapeJson(MatchDescriptions[Index]) << '"';
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

void ClientSession::Start()
{
	ReceiveThread = std::thread(&ClientSession::ReceiveLoop, this);
}

void ClientSession::Close()
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

void ClientSession::SendLine(const std::string& Line)
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

void ClientSession::ReceiveLoop()
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

int main(int argc, char* argv[])
{
	WSADATA WsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
	{
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}

	std::uint16_t Port = demo::kDefaultPort;
	if (argc > 1)
	{
		Port = static_cast<std::uint16_t>(std::strtoul(argv[1], nullptr, 10));
	}

	auto Server = std::make_shared<demo::DedicatedServer>();
	if (!Server->Start(Port))
	{
		WSACleanup();
		return 1;
	}

	std::cout << "Commands: status | quit\n";
	std::string Line;
	while (std::getline(std::cin, Line))
	{
		const std::string Command = demo::ToUpperCopy(demo::Trim(Line));
		if (Command == "QUIT" || Command == "EXIT")
		{
			break;
		}
		if (Command == "STATUS")
		{
			Server->PrintStatus();
		}
	}

	Server->Stop();
	WSACleanup();
	return 0;
}
#endif

#include "server/DedicatedServer.h"

int main(int argc, char* argv[])
{
	WSADATA WsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
	{
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}

	std::uint16_t Port = demo::kDefaultPort;
	if (argc > 1)
	{
		Port = static_cast<std::uint16_t>(std::strtoul(argv[1], nullptr, 10));
	}

	auto Server = std::make_shared<demo::DedicatedServer>();
	if (!Server->Start(Port))
	{
		WSACleanup();
		return 1;
	}

	std::cout << "Commands: status | quit\n";
	std::string Line;
	while (std::getline(std::cin, Line))
	{
		const std::string Command = demo::ToUpperCopy(demo::Trim(Line));
		if (Command == "QUIT" || Command == "EXIT")
		{
			break;
		}
		if (Command == "STATUS")
		{
			Server->PrintStatus();
		}
	}

	Server->Stop();
	WSACleanup();
	return 0;
}
