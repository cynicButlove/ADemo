#include "AI/Tasks/BTTask_DemoInvestigate.h"
#include "AI/DemoAICharacter.h"
#include "AI/DemoAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

UBTTask_DemoInvestigate::UBTTask_DemoInvestigate()
{
	NodeName = TEXT("Demo Investigate");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_DemoInvestigate::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!AIC) return EBTNodeResult::Failed;
	AIC->SetTacticalState(AIC->WasAlertedBySquad() ? EDemoAITacticalState::Alert : EDemoAITacticalState::Investigate);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	const FVector Location = BB->GetValueAsVector(ADemoAIController::KEY_LastKnownLocation);
	if (Location.IsNearlyZero()) return EBTNodeResult::Failed;

	AIC->MoveToLocation(Location, AcceptanceRadius);
	AIC->SetTacticalDebugMoveLocation(Location, 0.f, TEXT("Investigate last known"), FColor::Yellow);

	Timer = 0.f;
	bReachedInitialLocation = false;
	bWaitingAtSearchPoint = false;
	CurrentSearchPointIndex = INDEX_NONE;
	SearchPoints.Reset();

	return EBTNodeResult::InProgress;
}

void UBTTask_DemoInvestigate::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!AIC || !AIC->GetPawn())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (AIC->GetTargetEnemy())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (!bReachedInitialLocation)
	{
		const EPathFollowingStatus::Type Status = AIC->GetMoveStatus();
		if (Status != EPathFollowingStatus::Moving)
		{
			bReachedInitialLocation = true;
			Timer = 0.f;
			AIC->SetTacticalState(EDemoAITacticalState::Search);
			BuildSearchPoints(OwnerComp, AIC->GetLastKnownEnemyLocation());
			CurrentSearchPointIndex = SearchPoints.Num() > 0 ? 0 : INDEX_NONE;
			MoveToCurrentSearchPoint(OwnerComp);
		}
		return;
	}

	if (SearchPoints.Num() > 0)
	{
		if (!bWaitingAtSearchPoint)
		{
			if (AIC->GetMoveStatus() != EPathFollowingStatus::Moving)
			{
				bWaitingAtSearchPoint = true;
				Timer = 0.f;
			}
			return;
		}

		Timer += DeltaSeconds;
		if (APawn* Pawn = AIC->GetPawn())
		{
			const float LookYaw = FMath::Sin(Timer * 4.f) * 85.f;
			FRotator Rot = Pawn->GetActorRotation();
			Rot.Yaw += LookYaw * DeltaSeconds;
			Pawn->SetActorRotation(Rot);
		}

		if (Timer >= SearchPointLookTime)
		{
			++CurrentSearchPointIndex;
			if (CurrentSearchPointIndex >= SearchPoints.Num())
			{
				FinishSearch(OwnerComp);
				return;
			}

			MoveToCurrentSearchPoint(OwnerComp);
		}
		return;
	}

	Timer += DeltaSeconds;

	const float LookYaw = FMath::Sin(Timer * 2.f) * 60.f;
	FRotator Rot = AIC->GetPawn()->GetActorRotation();
	Rot.Yaw += LookYaw * DeltaSeconds;
	AIC->GetPawn()->SetActorRotation(Rot);

	if (Timer >= LookAroundTime)
	{
		FinishSearch(OwnerComp);
	}
}

void UBTTask_DemoInvestigate::BuildSearchPoints(UBehaviorTreeComponent& OwnerComp, const FVector& CenterLocation)
{
	SearchPoints.Reset();

	UWorld* World = OwnerComp.GetWorld();
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!World || !AIC || CenterLocation.IsNearlyZero())
	{
		return;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		return;
	}

	const int32 SafePointCount = FMath::Max(0, SearchPointCount);
	for (int32 Index = 0; Index < SafePointCount; ++Index)
	{
		const float Angle = (2.0f * PI * static_cast<float>(Index) / static_cast<float>(SafePointCount)) + 0.45f;
		const float Radius = SearchRadius * (0.65f + 0.18f * static_cast<float>(Index % 2));
		const FVector Candidate = CenterLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

		FNavLocation NavLoc;
		if (NavSys->ProjectPointToNavigation(Candidate, NavLoc))
		{
			SearchPoints.Add(NavLoc.Location);
		}
	}

	if (const ADemoAICharacter* AIChar = Cast<ADemoAICharacter>(AIC->GetPawn()))
	{
		if (AIChar->bDrawTacticalDebug)
		{
			for (int32 Index = 0; Index < SearchPoints.Num(); ++Index)
			{
				const FVector DebugLocation = SearchPoints[Index] + FVector(0.f, 0.f, 28.f);
				DrawDebugSphere(World, DebugLocation, 28.f, 10, FColor::Cyan, false, LookAroundTime + 2.f, 0, 1.5f);
				DrawDebugString(
					World,
					DebugLocation + FVector(0.f, 0.f, 42.f),
					FString::Printf(TEXT("Search %d/%d"), Index + 1, SearchPoints.Num()),
					nullptr,
					FColor::Cyan,
					LookAroundTime + 2.f,
					true,
					0.85f);
			}
		}
	}
}

void UBTTask_DemoInvestigate::MoveToCurrentSearchPoint(UBehaviorTreeComponent& OwnerComp)
{
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!AIC || !SearchPoints.IsValidIndex(CurrentSearchPointIndex))
	{
		return;
	}

	const FVector SearchLocation = SearchPoints[CurrentSearchPointIndex];
	AIC->MoveToLocation(SearchLocation, AcceptanceRadius);
	AIC->SetTacticalDebugMoveLocation(
		SearchLocation,
		static_cast<float>(CurrentSearchPointIndex + 1),
		FString::Printf(TEXT("Search point %d/%d"), CurrentSearchPointIndex + 1, SearchPoints.Num()),
		FColor::Cyan);

	bWaitingAtSearchPoint = false;
	Timer = 0.f;
}

void UBTTask_DemoInvestigate::FinishSearch(UBehaviorTreeComponent& OwnerComp)
{
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		BB->ClearValue(ADemoAIController::KEY_LastKnownLocation);
	}

	if (ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner()))
	{
		AIC->SetTacticalState(EDemoAITacticalState::ReturnToPatrol);
		AIC->ClearTacticalDebugMoveLocation();
	}

	SearchPoints.Reset();
	CurrentSearchPointIndex = INDEX_NONE;
	bWaitingAtSearchPoint = false;
	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
