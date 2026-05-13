#include "Online/DemoDedicatedServerContainerActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ADemoDedicatedServerContainerActor::ADemoDedicatedServerContainerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContainerMesh"));
	ContainerMesh->SetupAttachment(SceneRoot);
	ContainerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ContainerMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ContainerMesh->SetRelativeScale3D(FVector(0.8f, 0.6f, 0.45f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MeshFinder.Succeeded())
	{
		ContainerMesh->SetStaticMesh(MeshFinder.Object);
	}
}

void ADemoDedicatedServerContainerActor::ApplySnapshot(const FDemoDedicatedServerContainerSnapshot& Snapshot)
{
	ContainerId = Snapshot.ContainerId;
	bOpened = Snapshot.bOpened;
	RemainingItemCount = Snapshot.RemainingItemCount;

	SetActorLocation(Snapshot.Position);

	if (ContainerMesh)
	{
		const FLinearColor Color = bOpened
			? (RemainingItemCount > 0 ? FLinearColor(0.30f, 0.70f, 1.0f) : FLinearColor(0.25f, 0.25f, 0.25f))
			: FLinearColor(0.85f, 0.65f, 0.20f);
		ContainerMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(Color.R, Color.G, Color.B));
	}
}
