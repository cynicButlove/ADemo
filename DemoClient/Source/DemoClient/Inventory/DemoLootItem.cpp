#include "Inventory/DemoLootItem.h"
#include "Inventory/DemoInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"

ADemoLootItem::ADemoLootItem()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->SetSphereRadius(80.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.3f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(
		TEXT("/Engine/BasicShapes/Cube"));
	if (DefaultMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(DefaultMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultItemDT(
		TEXT("/Game/FirstPerson/DT_Items"));
	if (DefaultItemDT.Succeeded())
	{
		ItemDataTable = DefaultItemDT.Object;
	}
}

void ADemoLootItem::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ADemoLootItem::OnOverlapBegin);

	if (ItemDataTable && !ItemID.IsNone())
	{
		if (const FDemoItemData* Data = ItemDataTable->FindRow<FDemoItemData>(ItemID, TEXT("")))
		{
			if (UStaticMesh* Mesh = Data->WorldMesh.LoadSynchronous())
			{
				MeshComponent->SetStaticMesh(Mesh);
			}
		}
	}
}

void ADemoLootItem::InitFromData(FName InItemID, int32 InQuantity, UDataTable* InDataTable)
{
	ItemID = InItemID;
	Quantity = InQuantity;
	ItemDataTable = InDataTable;

	if (ItemDataTable)
	{
		if (const FDemoItemData* Data = ItemDataTable->FindRow<FDemoItemData>(ItemID, TEXT("")))
		{
			if (UStaticMesh* Mesh = Data->WorldMesh.LoadSynchronous())
			{
				MeshComponent->SetStaticMesh(Mesh);
			}
		}
	}
}

FText ADemoLootItem::GetDisplayName() const
{
	if (ItemDataTable)
	{
		if (const FDemoItemData* Data = ItemDataTable->FindRow<FDemoItemData>(ItemID, TEXT("")))
		{
			return Data->DisplayName;
		}
	}
	return FText::FromName(ItemID);
}

void ADemoLootItem::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	UDemoInventoryComponent* Inv = OtherActor->FindComponentByClass<UDemoInventoryComponent>();
	if (Inv && Inv->AddItem(ItemID, Quantity))
	{
		Destroy();
	}
}
