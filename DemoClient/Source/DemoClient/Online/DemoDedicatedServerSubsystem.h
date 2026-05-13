#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/DemoTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "DemoDedicatedServerSubsystem.generated.h"

class FSocket;

UENUM(BlueprintType)
enum class EDemoDedicatedServerConnectionState : uint8
{
	Disconnected,
	Connected,
	Authenticated,
	InMatch
};

USTRUCT(BlueprintType)
struct FDemoDedicatedServerPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Armor = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Stamina = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 CurrentAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	bool bReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	bool bAlive = false;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	bool bIsBot = false;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	bool bExtracted = false;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 ExtractedValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 LastProcessedInputSequence = 0;
};

USTRUCT(BlueprintType)
struct FDemoDedicatedServerSnapshotEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString SourcePlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString TargetPlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Armor = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 Ammo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString ZoneId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 TotalValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 ItemCount = 0;
};

USTRUCT(BlueprintType)
struct FDemoDedicatedServerContainerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString ContainerId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	bool bOpened = false;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 RemainingItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	TArray<FDemoInventorySlot> Contents;
};

USTRUCT(BlueprintType)
struct FDemoDedicatedServerWorldLootSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString LootId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct FDemoDedicatedServerExtractionZoneSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString ZoneId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Radius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	EDemoExtractionState State = EDemoExtractionState::Inactive;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString ExtractingPlayerId;
};

USTRUCT(BlueprintType)
struct FDemoDedicatedServerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString MatchId;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int32 Tick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	int64 ServerTime = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	float MatchRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	bool bMatchExpired = false;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	TArray<FDemoDedicatedServerPlayerSnapshot> Players;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	TArray<FDemoDedicatedServerSnapshotEvent> Events;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	TArray<FDemoDedicatedServerContainerSnapshot> Containers;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	TArray<FDemoDedicatedServerWorldLootSnapshot> WorldLoot;

	UPROPERTY(BlueprintReadOnly, Category = "DedicatedServer")
	TArray<FDemoDedicatedServerExtractionZoneSnapshot> ExtractionZones;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDemoDedicatedServerMessageSignature, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDemoDedicatedServerAuthSignature, const FString&, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDemoDedicatedServerMatchSignature, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDemoDedicatedServerSnapshotSignature, const FDemoDedicatedServerSnapshot&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDemoDedicatedServerInventorySignature, const TArray<FDemoInventorySlot>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDemoDedicatedServerContainerSignature, const FDemoDedicatedServerContainerSnapshot&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDemoDedicatedServerErrorSignature, const FString&, const FString&);

UCLASS()
class DEMOCLIENT_API UDemoDedicatedServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool Connect(const FString& InHost, int32 InPort, FString* OutError = nullptr);
	void Disconnect(const FString& Reason = TEXT("ManualDisconnect"));

	bool Login(const FString& DisplayName, FString* OutError = nullptr);
	bool JoinMatch(const FString& PreferredMatchId = TEXT(""), FString* OutError = nullptr);
	bool LeaveMatch(FString* OutError = nullptr);
	bool InteractContainer(const FString& ContainerId, FString* OutError = nullptr);
	bool TakeContainerItem(const FString& ContainerId, int32 SlotIndex, FString* OutError = nullptr);
	bool PickupLoot(const FString& LootId, FString* OutError = nullptr);
	bool DropItem(int32 SlotIndex, FString* OutError = nullptr);
	bool UseItem(int32 SlotIndex, FString* OutError = nullptr);
	bool SendInput(
		int32 Sequence,
		const FVector2D& MoveInput,
		const FRotator& ControlRotation,
		bool bSprinting,
		bool bCrouching,
		FString* OutError = nullptr);
	bool SendFire(FString* OutError = nullptr);
	bool SendReload(FString* OutError = nullptr);

	bool IsConnected() const;
	bool IsAuthenticated() const { return bAuthenticated; }
	bool IsInMatch() const { return !CurrentMatchId.IsEmpty(); }
	EDemoDedicatedServerConnectionState GetConnectionState() const;

	const FString& GetConnectionId() const { return ConnectionId; }
	const FString& GetPlayerId() const { return PlayerId; }
	const FString& GetCurrentMatchId() const { return CurrentMatchId; }
	const FString& GetCurrentMapName() const { return CurrentMapName; }
	const FString& GetLastStatusMessage() const { return LastStatusMessage; }
	const FDemoDedicatedServerSnapshot& GetLastSnapshot() const { return LastSnapshot; }
	const TArray<FDemoInventorySlot>& GetInventoryItems() const { return InventoryItems; }
	const TMap<FString, FDemoDedicatedServerContainerSnapshot>& GetContainerSnapshots() const { return ContainerSnapshots; }
	const FDemoDedicatedServerContainerSnapshot* FindContainerSnapshot(const FString& ContainerId) const;
	const FDemoDedicatedServerPlayerSnapshot* FindPlayerSnapshot(const FString& InPlayerId) const;
	const FDemoDedicatedServerPlayerSnapshot* GetLocalPlayerSnapshot() const;

	FDemoDedicatedServerMessageSignature OnStatusMessage;
	FDemoDedicatedServerAuthSignature OnAuthSucceeded;
	FDemoDedicatedServerMatchSignature OnMatchJoined;
	FDemoDedicatedServerSnapshotSignature OnSnapshotReceived;
	FDemoDedicatedServerInventorySignature OnInventoryUpdated;
	FDemoDedicatedServerContainerSignature OnContainerUpdated;
	FDemoDedicatedServerErrorSignature OnServerError;

private:
	bool Tick(float DeltaTime);
	void PumpSocket();
	bool SendCommand(const FString& Command, FString* OutError = nullptr);
	void ProcessLine(const FString& Line);
	void SetStatusMessage(const FString& Message);
	void BroadcastError(const FString& Code, const FString& Message);
	void ResetSessionState();

	bool ParseSnapshotJson(const FString& JsonPayload, FDemoDedicatedServerSnapshot& OutSnapshot) const;
	bool ParseInventoryJson(const FString& JsonPayload, TArray<FDemoInventorySlot>& OutItems) const;
	bool ParseContainerJson(const FString& JsonPayload, FDemoDedicatedServerContainerSnapshot& OutContainer) const;
	static bool TryReadVector(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FVector& OutVector);
	static bool TryReadRotator(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FRotator& OutRotator);
	static EDemoExtractionState ParseExtractionState(const FString& RawState);
	static void ParseInventorySlotArray(const TArray<TSharedPtr<FJsonValue>>& JsonValues, TArray<FDemoInventorySlot>& OutItems);

	FSocket* Socket = nullptr;
	FTSTicker::FDelegateHandle TickHandle;

	FString Host;
	int32 Port = 0;
	FString ReceiveBuffer;
	FString ConnectionId;
	FString PlayerId;
	FString CurrentMatchId;
	FString CurrentMapName;
	FString LastStatusMessage;
	bool bAuthenticated = false;
	FDemoDedicatedServerSnapshot LastSnapshot;
	TArray<FDemoInventorySlot> InventoryItems;
	TMap<FString, FDemoDedicatedServerContainerSnapshot> ContainerSnapshots;
};
