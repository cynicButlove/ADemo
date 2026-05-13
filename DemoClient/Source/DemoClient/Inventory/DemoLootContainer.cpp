#include "Inventory/DemoLootContainer.h"
#include "Inventory/DemoInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"

ADemoLootContainer::ADemoLootContainer()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrateMesh(
		TEXT("/Engine/BasicShapes/Cube"));
	if (CrateMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CrateMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.75f, 0.5f, 0.4f));
	}

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(RootComponent);
	InteractionVolume->SetSphereRadius(200.f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetGenerateOverlapEvents(true);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Overlap);

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultItemDT(
		TEXT("/Game/FirstPerson/DT_Items"));
	if (DefaultItemDT.Succeeded())
	{
		ItemDataTable = DefaultItemDT.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultLootDT(
		TEXT("/Game/FirstPerson/DT_LootTable"));
	if (DefaultLootDT.Succeeded())
	{
		LootTable = DefaultLootDT.Object;
	}
}

void ADemoLootContainer::BeginPlay()
{
	Super::BeginPlay();
}

void ADemoLootContainer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsBeingSearched && CurrentSearcher)
	{
		float Dist = FVector::Dist(GetActorLocation(), CurrentSearcher->GetActorLocation());
		if (Dist > InteractionVolume->GetScaledSphereRadius())
		{
			bIsBeingSearched = false;
			SearchProgress = 0.f;
			CurrentSearcher = nullptr;
			return;
		}

		SearchProgress += DeltaTime / SearchTime;
		if (SearchProgress >= 1.0f)
		{
			bIsBeingSearched = false;
			bIsOpened = true;
			SearchProgress = 1.0f;
			GenerateLoot();
			OnContainerOpened.Broadcast(this);
			CurrentSearcher = nullptr;
		}
	}
}

void ADemoLootContainer::Interact(AActor* Interactor)
{
	if (!Interactor) return;
	if (bIsOpened) return;

	if (!bIsBeingSearched)
	{
		bIsBeingSearched = true;
		SearchProgress = 0.f;
		CurrentSearcher = Interactor;
	}
	else if (CurrentSearcher == Interactor)
	{
		SearchProgress += 0.35f;
	}

	if (SearchProgress >= 1.0f)
	{
		bIsBeingSearched = false;
		bIsOpened = true;
		SearchProgress = 1.0f;
		GenerateLoot();
		OnContainerOpened.Broadcast(this);
		CurrentSearcher = nullptr;
	}
}

bool ADemoLootContainer::TakeItem(AActor* Taker, int32 SlotIndex)
{
	if (!bIsOpened || !Taker) return false;
	if (SlotIndex < 0 || SlotIndex >= Contents.Num()) return false;
	if (Contents[SlotIndex].IsEmpty()) return false;

	UDemoInventoryComponent* Inv = Taker->FindComponentByClass<UDemoInventoryComponent>();
	if (!Inv) return false;

	FDemoInventorySlot& Slot = Contents[SlotIndex];
	if (Inv->AddItem(Slot.ItemID, Slot.Quantity))
	{
		Slot.ItemID = NAME_None;
		Slot.Quantity = 0;
		return true;
	}
	return false;
}

void ADemoLootContainer::GenerateLoot()
{
	if (!LootTable || !ItemDataTable) return;

	TArray<FDemoLootTableRow*> AllRows;
	LootTable->GetAllRows<FDemoLootTableRow>(TEXT("GenerateLoot"), AllRows);

	int32 NumItems = FMath::RandRange(MinItems, MaxItems);
	int32 Added = 0;

	TArray<FDemoLootTableRow*> ShuffledRows = AllRows;
	for (int32 i = ShuffledRows.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		ShuffledRows.Swap(i, j);
	}

	for (FDemoLootTableRow* Row : ShuffledRows)
	{
		if (Added >= NumItems) break;
		if (!Row) continue;

		if (FMath::FRand() <= Row->SpawnChance)
		{
			FDemoInventorySlot Slot;
			Slot.ItemID = Row->ItemID;
			Slot.Quantity = FMath::RandRange(Row->MinQuantity, Row->MaxQuantity);
			Contents.Add(Slot);
			Added++;
		}
	}
}
