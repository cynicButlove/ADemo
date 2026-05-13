#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoDedicatedServerContainerActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
struct FDemoDedicatedServerContainerSnapshot;

UCLASS()
class DEMOCLIENT_API ADemoDedicatedServerContainerActor : public AActor
{
	GENERATED_BODY()

public:
	ADemoDedicatedServerContainerActor();

	void ApplySnapshot(const FDemoDedicatedServerContainerSnapshot& Snapshot);

	const FString& GetContainerId() const { return ContainerId; }
	bool IsOpened() const { return bOpened; }
	int32 GetRemainingItemCount() const { return RemainingItemCount; }

private:
	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<UStaticMeshComponent> ContainerMesh;

	FString ContainerId;
	bool bOpened = false;
	int32 RemainingItemCount = 0;
};
