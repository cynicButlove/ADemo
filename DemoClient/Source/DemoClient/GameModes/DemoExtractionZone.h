#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DemoTypes.h"
#include "DemoExtractionZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExtractionStateChanged, ADemoExtractionZone*, Zone, EDemoExtractionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerExtracted, ADemoExtractionZone*, Zone, AActor*, Player);

UCLASS()
class DEMOCLIENT_API ADemoExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	ADemoExtractionZone();

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	void ActivateZone();

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	void DeactivateZone();

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	EDemoExtractionState GetState() const { return State; }

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	float GetExtractionProgress() const;

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	float GetExtractionTime() const { return ExtractionTime; }

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	FText GetZoneName() const { return ZoneName; }

	UFUNCTION(BlueprintCallable, Category = "Extraction")
	bool IsPlayerInZone() const { return bPlayerInZone; }

	UPROPERTY(BlueprintAssignable, Category = "Extraction")
	FOnExtractionStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Extraction")
	FOnPlayerExtracted OnPlayerExtracted;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* ExtractionVolume;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MarkerMesh;

	UPROPERTY(EditAnywhere, Category = "Extraction")
	FText ZoneName = FText::FromString(TEXT("Extraction Point"));

	UPROPERTY(EditAnywhere, Category = "Extraction")
	float ExtractionTime = 5.f;

	UPROPERTY(EditAnywhere, Category = "Extraction")
	bool bRequiresKey = false;

	UPROPERTY(EditAnywhere, Category = "Extraction", meta = (EditCondition = "bRequiresKey"))
	FName RequiredKeyItemID;

private:
	void SetState(EDemoExtractionState NewState);

	EDemoExtractionState State = EDemoExtractionState::Inactive;
	bool bPlayerInZone = false;
	float ExtractionTimer = 0.f;

	UPROPERTY()
	AActor* ExtractingPlayer = nullptr;
};
