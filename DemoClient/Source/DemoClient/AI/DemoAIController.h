#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/DemoAITypes.h"
#include "DemoAIController.generated.h"

class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBlackboardData;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UCLASS()
class DEMOCLIENT_API ADemoAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADemoAIController();

	UFUNCTION(BlueprintCallable, Category = "AI")
	AActor* GetTargetEnemy() const;

	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetTargetEnemy(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "AI")
	FVector GetLastKnownEnemyLocation() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	EDemoAITacticalState GetTacticalState() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	EDemoAITacticalRole GetTacticalRole() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	bool WasAlertedBySquad() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	bool HasTacticalDebugMoveLocation() const { return bHasTacticalDebugMoveLocation; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	FVector GetTacticalDebugMoveLocation() const { return TacticalDebugMoveLocation; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	float GetLastTacticalDebugScore() const { return LastTacticalDebugScore; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	FString GetLastTacticalDebugSummary() const { return LastTacticalDebugSummary; }

	FColor GetTacticalDebugColor() const { return TacticalDebugColor; }

	void SetTacticalState(EDemoAITacticalState NewState);
	void SetTacticalRole(EDemoAITacticalRole NewRole);
	void ReceiveSharedAlert(AActor* AlertTarget, const FVector& AlertLocation, ADemoAIController* SourceController);
	void AssignTacticalRole(AActor* TargetActor, bool bFromSharedAlert);
	void SetTacticalDebugMoveLocation(const FVector& Location, float Score, const FString& Summary, const FColor& DebugColor);
	void ClearTacticalDebugMoveLocation();

	static const FName KEY_TargetEnemy;
	static const FName KEY_LastKnownLocation;
	static const FName KEY_PatrolLocation;
	static const FName KEY_AIState;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY()
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY()
	UAISenseConfig_Hearing* HearingConfig;

private:
	float LoseTargetTimer = 0.f;
	float LoseTargetDelay = 5.f;
	float AlertShareRadius = 2200.f;
	float SharedAlertMemoryTime = 9.f;
	float LastAlertBroadcastTime = -1000.f;
	float AlertBroadcastCooldown = 1.0f;
	float SharedAlertAge = 0.f;

	bool bReceivedSharedAlert = false;
	bool bHasTacticalDebugMoveLocation = false;
	float LastTacticalDebugScore = 0.f;
	FVector TacticalDebugMoveLocation = FVector::ZeroVector;
	FString LastTacticalDebugSummary;
	FColor TacticalDebugColor = FColor::White;

	UPROPERTY()
	TWeakObjectPtr<AActor> SharedAlertTarget;

	UPROPERTY()
	TWeakObjectPtr<ADemoAIController> SharedAlertSource;

	UPROPERTY()
	UBlackboardData* DefaultBlackboardAsset;

	void BroadcastSquadAlert(AActor* AlertTarget, const FVector& AlertLocation);
	void UpdateTacticalStateFromContext(float DeltaTime);
	void SyncBlackboardAIState() const;
};
