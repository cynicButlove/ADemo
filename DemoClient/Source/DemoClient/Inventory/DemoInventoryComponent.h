#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DemoTypes.h"
#include "DemoInventoryComponent.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemPickedUp, FName, ItemID, int32, Quantity);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEMOCLIENT_API UDemoInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDemoInventoryComponent();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(FName ItemID, int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FName ItemID, int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasItem(FName ItemID, int32 Quantity = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemCount(FName ItemID) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FDemoInventorySlot>& GetAllItems() const { return Items; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	float GetCurrentWeight() const { return CurrentWeight; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	float GetMaxWeight() const { return MaxWeight; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetTotalValue() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropItem(FName ItemID, int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseItem(FName ItemID);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool FindItemData(FName ItemID, FDemoItemData& OutData) const;

	void ReplaceItemsFromServer(const TArray<FDemoInventorySlot>& NewItems);

	const FDemoItemData* FindItemDataPtr(FName ItemID) const;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemPickedUp OnItemPickedUp;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	UDataTable* ItemDataTable;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	float MaxWeight = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	int32 MaxSlots = 20;

private:
	void RecalculateWeight();

	UPROPERTY()
	TArray<FDemoInventorySlot> Items;

	float CurrentWeight = 0.f;
};
