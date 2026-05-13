#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoDedicatedServerLootActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
struct FDemoDedicatedServerWorldLootSnapshot;

UCLASS()
class DEMOCLIENT_API ADemoDedicatedServerLootActor : public AActor
{
	GENERATED_BODY()

public:
	ADemoDedicatedServerLootActor();

	void ApplySnapshot(const FDemoDedicatedServerWorldLootSnapshot& Snapshot);

	const FString& GetLootId() const { return LootId; }
	FName GetItemId() const { return ItemId; }
	int32 GetQuantity() const { return Quantity; }

private:
	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<UStaticMeshComponent> LootMesh;

	FString LootId;
	FName ItemId = NAME_None;
	int32 Quantity = 0;
};
