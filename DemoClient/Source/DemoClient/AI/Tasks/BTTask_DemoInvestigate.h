#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DemoInvestigate.generated.h"

UCLASS()
class DEMOCLIENT_API UBTTask_DemoInvestigate : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DemoInvestigate();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Investigate")
	float LookAroundTime = 3.f;

	UPROPERTY(EditAnywhere, Category = "Investigate")
	float AcceptanceRadius = 100.f;

	UPROPERTY(EditAnywhere, Category = "Search")
	int32 SearchPointCount = 3;

	UPROPERTY(EditAnywhere, Category = "Search")
	float SearchRadius = 520.f;

	UPROPERTY(EditAnywhere, Category = "Search")
	float SearchPointLookTime = 0.8f;

private:
	float Timer = 0.f;
	bool bReachedInitialLocation = false;
	bool bWaitingAtSearchPoint = false;
	int32 CurrentSearchPointIndex = INDEX_NONE;
	TArray<FVector> SearchPoints;

	void BuildSearchPoints(UBehaviorTreeComponent& OwnerComp, const FVector& CenterLocation);
	void MoveToCurrentSearchPoint(UBehaviorTreeComponent& OwnerComp);
	void FinishSearch(UBehaviorTreeComponent& OwnerComp);
};
