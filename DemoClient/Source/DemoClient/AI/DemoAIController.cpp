#include "AI/DemoAIController.h"
#include "AI/DemoAICharacter.h"
#include "Character/DemoCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "DemoClient.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

namespace DemoAIControllerPaths
{
	static const TCHAR* const BehaviorTreeObject = TEXT("/Game/FirstPerson/AI/BT_DemoAI.BT_DemoAI");
	static const TCHAR* const BlackboardObject = TEXT("/Game/FirstPerson/AI/BB_DemoAI.BB_DemoAI");
}

const FName ADemoAIController::KEY_TargetEnemy = TEXT("TargetEnemy");
const FName ADemoAIController::KEY_LastKnownLocation = TEXT("LastKnownLocation");
const FName ADemoAIController::KEY_PatrolLocation = TEXT("PatrolLocation");
const FName ADemoAIController::KEY_AIState = TEXT("AIState");

ADemoAIController::ADemoAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	BrainComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerceptionComp);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2000.f;
	SightConfig->LoseSightRadius = 2500.f;
	SightConfig->PeripheralVisionAngleDegrees = 60.f;
	SightConfig->SetMaxAge(5.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	AIPerceptionComp->ConfigureSense(*SightConfig);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 3000.f;
	HearingConfig->SetMaxAge(3.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	AIPerceptionComp->ConfigureSense(*HearingConfig);

	AIPerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());

	static ConstructorHelpers::FObjectFinder<UBlackboardData> BBLoader(TEXT("/Game/FirstPerson/AI/BB_DemoAI"));
	if (BBLoader.Succeeded())
	{
		DefaultBlackboardAsset = BBLoader.Object;
	}

	static ConstructorHelpers::FObjectFinder<UBehaviorTree> BTLdr(TEXT("/Game/FirstPerson/AI/BT_DemoAI"));
	if (BTLdr.Succeeded())
	{
		BehaviorTreeAsset = BTLdr.Object;
	}
}

void ADemoAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!BehaviorTreeAsset)
	{
		BehaviorTreeAsset = LoadObject<UBehaviorTree>(nullptr, DemoAIControllerPaths::BehaviorTreeObject);
	}
	if (!DefaultBlackboardAsset)
	{
		DefaultBlackboardAsset = LoadObject<UBlackboardData>(nullptr, DemoAIControllerPaths::BlackboardObject);
	}

	AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ADemoAIController::OnPerceptionUpdated);

	UBlackboardData* BBData = nullptr;
	if (BehaviorTreeAsset && BehaviorTreeAsset->BlackboardAsset)
	{
		BBData = BehaviorTreeAsset->BlackboardAsset;
	}
	else if (DefaultBlackboardAsset)
	{
		BBData = DefaultBlackboardAsset;
	}

	if (BBData)
	{
		Blackboard->InitializeBlackboard(*BBData);
		SetTacticalState(EDemoAITacticalState::Patrol);
	}

	if (BehaviorTreeAsset)
	{
		if (!BehaviorTreeAsset->RootNode)
		{
			UE_LOG(LogDemoClient, Error, TEXT("DemoAIController: 行为树 %s 没有根节点（RootNode 为空），AI 不会移动。请在 BT_DemoAI 中保存有效图。"), *BehaviorTreeAsset->GetName());
		}
		if (!RunBehaviorTree(BehaviorTreeAsset))
		{
			UE_LOG(LogDemoClient, Error, TEXT("DemoAIController: RunBehaviorTree(%s) 失败。"), *BehaviorTreeAsset->GetName());
		}
	}
	else
	{
		UE_LOG(LogDemoClient, Error, TEXT("DemoAIController: 无法加载行为树（已尝试 Constructor 与 LoadObject: %s）。请确认资产存在；若只用蓝图配置，请把角色「AI Controller Class」设为 BP_DemoAIController。"), DemoAIControllerPaths::BehaviorTreeObject);
	}
}

void ADemoAIController::OnUnPossess()
{
	if (UBehaviorTreeComponent* BTC = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTC->StopTree();
	}
	Super::OnUnPossess();
}

void ADemoAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AActor* Target = GetTargetEnemy();
	ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn());
	bool bHasLiveTarget = false;
	if (Target)
	{
		if (!LineOfSightTo(Target))
		{
			LoseTargetTimer += DeltaTime;
			if (LoseTargetTimer >= LoseTargetDelay)
			{
				if (Blackboard)
				{
					Blackboard->SetValueAsVector(KEY_LastKnownLocation, Target->GetActorLocation());
				}
				SetTargetEnemy(nullptr);
				SetTacticalState(EDemoAITacticalState::LostTarget);
				LoseTargetTimer = 0.f;
			}
		}
		else
		{
			LoseTargetTimer = 0.f;
			if (Blackboard)
			{
				Blackboard->SetValueAsVector(KEY_LastKnownLocation, Target->GetActorLocation());
			}
		}

		ADemoCharacter* PlayerChar = Cast<ADemoCharacter>(Target);
		if (PlayerChar && PlayerChar->GetIsDead())
		{
			SetTargetEnemy(nullptr);
			Target = nullptr;
		}
	}

	if (Target)
	{
		bHasLiveTarget = true;
		SetFocus(Target, EAIFocusPriority::Gameplay);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (ControlledAI)
	{
		ControlledAI->SetCombatTarget(bHasLiveTarget ? Target : nullptr);
		ControlledAI->SetTargetingState(bHasLiveTarget);
	}

	UpdateTacticalStateFromContext(DeltaTime);
	SyncBlackboardAIState();
}

void ADemoAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	ADemoCharacter* PlayerChar = Cast<ADemoCharacter>(Actor);
	if (!PlayerChar) return;
	if (PlayerChar->GetIsDead()) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		AActor* CurrentTarget = GetTargetEnemy();
		if (!CurrentTarget)
		{
			SetTargetEnemy(Actor);
		}
		AssignTacticalRole(Actor, false);
		SetTacticalState(EDemoAITacticalState::Engage);

		if (Blackboard)
		{
			Blackboard->SetValueAsVector(KEY_LastKnownLocation, Actor->GetActorLocation());
		}
		BroadcastSquadAlert(Actor, Actor->GetActorLocation());
	}
	else if (GetTargetEnemy() == Actor)
	{
		if (Blackboard)
		{
			Blackboard->SetValueAsVector(KEY_LastKnownLocation, Actor->GetActorLocation());
		}
		SetTacticalState(EDemoAITacticalState::LostTarget);
	}
}

AActor* ADemoAIController::GetTargetEnemy() const
{
	if (!Blackboard) return nullptr;
	return Cast<AActor>(Blackboard->GetValueAsObject(KEY_TargetEnemy));
}

void ADemoAIController::SetTargetEnemy(AActor* NewTarget)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsObject(KEY_TargetEnemy, NewTarget);
	}

	if (ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn()))
	{
		ControlledAI->SetAlertedBySquad(false);
		if (NewTarget)
		{
			ControlledAI->SetCombatTarget(NewTarget);
		}
	}
}

FVector ADemoAIController::GetLastKnownEnemyLocation() const
{
	if (!Blackboard) return FVector::ZeroVector;
	return Blackboard->GetValueAsVector(KEY_LastKnownLocation);
}

EDemoAITacticalState ADemoAIController::GetTacticalState() const
{
	if (const ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn()))
	{
		return ControlledAI->GetTacticalState();
	}

	return EDemoAITacticalState::Patrol;
}

EDemoAITacticalRole ADemoAIController::GetTacticalRole() const
{
	if (const ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn()))
	{
		return ControlledAI->GetTacticalRole();
	}

	return EDemoAITacticalRole::None;
}

bool ADemoAIController::WasAlertedBySquad() const
{
	if (const ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn()))
	{
		return ControlledAI->WasAlertedBySquad();
	}

	return false;
}

void ADemoAIController::SetTacticalState(EDemoAITacticalState NewState)
{
	if (ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn()))
	{
		ControlledAI->SetTacticalState(NewState);
	}
	SyncBlackboardAIState();
}

void ADemoAIController::SetTacticalRole(EDemoAITacticalRole NewRole)
{
	if (ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn()))
	{
		ControlledAI->SetTacticalRole(NewRole);
	}
	SyncBlackboardAIState();
}

void ADemoAIController::SetTacticalDebugMoveLocation(
	const FVector& Location,
	float Score,
	const FString& Summary,
	const FColor& DebugColor)
{
	TacticalDebugMoveLocation = Location;
	LastTacticalDebugScore = Score;
	LastTacticalDebugSummary = Summary;
	TacticalDebugColor = DebugColor;
	bHasTacticalDebugMoveLocation = true;
}

void ADemoAIController::ClearTacticalDebugMoveLocation()
{
	bHasTacticalDebugMoveLocation = false;
	TacticalDebugMoveLocation = FVector::ZeroVector;
	LastTacticalDebugScore = 0.f;
	LastTacticalDebugSummary.Reset();
	TacticalDebugColor = FColor::White;
}

void ADemoAIController::ReceiveSharedAlert(AActor* AlertTarget, const FVector& AlertLocation, ADemoAIController* SourceController)
{
	ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn());
	if (!ControlledAI || ControlledAI->IsDead() || GetTargetEnemy())
	{
		return;
	}

	if (Blackboard)
	{
		Blackboard->SetValueAsVector(KEY_LastKnownLocation, AlertLocation);
	}

	SharedAlertTarget = AlertTarget;
	SharedAlertSource = SourceController;
	SharedAlertAge = 0.f;
	bReceivedSharedAlert = true;

	ControlledAI->SetAlertedBySquad(true);
	AssignTacticalRole(AlertTarget, true);
	SetTacticalState(EDemoAITacticalState::Alert);

	if (GetWorld())
	{
		DrawDebugLine(GetWorld(), ControlledAI->GetActorLocation() + FVector(0.f, 0.f, 90.f), AlertLocation + FVector(0.f, 0.f, 60.f), FColor::Orange, false, 2.0f, 0, 1.5f);
	}
}

