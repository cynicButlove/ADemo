#include "AI/Tasks/BTTask_DemoPatrol.h"
#include "AI/DemoAICharacter.h"
#include "AI/DemoAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_DemoPatrol::UBTTask_DemoPatrol()
{
	NodeName = TEXT("Demo Patrol");
}

EBTNodeResult::Type UBTTask_DemoPatrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Failed;

	ADemoAICharacter* AIChar = Cast<ADemoAICharacter>(AIC->GetPawn());
	if (!AIChar) return EBTNodeResult::Failed;

	if (ADemoAIController* DemoController = Cast<ADemoAIController>(AIC))
	{
		DemoController->SetTacticalState(EDemoAITacticalState::Patrol);
		DemoController->SetTacticalRole(EDemoAITacticalRole::None);
		DemoController->ClearTacticalDebugMoveLocation();
	}

	if (AIChar->PatrolPoints.Num() == 0) return EBTNodeResult::Failed;

	FVector TargetPoint = AIChar->PatrolPoints[AIChar->CurrentPatrolIndex];
	AIChar->CurrentPatrolIndex = (AIChar->CurrentPatrolIndex + 1) % AIChar->PatrolPoints.Num();

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (BB)
	{
		BB->SetValueAsVector(ADemoAIController::KEY_PatrolLocation, TargetPoint);
	}

	return EBTNodeResult::Succeeded;
}
