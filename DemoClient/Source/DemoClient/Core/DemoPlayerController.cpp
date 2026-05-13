#include "Core/DemoPlayerController.h"

#include "Character/DemoCharacter.h"
#include "Inventory/DemoInventoryComponent.h"
#include "Online/DemoDedicatedServerContainerActor.h"
#include "Online/DemoDedicatedServerLootActor.h"
#include "Online/DemoDedicatedServerProxyActor.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "Online/DemoOnlineGameInstance.h"

ADemoPlayerController::ADemoPlayerController()
{
	bShowMouseCursor = false;
}

void ADemoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	if (IsLocalController())
	{
		BindDedicatedServerDelegates();
	}
}

void ADemoPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllDedicatedServerProxies();
	RemoveAllDedicatedServerContainers();
	RemoveAllDedicatedServerWorldLoot();

	if (bDedicatedServerDelegatesBound && GetGameInstance())
	{
		if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
		{
			DedicatedServer->OnSnapshotReceived.RemoveAll(this);
			DedicatedServer->OnInventoryUpdated.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool ADemoPlayerController::ShouldUseDedicatedServerPath() const
{
	const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance());
	return GI && GI->ShouldUseDedicatedServer();
}

void ADemoPlayerController::BindDedicatedServerDelegates()
{
	if (bDedicatedServerDelegatesBound || !ShouldUseDedicatedServerPath() || !GetGameInstance())
	{
		return;
	}

	if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
	{
		DedicatedServer->OnSnapshotReceived.RemoveAll(this);
		DedicatedServer->OnSnapshotReceived.AddUObject(this, &ADemoPlayerController::HandleDedicatedServerSnapshot);
		DedicatedServer->OnInventoryUpdated.RemoveAll(this);
		DedicatedServer->OnInventoryUpdated.AddUObject(this, &ADemoPlayerController::HandleDedicatedServerInventoryUpdated);
		bDedicatedServerDelegatesBound = true;

		if (DedicatedServer->GetLastSnapshot().Players.Num() > 0)
		{
			HandleDedicatedServerSnapshot(DedicatedServer->GetLastSnapshot());
		}

		HandleDedicatedServerInventoryUpdated(DedicatedServer->GetInventoryItems());
	}
}

void ADemoPlayerController::HandleDedicatedServerSnapshot(const FDemoDedicatedServerSnapshot& Snapshot)
{
	if (!IsLocalController() || !ShouldUseDedicatedServerPath())
	{
		return;
	}

	SyncDedicatedInventoryToPawn();
	ApplySnapshotToLocalPawn(Snapshot);
	SyncRemoteProxyActors(Snapshot);
	SyncDedicatedServerContainers(Snapshot);
	SyncDedicatedServerWorldLoot(Snapshot);
}

void ADemoPlayerController::HandleDedicatedServerInventoryUpdated(const TArray<FDemoInventorySlot>& InventoryItems)
{
	(void)InventoryItems;
	SyncDedicatedInventoryToPawn();
}

void ADemoPlayerController::ApplySnapshotToLocalPawn(const FDemoDedicatedServerSnapshot& Snapshot)
{
	if (!GetGameInstance())
	{
		return;
	}

	UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
	ADemoCharacter* DemoCharacter = Cast<ADemoCharacter>(GetPawn());
	if (!DedicatedServer || !DemoCharacter)
	{
		return;
	}

	const FString LocalPlayerId = DedicatedServer->GetPlayerId();
	for (const FDemoDedicatedServerPlayerSnapshot& PlayerSnapshot : Snapshot.Players)
	{
		if (PlayerSnapshot.PlayerId == LocalPlayerId)
		{
			DemoCharacter->ApplyDedicatedServerSnapshot(PlayerSnapshot);
			break;
		}
	}
}

void ADemoPlayerController::SyncDedicatedInventoryToPawn()
{
	if (!GetGameInstance())
	{
		return;
	}

	UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
	ADemoCharacter* DemoCharacter = Cast<ADemoCharacter>(GetPawn());
	if (!DedicatedServer || !DemoCharacter)
	{
		return;
	}

	if (UDemoInventoryComponent* InventoryComponent = DemoCharacter->FindComponentByClass<UDemoInventoryComponent>())
	{
		InventoryComponent->ReplaceItemsFromServer(DedicatedServer->GetInventoryItems());
	}
}

void ADemoPlayerController::SyncRemoteProxyActors(const FDemoDedicatedServerSnapshot& Snapshot)
{
	if (!GetWorld() || !GetGameInstance())
	{
		return;
	}

	UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
	if (!DedicatedServer)
	{
		return;
	}

	const FString LocalPlayerId = DedicatedServer->GetPlayerId();
	TSet<FString> SeenPlayerIds;

	for (const FDemoDedicatedServerPlayerSnapshot& PlayerSnapshot : Snapshot.Players)
	{
		if (PlayerSnapshot.PlayerId.IsEmpty() || PlayerSnapshot.PlayerId == LocalPlayerId)
		{
			continue;
		}

		SeenPlayerIds.Add(PlayerSnapshot.PlayerId);

		TObjectPtr<ADemoDedicatedServerProxyActor>& ProxyActor = DedicatedServerProxyActors.FindOrAdd(PlayerSnapshot.PlayerId);
		if (!IsValid(ProxyActor))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ProxyActor = GetWorld()->SpawnActor<ADemoDedicatedServerProxyActor>(
				ADemoDedicatedServerProxyActor::StaticClass(),
				PlayerSnapshot.Position,
				FRotator::ZeroRotator,
				SpawnParams);
		}

		if (IsValid(ProxyActor))
		{
			ProxyActor->ApplySnapshot(PlayerSnapshot);
		}
	}

	TArray<FString> KeysToRemove;
	for (const TPair<FString, TObjectPtr<ADemoDedicatedServerProxyActor>>& Pair : DedicatedServerProxyActors)
	{
		if (!SeenPlayerIds.Contains(Pair.Key))
		{
			if (IsValid(Pair.Value))
			{
				Pair.Value->Destroy();
			}
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FString& PlayerId : KeysToRemove)
	{
		DedicatedServerProxyActors.Remove(PlayerId);
	}
}

void ADemoPlayerController::SyncDedicatedServerContainers(const FDemoDedicatedServerSnapshot& Snapshot)
{
	if (!GetWorld())
	{
		return;
	}

	TSet<FString> SeenContainerIds;

	for (const FDemoDedicatedServerContainerSnapshot& ContainerSnapshot : Snapshot.Containers)
	{
		if (ContainerSnapshot.ContainerId.IsEmpty())
		{
			continue;
		}

		SeenContainerIds.Add(ContainerSnapshot.ContainerId);

		TObjectPtr<ADemoDedicatedServerContainerActor>& ContainerActor =
			DedicatedServerContainerActors.FindOrAdd(ContainerSnapshot.ContainerId);
		if (!IsValid(ContainerActor))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ContainerActor = GetWorld()->SpawnActor<ADemoDedicatedServerContainerActor>(
				ADemoDedicatedServerContainerActor::StaticClass(),
				ContainerSnapshot.Position,
				FRotator::ZeroRotator,
				SpawnParams);
		}

		if (IsValid(ContainerActor))
		{
			ContainerActor->ApplySnapshot(ContainerSnapshot);
		}
	}

	TArray<FString> KeysToRemove;
	for (const TPair<FString, TObjectPtr<ADemoDedicatedServerContainerActor>>& Pair : DedicatedServerContainerActors)
	{
		if (!SeenContainerIds.Contains(Pair.Key))
		{
			if (IsValid(Pair.Value))
			{
				Pair.Value->Destroy();
			}
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FString& ContainerId : KeysToRemove)
	{
		DedicatedServerContainerActors.Remove(ContainerId);
	}
}

void ADemoPlayerController::SyncDedicatedServerWorldLoot(const FDemoDedicatedServerSnapshot& Snapshot)
{
	if (!GetWorld())
	{
		return;
	}

	TSet<FString> SeenLootIds;

	for (const FDemoDedicatedServerWorldLootSnapshot& LootSnapshot : Snapshot.WorldLoot)
	{
		if (LootSnapshot.LootId.IsEmpty() || LootSnapshot.ItemId.IsNone() || LootSnapshot.Quantity <= 0)
		{
			continue;
		}

		SeenLootIds.Add(LootSnapshot.LootId);

		TObjectPtr<ADemoDedicatedServerLootActor>& LootActor = DedicatedServerLootActors.FindOrAdd(LootSnapshot.LootId);
		if (!IsValid(LootActor))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			LootActor = GetWorld()->SpawnActor<ADemoDedicatedServerLootActor>(
				ADemoDedicatedServerLootActor::StaticClass(),
				LootSnapshot.Position,
				FRotator::ZeroRotator,
				SpawnParams);
		}

		if (IsValid(LootActor))
		{
			LootActor->ApplySnapshot(LootSnapshot);
		}
	}

	TArray<FString> KeysToRemove;
	for (const TPair<FString, TObjectPtr<ADemoDedicatedServerLootActor>>& Pair : DedicatedServerLootActors)
	{
		if (!SeenLootIds.Contains(Pair.Key))
		{
			if (IsValid(Pair.Value))
			{
				Pair.Value->Destroy();
			}
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FString& LootId : KeysToRemove)
	{
		DedicatedServerLootActors.Remove(LootId);
	}
}

void ADemoPlayerController::RemoveAllDedicatedServerProxies()
{
	for (TPair<FString, TObjectPtr<ADemoDedicatedServerProxyActor>>& Pair : DedicatedServerProxyActors)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}

	DedicatedServerProxyActors.Empty();
}

void ADemoPlayerController::RemoveAllDedicatedServerContainers()
{
	for (TPair<FString, TObjectPtr<ADemoDedicatedServerContainerActor>>& Pair : DedicatedServerContainerActors)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}

	DedicatedServerContainerActors.Empty();
}

void ADemoPlayerController::RemoveAllDedicatedServerWorldLoot()
{
	for (TPair<FString, TObjectPtr<ADemoDedicatedServerLootActor>>& Pair : DedicatedServerLootActors)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}

	DedicatedServerLootActors.Empty();
}
