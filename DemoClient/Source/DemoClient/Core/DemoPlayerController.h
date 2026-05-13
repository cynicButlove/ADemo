#pragma once

#include "CoreMinimal.h"
#include "Core/DemoTypes.h"
#include "GameFramework/PlayerController.h"
#include "DemoPlayerController.generated.h"

class ADemoDedicatedServerProxyActor;
class ADemoDedicatedServerContainerActor;
class ADemoDedicatedServerLootActor;
struct FDemoDedicatedServerSnapshot;

UCLASS()
class DEMOCLIENT_API ADemoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADemoPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ShouldUseDedicatedServerPath() const;
	void BindDedicatedServerDelegates();
	void HandleDedicatedServerSnapshot(const FDemoDedicatedServerSnapshot& Snapshot);
	void HandleDedicatedServerInventoryUpdated(const TArray<FDemoInventorySlot>& InventoryItems);
	void ApplySnapshotToLocalPawn(const FDemoDedicatedServerSnapshot& Snapshot);
	void SyncRemoteProxyActors(const FDemoDedicatedServerSnapshot& Snapshot);
	void SyncDedicatedServerContainers(const FDemoDedicatedServerSnapshot& Snapshot);
	void SyncDedicatedServerWorldLoot(const FDemoDedicatedServerSnapshot& Snapshot);
	void SyncDedicatedInventoryToPawn();
	void RemoveAllDedicatedServerProxies();
	void RemoveAllDedicatedServerContainers();
	void RemoveAllDedicatedServerWorldLoot();

	bool bDedicatedServerDelegatesBound = false;

	UPROPERTY()
	TMap<FString, TObjectPtr<ADemoDedicatedServerProxyActor>> DedicatedServerProxyActors;

	UPROPERTY()
	TMap<FString, TObjectPtr<ADemoDedicatedServerContainerActor>> DedicatedServerContainerActors;

	UPROPERTY()
	TMap<FString, TObjectPtr<ADemoDedicatedServerLootActor>> DedicatedServerLootActors;
};
