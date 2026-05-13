#pragma once

#include "server/ClientSession.h"

namespace demo
{
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

	void QueueReload(const std::string& PlayerId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto It = Participants.find(PlayerId);
		if (It != Participants.end())
		{
			It->second.PendingReload = true;
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
		const int AmmoAmount = GetItemAmmoAmount(Entry.ItemId);
		if (HealAmount <= 0.0f && ArmorAmount <= 0.0f && AmmoAmount <= 0)
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
		if (AmmoAmount > 0)
		{
			PlayerIt->second.State.ReserveAmmo =
				std::min(kMaxReserveAmmo, PlayerIt->second.State.ReserveAmmo + AmmoAmount);
		}
		Entry.Quantity -= 1;
		if (Entry.Quantity <= 0)
		{
			PlayerIt->second.Inventory.erase(PlayerIt->second.Inventory.begin() + SlotIndex);
		}

		RecentEvents.push_back("{\"type\":\"item_used\",\"playerId\":\"" + EscapeJson(PlayerId) +
							  "\",\"itemId\":\"" + EscapeJson(ItemId) + "\",\"heal\":" +
							  std::to_string(static_cast<int>(HealAmount)) + ",\"armor\":" +
							  std::to_string(static_cast<int>(ArmorAmount)) + ",\"ammo\":" +
							  std::to_string(AmmoAmount) + "}");
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

			UpdateReloadsLocked(Now, Events);

			if (!bMatchExpired)
			{
				for (auto& Pair : Participants)
				{
					Participant& Entry = Pair.second;
					if (!Entry.PendingReload || !Entry.State.Alive || Entry.State.Extracted)
					{
						Entry.PendingReload = false;
						continue;
					}

					Entry.PendingReload = false;
					StartReloadLocked(Entry, Now, Events);
				}
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
					if (Entry.State.Reloading)
					{
						continue;
					}
					if (Entry.State.CurrentAmmo <= 0)
					{
						StartReloadLocked(Entry, Now, Events);
						continue;
					}
					const std::uint64_t FireCooldownMs = Entry.State.IsBot ? kBotFireCooldownMs : kPlayerFireCooldownMs;
					if (Now - Entry.LastFireAtMs < FireCooldownMs)
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
		bool PendingReload = false;
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

		auto AddContainer = [this](const float X, const float Y, std::initializer_list<InventoryEntry> Entries) {
			WorldContainerState Container;
			Container.ContainerId = GenerateId("container");
			Container.Position = {X, Y, 50.0f};
			for (const InventoryEntry& Entry : Entries)
			{
				Container.Contents.push_back(std::move(Entry));
			}
			Containers.push_back(std::move(Container));
		};

		AddContainer(-780.0f, -660.0f, {{"Item_DemoBandage", 1}, {"Item_DemoAmmo545", 1}, {kDefaultLootItemId, 1}});
		AddContainer(760.0f, -640.0f, {{"Item_DemoArmorPlate", 1}, {kDefaultLootItemId, 1}});
		AddContainer(-820.0f, 700.0f, {{"Item_DemoMedkit", 1}, {"Item_DemoAmmo545", 1}});
		AddContainer(780.0f, 760.0f, {{"Item_DemoArmorPlate", 1}, {kDefaultLootItemId, 2}});
		AddContainer(0.0f, -260.0f, {{"Item_DemoAmmoBox", 1}, {"Item_DemoBandage", 1}, {kDefaultLootItemId, 1}});
		AddContainer(0.0f, 320.0f, {{"Item_DemoArmorPlate", 1}, {"Item_DemoMedkit", 1}});
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

	bool StartReloadLocked(Participant& Entry, const std::uint64_t Now, std::vector<std::string>& Events)
	{
		if (Entry.State.Reloading ||
			!Entry.State.Alive ||
			Entry.State.Extracted ||
			Entry.State.CurrentAmmo >= kMagazineCapacity ||
			Entry.State.ReserveAmmo <= 0)
		{
			return false;
		}

		Entry.State.Reloading = true;
		Entry.State.ReloadCompleteAtMs = Now + kReloadDurationMs;
		Entry.PendingFire = false;

		std::ostringstream EventStream;
		EventStream << "{\"type\":\"weapon_reload_started\",\"playerId\":\""
					<< EscapeJson(Entry.State.PlayerId) << "\",\"currentAmmo\":" << Entry.State.CurrentAmmo
					<< ",\"reserveAmmo\":" << Entry.State.ReserveAmmo << '}';
		Events.push_back(EventStream.str());
		return true;
	}

	void CompleteReloadLocked(Participant& Entry, std::vector<std::string>& Events)
	{
		if (!Entry.State.Reloading)
		{
			return;
		}

		const int NeededAmmo = std::max(0, kMagazineCapacity - Entry.State.CurrentAmmo);
		const int LoadedAmmo = std::min(NeededAmmo, Entry.State.ReserveAmmo);
		Entry.State.CurrentAmmo += LoadedAmmo;
		Entry.State.ReserveAmmo -= LoadedAmmo;
		Entry.State.Reloading = false;
		Entry.State.ReloadCompleteAtMs = 0;

		std::ostringstream EventStream;
		EventStream << "{\"type\":\"weapon_reloaded\",\"playerId\":\"" << EscapeJson(Entry.State.PlayerId)
					<< "\",\"currentAmmo\":" << Entry.State.CurrentAmmo << ",\"reserveAmmo\":"
					<< Entry.State.ReserveAmmo << '}';
		Events.push_back(EventStream.str());
	}

	void UpdateReloadsLocked(const std::uint64_t Now, std::vector<std::string>& Events)
	{
		for (auto& Pair : Participants)
		{
			Participant& Entry = Pair.second;
			if (!Entry.State.Reloading)
			{
				continue;
			}

			if (!Entry.State.Alive || Entry.State.Extracted)
			{
				Entry.State.Reloading = false;
				Entry.State.ReloadCompleteAtMs = 0;
				continue;
			}

			if (Entry.State.ReloadCompleteAtMs > 0 && Now >= Entry.State.ReloadCompleteAtMs)
			{
				CompleteReloadLocked(Entry, Events);
			}
		}
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
			Bot.PendingReload = false;
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
			if (Bot.State.CurrentAmmo <= 0 && Bot.State.ReserveAmmo > 0)
			{
				Bot.PendingReload = true;
			}
			else if (!Bot.State.Reloading)
			{
				Bot.PendingFire = Distance <= kBotFireRange && (Now - Bot.LastFireAtMs) >= kBotFireCooldownMs;
			}
		}
	}

	void ExtractParticipantLocked(
		Participant& ParticipantEntry,
		ExtractionZoneState& Zone,
		std::vector<std::string>& Events)
	{
		ParticipantEntry.State.Extracted = true;
		ParticipantEntry.State.Velocity = {};
		ParticipantEntry.State.Reloading = false;
		ParticipantEntry.State.ReloadCompleteAtMs = 0;
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
		State.Position.X = Clamp(State.Position.X + State.Velocity.X * DeltaSeconds, kWorldMinX, kWorldMaxX);
		State.Position.Y = Clamp(State.Position.Y + State.Velocity.Y * DeltaSeconds, kWorldMinY, kWorldMaxY);
	}

	void ResolveShotLocked(Participant& Shooter, const std::uint64_t Now, std::vector<std::string>& Events)
	{
		if (Shooter.State.CurrentAmmo <= 0)
		{
			Events.push_back("{\"type\":\"weapon_dry_fire\",\"playerId\":\"" + EscapeJson(Shooter.State.PlayerId) + "\"}");
			return;
		}

		Shooter.State.CurrentAmmo = std::max(0, Shooter.State.CurrentAmmo - 1);
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

		const float ShotDamage = Shooter.State.IsBot ? kBotShotDamage : kPlayerShotDamage;
		const float ArmorAbsorb = std::min(BestTarget->State.Armor, ShotDamage * kArmorAbsorbFraction);
		const float HealthDamage = ShotDamage - ArmorAbsorb;
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
			BestTarget->State.Reloading = false;
			BestTarget->State.ReloadCompleteAtMs = 0;
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
		Entry.State.CurrentAmmo = kMagazineCapacity;
		Entry.State.ReserveAmmo = kDefaultReserveAmmo;
		Entry.State.Reloading = false;
		Entry.State.Alive = true;
		Entry.State.Extracted = false;
		Entry.State.ExtractedValue = 0;
		Entry.State.RespawnAtMs = 0;
		Entry.State.ReloadCompleteAtMs = 0;
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
				   << ",\"stamina\":" << State.Stamina << ",\"currentAmmo\":" << State.CurrentAmmo
				   << ",\"reserveAmmo\":" << State.ReserveAmmo << ",\"reloading\":"
				   << (State.Reloading ? "true" : "false") << ",\"alive\":"
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

} // namespace demo
