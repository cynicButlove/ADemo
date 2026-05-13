#include "Inventory/DemoInventoryComponent.h"
#include "Engine/DataTable.h"
#include "Inventory/DemoLootItem.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/DemoAttributeSet.h"
#include "GameplayEffectTypes.h"

UDemoInventoryComponent::UDemoInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultItemDT(
		TEXT("/Game/FirstPerson/DT_Items"));
	if (DefaultItemDT.Succeeded())
	{
		ItemDataTable = DefaultItemDT.Object;
	}
}

void UDemoInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UDemoInventoryComponent::FindItemData(FName ItemID, FDemoItemData& OutData) const
{
	const FDemoItemData* Data = FindItemDataPtr(ItemID);
	if (Data)
	{
		OutData = *Data;
		return true;
	}
	return false;
}

const FDemoItemData* UDemoInventoryComponent::FindItemDataPtr(FName ItemID) const
{
	if (!ItemDataTable) return nullptr;
	return ItemDataTable->FindRow<FDemoItemData>(ItemID, TEXT("FindItemData"));
}

void UDemoInventoryComponent::ReplaceItemsFromServer(const TArray<FDemoInventorySlot>& NewItems)
{
	Items = NewItems;
	RecalculateWeight();
	OnInventoryChanged.Broadcast();
}

bool UDemoInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
	if (ItemID.IsNone() || Quantity <= 0) return false;

	const FDemoItemData* Data = FindItemDataPtr(ItemID);

	int32 StackMax = Data ? Data->MaxStack : 99;
	float ItemWeight = Data ? Data->Weight : 0.f;

	float AddedWeight = ItemWeight * Quantity;
	if (MaxWeight > 0.f && CurrentWeight + AddedWeight > MaxWeight) return false;

	for (FDemoInventorySlot& Slot : Items)
	{
		if (Slot.ItemID == ItemID && StackMax > 1)
		{
			int32 CanAdd = FMath::Min(Quantity, StackMax - Slot.Quantity);
			if (CanAdd > 0)
			{
				Slot.Quantity += CanAdd;
				Quantity -= CanAdd;
			}
		}
		if (Quantity <= 0) break;
	}

	while (Quantity > 0)
	{
		if (Items.Num() >= MaxSlots) return false;

		FDemoInventorySlot NewSlot;
		NewSlot.ItemID = ItemID;
		NewSlot.Quantity = FMath::Min(Quantity, StackMax);
		Items.Add(NewSlot);
		Quantity -= NewSlot.Quantity;
	}

	RecalculateWeight();
	OnItemPickedUp.Broadcast(ItemID, Quantity);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UDemoInventoryComponent::RemoveItem(FName ItemID, int32 Quantity)
{
	if (ItemID.IsNone() || Quantity <= 0) return false;

	int32 Remaining = Quantity;

	for (int32 i = Items.Num() - 1; i >= 0 && Remaining > 0; --i)
	{
		if (Items[i].ItemID != ItemID) continue;

		int32 ToRemove = FMath::Min(Remaining, Items[i].Quantity);
		Items[i].Quantity -= ToRemove;
		Remaining -= ToRemove;

		if (Items[i].Quantity <= 0)
		{
			Items.RemoveAt(i);
		}
	}

	if (Remaining > 0) return false;

	RecalculateWeight();
	OnInventoryChanged.Broadcast();
	return true;
}

bool UDemoInventoryComponent::HasItem(FName ItemID, int32 Quantity) const
{
	return GetItemCount(ItemID) >= Quantity;
}

int32 UDemoInventoryComponent::GetItemCount(FName ItemID) const
{
	int32 Total = 0;
	for (const FDemoInventorySlot& Slot : Items)
	{
		if (Slot.ItemID == ItemID)
		{
			Total += Slot.Quantity;
		}
	}
	return Total;
}

int32 UDemoInventoryComponent::GetTotalValue() const
{
	int32 Total = 0;
	for (const FDemoInventorySlot& Slot : Items)
	{
		if (const FDemoItemData* Data = FindItemDataPtr(Slot.ItemID))
		{
			Total += Data->BaseValue * Slot.Quantity;
		}
	}
	return Total;
}

void UDemoInventoryComponent::DropItem(FName ItemID, int32 Quantity)
{
	if (!HasItem(ItemID, Quantity)) return;

	const FDemoItemData* Data = FindItemDataPtr(ItemID);
	if (!Data) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector SpawnLoc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 150.f;
	SpawnLoc.Z -= 40.f;

	if (UWorld* World = GetWorld())
	{
		ADemoLootItem* DroppedItem = World->SpawnActor<ADemoLootItem>(
			ADemoLootItem::StaticClass(), SpawnLoc, FRotator::ZeroRotator);
		if (DroppedItem)
		{
			DroppedItem->InitFromData(ItemID, Quantity, ItemDataTable);
		}
	}

	RemoveItem(ItemID, Quantity);
}

void UDemoInventoryComponent::UseItem(FName ItemID)
{
	if (!HasItem(ItemID, 1)) return;

	const FDemoItemData* Data = FindItemDataPtr(ItemID);
	if (!Data) return;

	if (Data->ItemType == EDemoItemType::Medical && Data->HealAmount > 0.f)
	{
		if (APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
			{
				if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
				{
					UGameplayEffect* HealEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("HealEffect"));
					HealEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

					int32 Idx = HealEffect->Modifiers.Num();
					HealEffect->Modifiers.SetNum(Idx + 1);
					FGameplayModifierInfo& Mod = HealEffect->Modifiers[Idx];
					Mod.Attribute = UDemoAttributeSet::GetHealthAttribute();
					Mod.ModifierOp = EGameplayModOp::Additive;
					FScalableFloat HealVal;
					HealVal.Value = Data->HealAmount;
					Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealVal);

					FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
					FGameplayEffectSpec* Spec = new FGameplayEffectSpec(HealEffect, Ctx, 1.f);
					ASC->ApplyGameplayEffectSpecToSelf(*Spec);
					delete Spec;
				}
			}
		}

		RemoveItem(ItemID, 1);
	}
}

void UDemoInventoryComponent::RecalculateWeight()
{
	CurrentWeight = 0.f;
	for (const FDemoInventorySlot& Slot : Items)
	{
		if (const FDemoItemData* Data = FindItemDataPtr(Slot.ItemID))
		{
			CurrentWeight += Data->Weight * Slot.Quantity;
		}
	}
}
