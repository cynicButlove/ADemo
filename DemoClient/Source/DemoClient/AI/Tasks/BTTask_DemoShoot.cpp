#include "AI/Tasks/BTTask_DemoShoot.h"
#include "AI/DemoAICharacter.h"
#include "AI/DemoAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"

UBTTask_DemoShoot::UBTTask_DemoShoot()
{
	NodeName = TEXT("Demo Shoot");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_DemoShoot::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!AIC) return EBTNodeResult::Failed;

	ADemoAICharacter* AIChar = Cast<ADemoAICharacter>(AIC->GetPawn());
	if (!AIChar || AIChar->IsDead()) return EBTNodeResult::Failed;

	AActor* Target = AIC->GetTargetEnemy();
	if (!Target) return EBTNodeResult::Failed;

	AIC->SetFocus(Target);
	AIC->SetTacticalState(EDemoAITacticalState::Engage);
	AIChar->StartFiring();
	ElapsedTime = 0.f;
	ActiveShootDuration = ShootDuration;

	const EDemoAITacticalRole Role = AIC->GetTacticalRole();
	if (Role == EDemoAITacticalRole::Suppress)
	{
		ActiveShootDuration *= SuppressionDurationMultiplier;
		AIC->SetTacticalDebugMoveLocation(Target->GetActorLocation(), 0.f, TEXT("Suppressing fire"), FColor::Red);
	}
	else if (Role == EDemoAITacticalRole::FlankLeft || Role == EDemoAITacticalRole::FlankRight)
	{
		ActiveShootDuration *= FlankDurationMultiplier;
		AIC->SetTacticalDebugMoveLocation(Target->GetActorLocation(), 0.f, TEXT("Flank fire"), FColor::Orange);
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_DemoShoot::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;

	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!AIC)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	ADemoAICharacter* AIChar = Cast<ADemoAICharacter>(AIC->GetPawn());
	AActor* Target = AIC->GetTargetEnemy();

	if (!AIChar || AIChar->IsDead() || !Target)
	{
		if (AIChar) AIChar->StopFiring();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - AIChar->GetActorLocation();
	float AimError = (1.f - AIChar->AimAccuracy) * 200.f;
	const EDemoAITacticalRole Role = AIC->GetTacticalRole();
	if (Role == EDemoAITacticalRole::Suppress)
	{
		AimError *= 1.25f;
	}
	else if (Role == EDemoAITacticalRole::FlankLeft || Role == EDemoAITacticalRole::FlankRight)
	{
		AimError *= 0.85f;
	}

	if (AIChar->GetCombatStyle() == EDemoAICombatStyle::Elite)
	{
		AimError *= 0.7f;
	}

	FVector AimOffset = FVector(
		FMath::RandRange(-AimError, AimError),
		FMath::RandRange(-AimError, AimError),
		FMath::RandRange(-AimError * 0.5f, AimError * 0.5f));

	FRotator LookRot = (ToTarget + AimOffset).Rotation();
	AIChar->SetActorRotation(FRotator(0.f, LookRot.Yaw, 0.f));

	if (AIChar->bDrawTacticalDebug && AIChar->GetWorld())
	{
		const FColor FireColor = Role == EDemoAITacticalRole::Suppress ? FColor::Red : FColor::Orange;
		DrawDebugLine(
			AIChar->GetWorld(),
			AIChar->GetActorLocation() + FVector(0.f, 0.f, 85.f),
			Target->GetActorLocation() + FVector(0.f, 0.f, 75.f),
			FireColor,
			false,
			0.f,
			0,
			1.0f);
	}

	if (ElapsedTime >= ActiveShootDuration)
	{
		AIChar->StopFiring();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_DemoShoot::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (AIC)
	{
		if (ADemoAICharacter* AIChar = Cast<ADemoAICharacter>(AIC->GetPawn()))
		{
			AIChar->StopFiring();
		}
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
	return EBTNodeResult::Aborted;
}
