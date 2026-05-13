#pragma once

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
inline constexpr std::uint16_t kDefaultPort = 7777;
inline constexpr int kTickRate = 20;
inline constexpr int kMaxPlayersPerMatch = 8;
inline constexpr std::uint64_t kPlayerFireCooldownMs = 120;
inline constexpr std::uint64_t kBotFireCooldownMs = 1400;
inline constexpr std::uint64_t kRespawnDelayMs = 4000;
inline constexpr float kMaxHealth = 100.0f;
inline constexpr float kMaxArmor = 50.0f;
inline constexpr float kMaxStamina = 100.0f;
inline constexpr float kBaseMoveSpeed = 650.0f;
inline constexpr float kSprintMultiplier = 1.35f;
inline constexpr float kCrouchMultiplier = 0.55f;
inline constexpr float kStaminaDrainPerSecond = 22.0f;
inline constexpr float kStaminaRegenPerSecond = 16.0f;
inline constexpr float kShotRange = 1800.0f;
inline constexpr float kShotHitRadius = 70.0f;
inline constexpr float kPlayerShotDamage = 34.0f;
inline constexpr float kBotShotDamage = 8.0f;
inline constexpr float kArmorAbsorbFraction = 0.5f;
inline constexpr int kMagazineCapacity = 30;
inline constexpr int kDefaultReserveAmmo = 120;
inline constexpr int kMaxReserveAmmo = 180;
inline constexpr std::uint64_t kReloadDurationMs = 1800;
inline constexpr float kInteractRange = 260.0f;
inline constexpr float kWorldMinX = -6000.0f;
inline constexpr float kWorldMaxX = 6000.0f;
inline constexpr float kWorldMinY = -6000.0f;
inline constexpr float kWorldMaxY = 6000.0f;
inline constexpr float kMatchDurationSeconds = 900.0f;
inline constexpr float kExtractionActivationDelaySeconds = 20.0f;
inline constexpr float kExtractionZoneRadius = 280.0f;
inline constexpr float kExtractionDurationSeconds = 5.0f;
inline constexpr int kDedicatedBotCount = 5;
inline constexpr float kBotPreferredRange = 900.0f;
inline constexpr float kBotRetreatRange = 375.0f;
inline constexpr float kBotFireRange = 1300.0f;
inline constexpr float kBotSprintDistance = 1100.0f;
inline constexpr float kBotStrafeStrength = 0.35f;
inline constexpr const char* kMapName = "/Game/UtopianCity/Maps/UtopianCityDemoMap";
inline constexpr const char* kDefaultLootItemId = "Item_DemoValuable";
inline constexpr int kDefaultLootItemValue = 1000;

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
	int CurrentAmmo = kMagazineCapacity;
	int ReserveAmmo = kDefaultReserveAmmo;
	bool Reloading = false;
	std::uint32_t LastProcessedInputSequence = 0;
	std::uint64_t RespawnAtMs = 0;
	std::uint64_t ReloadCompleteAtMs = 0;
};

struct SpawnPoint
{
	float X;
	float Y;
	float Z;
	float Yaw;
};

inline constexpr SpawnPoint kSpawnPoints[] = {
	{900.0f, 4120.0f, 2672.0f, -90.0f},
	{980.0f, 4060.0f, 2672.0f, -95.0f},
	{820.0f, 4060.0f, 2672.0f, -85.0f},
	{1060.0f, 4120.0f, 2672.0f, -100.0f},
	{740.0f, 4120.0f, 2672.0f, -80.0f},
	{980.0f, 4180.0f, 2672.0f, -95.0f},
	{820.0f, 4180.0f, 2672.0f, -85.0f},
	{900.0f, 4240.0f, 2672.0f, -90.0f},
};

inline constexpr SpawnPoint kBotSpawnPoints[] = {
	{900.0f, -2600.0f, 2672.0f, 90.0f},
	{980.0f, -2660.0f, 2672.0f, 85.0f},
	{820.0f, -2660.0f, 2672.0f, 95.0f},
	{1060.0f, -2540.0f, 2672.0f, 80.0f},
	{740.0f, -2540.0f, 2672.0f, 100.0f},
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
inline T Clamp(const T Value, const T MinValue, const T MaxValue)
{
	return std::max(MinValue, std::min(MaxValue, Value));
}

inline std::uint64_t NowMs()
{
	return static_cast<std::uint64_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count());
}

inline float NormalizeDegrees(float Value)
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

inline std::string Trim(const std::string& Value)
{
	const std::size_t First = Value.find_first_not_of(" \t\r\n");
	if (First == std::string::npos)
	{
		return {};
	}
	const std::size_t Last = Value.find_last_not_of(" \t\r\n");
	return Value.substr(First, Last - First + 1);
}

inline std::string ToUpperCopy(std::string Value)
{
	std::transform(Value.begin(), Value.end(), Value.begin(), [](const unsigned char Character) {
		return static_cast<char>(std::toupper(Character));
	});
	return Value;
}

inline std::string EscapeJson(const std::string& Value)
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

inline std::string GenerateId(const char* Prefix)
{
	static std::atomic<std::uint64_t> Counter{1};
	std::ostringstream Stream;
	Stream << Prefix << '_' << Counter.fetch_add(1, std::memory_order_relaxed);
	return Stream.str();
}

inline std::string SanitizeDisplayName(const std::string& Raw)
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

inline const char* ToString(const ExtractionZoneStatus Status)
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

inline int GetItemBaseValue(const std::string& ItemId)
{
	const std::string UpperItemId = ToUpperCopy(ItemId);
	if (UpperItemId.find("MEDKIT") != std::string::npos)
	{
		return 350;
	}
	if (UpperItemId.find("BANDAGE") != std::string::npos ||
		UpperItemId.find("MED") != std::string::npos ||
		UpperItemId.find("STIM") != std::string::npos ||
		UpperItemId.find("PAIN") != std::string::npos)
	{
		return 180;
	}
	if (UpperItemId.find("PLATE") != std::string::npos ||
		UpperItemId.find("ARMOR") != std::string::npos ||
		UpperItemId.find("VEST") != std::string::npos)
	{
		return 240;
	}
	if (UpperItemId.find("AMMO") != std::string::npos ||
		UpperItemId.find("545") != std::string::npos ||
		UpperItemId.find("556") != std::string::npos ||
		UpperItemId.find("762") != std::string::npos)
	{
		return 140;
	}
	if (ItemId == kDefaultLootItemId)
	{
		return kDefaultLootItemValue;
	}
	return 100;
}

inline float GetItemHealAmount(const std::string& ItemId)
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

inline float GetItemArmorAmount(const std::string& ItemId)
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

inline int GetItemAmmoAmount(const std::string& ItemId)
{
	const std::string UpperItemId = ToUpperCopy(ItemId);
	if (UpperItemId.find("BOX") != std::string::npos || UpperItemId.find("CRATE") != std::string::npos)
	{
		return 60;
	}
	if (UpperItemId.find("AMMO") != std::string::npos ||
		UpperItemId.find("545") != std::string::npos ||
		UpperItemId.find("556") != std::string::npos ||
		UpperItemId.find("762") != std::string::npos ||
		UpperItemId.find("RND") != std::string::npos)
	{
		return 30;
	}
	return 0;
}

class DedicatedServer;

} // namespace demo
