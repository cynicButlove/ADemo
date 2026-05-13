#include "Online/DemoDedicatedServerLootActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ADemoDedicatedServerLootActor::ADemoDedicatedServerLootActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	LootMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootMesh"));
	LootMesh->SetupAttachment(SceneRoot);
	LootMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LootMesh->SetCollisionResponseToAllChannels(ECR_Block);
	LootMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	LootMesh->SetGenerateOverlapEvents(false);
	LootMesh->SetCastShadow(false);
	LootMesh->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.12f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> LootMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (LootMeshFinder.Succeeded())
	{
		LootMesh->SetStaticMesh(LootMeshFinder.Object);
	}
}

void ADemoDedicatedServerLootActor::ApplySnapshot(const FDemoDedicatedServerWorldLootSnapshot& Snapshot)
{
	LootId = Snapshot.LootId;
	ItemId = Snapshot.ItemId;
	Quantity = Snapshot.Quantity;

	SetActorLocation(Snapshot.Position);

	if (LootMesh)
	{
		LootMesh->SetRelativeScale3D(FVector(0.25f, 0.25f, FMath::Clamp(0.08f + Quantity * 0.03f, 0.12f, 0.3f)));
	}
}
