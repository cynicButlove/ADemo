#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Blackboard/BlackboardKeyEnums.h"
#include "DemoBTSetupNodes.generated.h"

/** 用于在 C++ 中配置受保护的黑板选择器（装饰器：某键 Is Set）。 */
UCLASS()
class DEMOCLIENT_API UBTDecorator_DemoBBIsSet : public UBTDecorator_Blackboard
{
	GENERATED_BODY()

public:
	UBTDecorator_DemoBBIsSet(const FObjectInitializer& ObjectInitializer);

	void InitializeForKey(FName KeyName);
};

/** MoveTo，目标黑板键固定为 PatrolLocation（与 Demo 巡逻/找掩体任务一致）。 */
UCLASS()
class DEMOCLIENT_API UBTTask_DemoMoveToPatrolKey : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_DemoMoveToPatrolKey(const FObjectInitializer& ObjectInitializer);
};
