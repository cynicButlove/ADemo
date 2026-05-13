#include "Online/DemoDedicatedServerProxyActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ADemoDedicatedServerProxyActor::ADemoDedicatedServerProxyActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);
	BodyMesh->SetCastShadow(false);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f));
	BodyMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.8f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ProxyMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (ProxyMeshFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(ProxyMeshFinder.Object);
	}

	NameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameText"));
	NameText->SetupAttachment(SceneRoot);
	NameText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	NameText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	NameText->SetWorldSize(26.0f);
	NameText->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	NameText->SetTextRenderColor(FColor::White);
}

void ADemoDedicatedServerProxyActor::ApplySnapshot(const FDemoDedicatedServerPlayerSnapshot& Snapshot)
{
	PlayerId = Snapshot.PlayerId;

	SetActorLocationAndRotation(Snapshot.Position, FRotator(0.0f, Snapshot.Rotation.Yaw, 0.0f));
	SetActorHiddenInGame(!Snapshot.bAlive || Snapshot.bExtracted);

	if (NameText)
	{
		const FString Label = Snapshot.bIsBot
			? FString::Printf(TEXT("[AI] %s"), *Snapshot.DisplayName)
			: Snapshot.DisplayName;
		NameText->SetText(FText::FromString(Label));
		NameText->SetTextRenderColor(
			Snapshot.bIsBot
				? (Snapshot.bAlive ? FColor(255, 180, 90) : FColor(200, 110, 110))
				: (Snapshot.bAlive ? FColor::White : FColor(255, 120, 120)));
	}
}
