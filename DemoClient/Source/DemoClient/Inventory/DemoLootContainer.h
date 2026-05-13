#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DemoTypes.h"
#include "DemoLootContainer.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContainerOpened, ADemoLootContainer*, Container);

UCLASS()
class DEMOCLIENT_API ADemoLootContainer : public AActor
{
	GENERATED_BODY()

public:
	ADemoLootContainer();

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void Interact(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	const TArray<FDemoInventorySlot>& GetContents() const { return Contents; }

	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool TakeItem(AActor* Taker, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool IsOpened() const { return bIsOpened; }

	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool IsBeingSearched() const { return bIsBeingSearched; }

	UFUNCTION(BlueprintCallable, Category = "Loot")
	float GetSearchProgress() const { return SearchProgress; }

	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FOnContainerOpened OnContainerOpened;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* InteractionVolume;

	UPROPERTY(EditAnywhere, Category = "Loot")
	UDataTable* LootTable;

	UPROPERTY(EditAnywhere, Category = "Loot")
	UDataTable* ItemDataTable;

	UPROPERTY(EditAnywhere, Category = "Loot")
	float SearchTime = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Loot")
	int32 MinItems = 1;

	UPROPERTY(EditAnywhere, Category = "Loot")
	int32 MaxItems = 4;

private:
	void GenerateLoot();

	UPROPERTY()
	TArray<FDemoInventorySlot> Contents;

	bool bIsOpened = false;
	bool bIsBeingSearched = false;
	float SearchProgress = 0.f;

	UPROPERTY()
	AActor* CurrentSearcher = nullptr;
};
