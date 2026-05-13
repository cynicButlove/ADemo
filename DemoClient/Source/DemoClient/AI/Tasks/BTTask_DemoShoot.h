#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DemoShoot.generated.h"

UCLASS()
class DEMOCLIENT_API UBTTask_DemoShoot : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DemoShoot();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ShootDuration = 3.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float SuppressionDurationMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FlankDurationMultiplier = 0.85f;

private:
	float ElapsedTime = 0.f;
	float ActiveShootDuration = 3.f;
};
