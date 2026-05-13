#include "GameModes/DemoExtractionZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Inventory/DemoInventoryComponent.h"
#include "GameFramework/Character.h"

ADemoExtractionZone::ADemoExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;

	ExtractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ExtractionVolume"));
	ExtractionVolume->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	ExtractionVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = ExtractionVolume;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(ExtractionVolume);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	MarkerMesh->SetRelativeScale3D(FVector(0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowMesh(
		TEXT("/Engine/BasicShapes/Cone"));
	if (ArrowMesh.Succeeded())
	{
		MarkerMesh->SetStaticMesh(ArrowMesh.Object);
	}
}

void ADemoExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	ExtractionVolume->OnComponentBeginOverlap.AddDynamic(this, &ADemoExtractionZone::OnOverlapBegin);
	ExtractionVolume->OnComponentEndOverlap.AddDynamic(this, &ADemoExtractionZone::OnOverlapEnd);

	MarkerMesh->SetVisibility(false);
}

void ADemoExtractionZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == EDemoExtractionState::Available && MarkerMesh)
	{
		FRotator Rot = MarkerMesh->GetRelativeRotation();
		Rot.Yaw += 60.f * DeltaTime;
		MarkerMesh->SetRelativeRotation(Rot);
	}

	if (State == EDemoExtractionState::Extracting && bPlayerInZone && ExtractingPlayer)
	{
		ExtractionTimer += DeltaTime;
		if (ExtractionTimer >= ExtractionTime)
		{
			SetState(EDemoExtractionState::Completed);
			OnPlayerExtracted.Broadcast(this, ExtractingPlayer);
		}
	}
}

void ADemoExtractionZone::ActivateZone()
{
	if (State == EDemoExtractionState::Inactive)
	{
		SetState(EDemoExtractionState::Available);
		MarkerMesh->SetVisibility(true);
	}
}

void ADemoExtractionZone::DeactivateZone()
{
	SetState(EDemoExtractionState::Inactive);
	MarkerMesh->SetVisibility(false);
	bPlayerInZone = false;
	ExtractionTimer = 0.f;
	ExtractingPlayer = nullptr;
}

float ADemoExtractionZone::GetExtractionProgress() const
{
	if (ExtractionTime <= 0.f) return 1.f;
	return FMath::Clamp(ExtractionTimer / ExtractionTime, 0.f, 1.f);
}

void ADemoExtractionZone::SetState(EDemoExtractionState NewState)
{
	if (State == NewState) return;
	State = NewState;
	OnStateChanged.Broadcast(this, NewState);
}

void ADemoExtractionZone::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* PlayerChar = Cast<ACharacter>(OtherActor);
	if (!PlayerChar || !PlayerChar->IsPlayerControlled()) return;
	if (State != EDemoExtractionState::Available) return;

	if (bRequiresKey)
	{
		UDemoInventoryComponent* Inv = PlayerChar->FindComponentByClass<UDemoInventoryComponent>();
		if (!Inv || !Inv->HasItem(RequiredKeyItemID, 1))
		{
			return;
		}
	}

	bPlayerInZone = true;
	ExtractingPlayer = OtherActor;
	ExtractionTimer = 0.f;
	SetState(EDemoExtractionState::Extracting);
}

void ADemoExtractionZone::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == ExtractingPlayer)
	{
		bPlayerInZone = false;
		ExtractionTimer = 0.f;
		ExtractingPlayer = nullptr;

		if (State == EDemoExtractionState::Extracting)
		{
			SetState(EDemoExtractionState::Available);
		}
	}
}
