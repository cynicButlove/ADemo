#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DemoFindCover.generated.h"

UCLASS()
class DEMOCLIENT_API UBTTask_DemoFindCover : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DemoFindCover();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Cover")
	float SearchRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Cover")
	int32 NumCandidates = 12;

	UPROPERTY(EditAnywhere, Category = "Cover")
	bool bDrawCoverScoring = true;

	UPROPERTY(EditAnywhere, Category = "Cover")
	float DebugLifeTime = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Cover")
	float IdealMinThreatDistance = 450.f;

	UPROPERTY(EditAnywhere, Category = "Cover")
	float IdealMaxThreatDistance = 1500.f;
};
