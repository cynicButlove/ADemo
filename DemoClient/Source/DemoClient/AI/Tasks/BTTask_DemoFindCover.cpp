#include "AI/Tasks/BTTask_DemoFindCover.h"
#include "AI/DemoAIController.h"
#include "AI/DemoAICharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"

UBTTask_DemoFindCover::UBTTask_DemoFindCover()
{
	NodeName = TEXT("Demo Find Cover");
}

EBTNodeResult::Type UBTTask_DemoFindCover::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADemoAIController* AIC = Cast<ADemoAIController>(OwnerComp.GetAIOwner());
	if (!AIC) return EBTNodeResult::Failed;

	APawn* AIPawn = AIC->GetPawn();
	if (!AIPawn) return EBTNodeResult::Failed;

	ADemoAICharacter* AIChar = Cast<ADemoAICharacter>(AIPawn);

	AActor* Target = AIC->GetTargetEnemy();
	if (!Target) return EBTNodeResult::Failed;

	UWorld* World = OwnerComp.GetWorld();
	if (!World) return EBTNodeResult::Failed;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys) return EBTNodeResult::Failed;

	const FVector AILoc = AIPawn->GetActorLocation();
	const FVector ThreatLoc = Target->GetActorLocation();
	FVector ThreatToAI = AILoc - ThreatLoc;
	ThreatToAI.Z = 0.f;
	if (!ThreatToAI.Normalize())
	{
		ThreatToAI = AIPawn->GetActorForwardVector();
		ThreatToAI.Z = 0.f;
		ThreatToAI.Normalize();
	}

	const FVector ThreatRight = FVector::CrossProduct(FVector::UpVector, ThreatToAI).GetSafeNormal();
	const EDemoAITacticalRole Role = AIC->GetTacticalRole();
	const bool bWantsLeftFlank = Role == EDemoAITacticalRole::FlankLeft;
	const bool bWantsRightFlank = Role == EDemoAITacticalRole::FlankRight;
	const bool bWantsFlank = bWantsLeftFlank || bWantsRightFlank;
	const FVector PreferredFlankDir = bWantsLeftFlank ? -ThreatRight : ThreatRight;

	float BestScore = -1.f;
	FVector BestCover = AILoc;
	FString BestSummary;
	FColor BestColor = FColor::Green;

	for (int32 i = 0; i < NumCandidates; ++i)
	{
		FVector Candidate = AILoc;
		if (bWantsFlank)
		{
			FVector Jitter = FMath::VRand();
			Jitter.Z = 0.f;
			Jitter.Normalize();
			Candidate =
				ThreatLoc +
				PreferredFlankDir * FMath::RandRange(SearchRadius * 0.45f, SearchRadius) +
				ThreatToAI * FMath::RandRange(220.f, SearchRadius * 0.75f) +
				Jitter * FMath::RandRange(0.f, 260.f);
		}
		else
		{
			FVector RandDir = FMath::VRand();
			RandDir.Z = 0.f;
			RandDir.Normalize();
			Candidate = AILoc + RandDir * FMath::RandRange(200.f, SearchRadius);
		}

		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(Candidate, NavLoc)) continue;

		FVector CoverPos = NavLoc.Location;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(AIPawn);
		Params.AddIgnoredActor(Target);
		bool bBlocked = World->LineTraceSingleByChannel(
			Hit, CoverPos + FVector(0, 0, 50.f), ThreatLoc + FVector(0, 0, 50.f),
			ECC_Visibility, Params);

		const float DistFromThreat = FVector::Dist(CoverPos, ThreatLoc);
		const float DistFromSelf = FVector::Dist(CoverPos, AILoc);
		const bool bInIdealThreatRange = DistFromThreat > IdealMinThreatDistance && DistFromThreat < IdealMaxThreatDistance;
		FVector ThreatToCandidate = CoverPos - ThreatLoc;
		ThreatToCandidate.Z = 0.f;
		ThreatToCandidate.Normalize();

		float Score = 0.f;
		const float CoverScore = bBlocked ? 85.f : (Role == EDemoAITacticalRole::Suppress ? 20.f : -5.f);
		const float DistanceScore = bInIdealThreatRange ? 45.f : -FMath::Abs(DistFromThreat - ((IdealMinThreatDistance + IdealMaxThreatDistance) * 0.5f)) * 0.025f;
		const float TravelScore = -DistFromSelf * 0.045f;
		float RoleScore = 0.f;

		if (Role == EDemoAITacticalRole::Suppress)
		{
			const float FrontAlignment = FMath::Clamp(FVector::DotProduct(ThreatToCandidate, ThreatToAI), -1.f, 1.f);
			RoleScore += (FrontAlignment + 1.f) * 22.f;
			RoleScore += bBlocked ? 15.f : 30.f;
		}
		else if (bWantsFlank)
		{
			const float SideAlignment = FMath::Clamp(FVector::DotProduct(ThreatToCandidate, PreferredFlankDir), -1.f, 1.f);
			RoleScore += FMath::Max(0.f, SideAlignment) * 95.f;
			RoleScore += bBlocked ? 35.f : 10.f;
		}

		Score = CoverScore + DistanceScore + TravelScore + RoleScore;

		if (bDrawCoverScoring && AIChar && AIChar->bDrawTacticalDebug)
		{
			const FColor CandidateColor = Score > 130.f ? FColor::Green : (Score > 80.f ? FColor::Yellow : FColor::Red);
			DrawDebugSphere(World, CoverPos + FVector(0.f, 0.f, 24.f), 20.f, 8, CandidateColor, false, DebugLifeTime, 0, 1.25f);
			DrawDebugLine(World, CoverPos + FVector(0.f, 0.f, 50.f), ThreatLoc + FVector(0.f, 0.f, 50.f), bBlocked ? FColor::Blue : FColor::Red, false, DebugLifeTime, 0, 0.75f);
			DrawDebugString(
				World,
				CoverPos + FVector(0.f, 0.f, 64.f),
				FString::Printf(TEXT("%.0f C%.0f R%.0f"), Score, CoverScore, RoleScore),
				nullptr,
				CandidateColor,
				DebugLifeTime,
				true,
				0.85f);
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCover = CoverPos;
			BestSummary = FString::Printf(
				TEXT("%s score %.0f | cover %s | dist %.0f"),
				*DemoAI::TacticalRoleToString(Role),
				BestScore,
				bBlocked ? TEXT("yes") : TEXT("no"),
				DistFromThreat);
			BestColor = Score > 130.f ? FColor::Green : (Score > 80.f ? FColor::Yellow : FColor::Red);
		}
	}

	if (BestScore < 0.f) return EBTNodeResult::Failed;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (BB)
	{
		BB->SetValueAsVector(ADemoAIController::KEY_PatrolLocation, BestCover);
	}

	AIC->SetTacticalDebugMoveLocation(BestCover, BestScore, BestSummary, BestColor);

	return EBTNodeResult::Succeeded;
}
