#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DemoPatrol.generated.h"

UCLASS()
class DEMOCLIENT_API UBTTask_DemoPatrol : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DemoPatrol();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float AcceptanceRadius = 100.f;
};
