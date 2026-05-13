#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoDedicatedServerProxyActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;
struct FDemoDedicatedServerPlayerSnapshot;

UCLASS()
class DEMOCLIENT_API ADemoDedicatedServerProxyActor : public AActor
{
	GENERATED_BODY()

public:
	ADemoDedicatedServerProxyActor();

	void ApplySnapshot(const FDemoDedicatedServerPlayerSnapshot& Snapshot);

	const FString& GetPlayerId() const { return PlayerId; }

private:
	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "DedicatedServer")
	TObjectPtr<UTextRenderComponent> NameText;

	FString PlayerId;
};