void ADemoAIController::AssignTacticalRole(AActor* TargetActor, bool bFromSharedAlert)
{
	ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn());
	if (!ControlledAI)
	{
		return;
	}

	const int32 RoleSeed = FMath::Abs(static_cast<int32>(GetUniqueID() + (bFromSharedAlert ? 1 : 0))) % 3;
	EDemoAITacticalRole NewRole = EDemoAITacticalRole::Suppress;
	if (ControlledAI->GetCombatStyle() == EDemoAICombatStyle::Rifleman)
	{
		NewRole = bFromSharedAlert && RoleSeed == 1 ? EDemoAITacticalRole::FlankLeft : EDemoAITacticalRole::Suppress;
	}
	else if (ControlledAI->GetCombatStyle() == EDemoAICombatStyle::Assaulter)
	{
		NewRole = (RoleSeed == 0) ? EDemoAITacticalRole::FlankLeft : EDemoAITacticalRole::FlankRight;
	}
	else if (ControlledAI->GetCombatStyle() == EDemoAICombatStyle::Elite && TargetActor)
	{
		NewRole = (GetUniqueID() % 2 == 0) ? EDemoAITacticalRole::FlankLeft : EDemoAITacticalRole::FlankRight;
	}
	else if (RoleSeed == 1)
	{
		NewRole = EDemoAITacticalRole::FlankLeft;
	}
	else if (RoleSeed == 2)
	{
		NewRole = EDemoAITacticalRole::FlankRight;
	}

	SetTacticalRole(NewRole);
}

void ADemoAIController::BroadcastSquadAlert(AActor* AlertTarget, const FVector& AlertLocation)
{
	UWorld* World = GetWorld();
	ADemoAICharacter* SourceAI = Cast<ADemoAICharacter>(GetPawn());
	if (!World || !SourceAI || SourceAI->IsDead())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastAlertBroadcastTime < AlertBroadcastCooldown)
	{
		return;
	}
	LastAlertBroadcastTime = Now;

	for (TActorIterator<ADemoAIController> It(World); It; ++It)
	{
		ADemoAIController* OtherController = *It;
		if (!OtherController || OtherController == this)
		{
			continue;
		}

		ADemoAICharacter* OtherAI = Cast<ADemoAICharacter>(OtherController->GetPawn());
		if (!OtherAI || OtherAI->IsDead())
		{
			continue;
		}

		if (FVector::DistSquared(OtherAI->GetActorLocation(), SourceAI->GetActorLocation()) <= FMath::Square(AlertShareRadius))
		{
			OtherController->ReceiveSharedAlert(AlertTarget, AlertLocation, this);
		}
	}
}

void ADemoAIController::UpdateTacticalStateFromContext(float DeltaTime)
{
	ADemoAICharacter* ControlledAI = Cast<ADemoAICharacter>(GetPawn());
	if (!ControlledAI || ControlledAI->IsDead())
	{
		return;
	}

	if (GetTargetEnemy())
	{
		SetTacticalState(EDemoAITacticalState::Engage);
		return;
	}

	if (bReceivedSharedAlert)
	{
		SharedAlertAge += DeltaTime;
		if (SharedAlertAge > SharedAlertMemoryTime)
		{
			bReceivedSharedAlert = false;
			SharedAlertTarget.Reset();
			SharedAlertSource.Reset();
			ControlledAI->SetAlertedBySquad(false);
		}
	}

	const FVector LastKnownLocation = GetLastKnownEnemyLocation();
	if (!LastKnownLocation.IsNearlyZero())
	{
		const float DistSq = FVector::DistSquared(ControlledAI->GetActorLocation(), LastKnownLocation);
		if (DistSq > FMath::Square(240.f))
		{
			SetTacticalState(bReceivedSharedAlert ? EDemoAITacticalState::Alert : EDemoAITacticalState::Investigate);
		}
		else
		{
			SetTacticalState(EDemoAITacticalState::Search);
		}
		return;
	}

	if (GetTacticalState() != EDemoAITacticalState::Patrol)
	{
		SetTacticalState(EDemoAITacticalState::ReturnToPatrol);
		ClearTacticalDebugMoveLocation();
	}
}

void ADemoAIController::SyncBlackboardAIState() const
{
	if (!Blackboard)
	{
		return;
	}

	const FString StateText = FString::Printf(
		TEXT("%s | %s%s"),
		*DemoAI::TacticalStateToString(GetTacticalState()),
		*DemoAI::TacticalRoleToString(GetTacticalRole()),
		WasAlertedBySquad() ? TEXT(" | SquadAlert") : TEXT(""));
	Blackboard->SetValueAsString(KEY_AIState, StateText);
}
