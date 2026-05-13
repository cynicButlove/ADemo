#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DemoTypes.h"
#include "DemoLootItem.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UDataTable;

UCLASS()
class DEMOCLIENT_API ADemoLootItem : public AActor
{
	GENERATED_BODY()

public:
	ADemoLootItem();

	void InitFromData(FName InItemID, int32 InQuantity, UDataTable* InDataTable);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	FName GetItemID() const { return ItemID; }

	UFUNCTION(BlueprintCallable, Category = "Loot")
	int32 GetQuantity() const { return Quantity; }

	UFUNCTION(BlueprintCallable, Category = "Loot")
	FText GetDisplayName() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere, Category = "Loot")
	FName ItemID;

	UPROPERTY(EditAnywhere, Category = "Loot")
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, Category = "Loot")
	UDataTable* ItemDataTable;

	UPROPERTY(EditAnywhere, Category = "Loot")
	bool bAutoPickup = false;
};
