#include "Core/DemoGameMode.h"
#include "Character/DemoCharacter.h"
#include "Core/DemoPlayerController.h"
#include "Online/DemoOnlineGameInstance.h"
#include "Inventory/DemoInventoryComponent.h"
#include "UI/DemoHUD.h"
#include "GameModes/DemoExtractionZone.h"
#include "Inventory/DemoInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/DemoAICharacter.h"
#include "NavigationSystem.h"
#include "GameFramework/GameSession.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool IsDedicatedServerGameplay(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(World->GetGameInstance());
		return GI && GI->ShouldUseDedicatedServer();
	}
}

ADemoGameMode::ADemoGameMode()
{
	DefaultPawnClass = ADemoCharacter::StaticClass();
	PlayerControllerClass = ADemoPlayerController::StaticClass();
	HUDClass = ADemoHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FClassFinder<ADemoAICharacter> AICharBP(TEXT("/Game/FirstPerson/Blueprints/BP_DemoAICharacter"));
	if (AICharBP.Succeeded())
	{
		AICharacterClass = AICharBP.Class;
	}
	else
	{
		AICharacterClass = ADemoAICharacter::StaticClass();
	}
}

void ADemoGameMode::StartPlay()
{
	Super::StartPlay();

	if (IsDedicatedServerGameplay(GetWorld()))
	{
		bMatchActive = false;
		MatchElapsedTime = 0.0f;
		return;
	}

	for (TActorIterator<ADemoExtractionZone> It(GetWorld()); It; ++It)
	{
		AllExtractionZones.Add(*It);
		(*It)->OnPlayerExtracted.AddDynamic(this, &ADemoGameMode::HandlePlayerExtracted);
	}

	bMatchActive = true;
	MatchElapsedTime = 0.f;

	SpawnAIEnemies();
}

FString ADemoGameMode::InitNewPlayer(
	APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId,
	const FString& Options,
	const FString& Portal)
{
	const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	if (ErrorMessage.IsEmpty() && NewPlayerController)
	{
		const FString DisplayName = UGameplayStatics::ParseOption(Options, TEXT("DisplayName")).TrimStartAndEnd();
		if (!DisplayName.IsEmpty())
		{
			ChangeName(NewPlayerController, DisplayName, true);
		}
	}

	return ErrorMessage;
}

void ADemoGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDedicatedServerGameplay(GetWorld()))
	{
		return;
	}

	if (!bMatchActive) return;

	MatchElapsedTime += DeltaSeconds;

	if (!bExtractionsActivated && MatchElapsedTime >= ExtractionActivationDelay)
	{
		ActivateExtractionZones();
		bExtractionsActivated = true;
	}

	float Remaining = MatchDuration - MatchElapsedTime;

	if (!bWarningFired && Remaining <= WarningTime)
	{
		bWarningFired = true;
		OnMatchTimeWarning.Broadcast(Remaining);
	}

	if (Remaining <= 0.f)
	{
		HandleMatchTimeExpired();
	}
}

void ADemoGameMode::ActivateExtractionZones()
{
	if (AllExtractionZones.Num() == 0) return;

	TArray<ADemoExtractionZone*> Shuffled = AllExtractionZones;
	for (int32 i = Shuffled.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		Shuffled.Swap(i, j);
	}

	int32 Count = FMath::Min(ActiveExtractionCount, Shuffled.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		Shuffled[i]->ActivateZone();
	}
}

void ADemoGameMode::HandlePlayerExtracted(ADemoExtractionZone* Zone, AActor* Player)
{
	if (!Player) return;

	bMatchActive = false;

	int32 TotalValue = 0;
	if (UDemoInventoryComponent* Inv = Player->FindComponentByClass<UDemoInventoryComponent>())
	{
		TotalValue = Inv->GetTotalValue();
	}

	AController* PC = nullptr;
	if (APawn* Pawn = Cast<APawn>(Player))
	{
		PC = Pawn->GetController();
	}

	OnPlayerExtractionComplete.Broadcast(PC, TotalValue);

	if (ADemoCharacter* DemoChar = Cast<ADemoCharacter>(Player))
	{
		if (APlayerController* PlayerPC = Cast<APlayerController>(PC))
		{
			DemoChar->GetCharacterMovement()->StopMovementImmediately();
			DemoChar->GetCharacterMovement()->DisableMovement();
		}
	}
}

void ADemoGameMode::HandleMatchTimeExpired()
{
	bMatchActive = false;
	OnMatchTimeExpired.Broadcast();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (ADemoCharacter* DemoChar = Cast<ADemoCharacter>(PC->GetPawn()))
			{
				DemoChar->GetCharacterMovement()->StopMovementImmediately();
				DemoChar->GetCharacterMovement()->DisableMovement();
			}
		}
	}
}

void ADemoGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	ApplyRaidInventoryFromLobbySnapshot(NewPlayer);
}

void ADemoGameMode::ApplyRaidInventoryFromLobbySnapshot(AController* NewPlayer)
{
	UWorld* World = GetWorld();
	APlayerState* PS = NewPlayer->GetPlayerState<APlayerState>();
	if (!World || World->GetNetMode() == NM_Client || !NewPlayer || !PS)
	{
		return;
	}

	UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(World->GetGameInstance());
	if (!GI)
	{
		return;
	}

	const int32 Pid = PS->GetPlayerId();
	if (TArray<FDemoInventorySlot>* Stash = GI->ServerStashSnapshotByPlayerId.Find(Pid))
	{
		if (APawn* Pawn = NewPlayer->GetPawn())
		{
			if (ADemoCharacter* Ch = Cast<ADemoCharacter>(Pawn))
			{
				if (UDemoInventoryComponent* Inv = Ch->FindComponentByClass<UDemoInventoryComponent>())
				{
					for (const FDemoInventorySlot& S : *Stash)
					{
						if (!S.IsEmpty())
						{
							Inv->AddItem(S.ItemID, S.Quantity);
						}
					}
				}
			}
		}
		GI->ServerStashSnapshotByPlayerId.Remove(Pid);
	}
}

void ADemoGameMode::SpawnAIEnemies()
{
	if (!AICharacterClass || !GetWorld()) return;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return;

	FNavLocation NavLoc;
	const int32 NonEliteCount = FMath::Max(0, AICount);
	const int32 EffectiveAssaulterCount = FMath::Clamp(AssaulterAICount, 0, NonEliteCount);
	const int32 EffectiveEasyCount = FMath::Clamp(EasyAICount, 0, NonEliteCount - EffectiveAssaulterCount);
	const int32 TotalAI = NonEliteCount + FMath::Max(0, EliteAICount);

	for (int32 i = 0; i < TotalAI; ++i)
	{
		bool bFound = NavSys->GetRandomReachablePointInRadius(
			AISpawnCenter, AISpawnRadius, NavLoc);

		if (!bFound) continue;

		FVector SpawnLoc = NavLoc.Location + FVector(0, 0, 90.f);
		const FTransform SpawnTransform(
			FRotator(0, FMath::RandRange(0.f, 360.f), 0),
			SpawnLoc);

		EDemoAIDifficulty SpawnDifficulty = EDemoAIDifficulty::Normal;
		EDemoAICombatStyle SpawnCombatStyle = EDemoAICombatStyle::Rifleman;
		if (i >= NonEliteCount)
		{
			SpawnDifficulty = EDemoAIDifficulty::Elite;
			SpawnCombatStyle = EDemoAICombatStyle::Elite;
		}
		else if (i < EffectiveEasyCount)
		{
			SpawnDifficulty = EDemoAIDifficulty::Easy;
		}
		else if (i < EffectiveEasyCount + EffectiveAssaulterCount)
		{
			SpawnDifficulty = EDemoAIDifficulty::Hard;
			SpawnCombatStyle = EDemoAICombatStyle::Assaulter;
		}

		ADemoAICharacter* AI = GetWorld()->SpawnActorDeferred<ADemoAICharacter>(
			AICharacterClass,
			SpawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

		if (AI)
		{
			AI->Difficulty = SpawnDifficulty;
			AI->CombatStyle = SpawnCombatStyle;

			FVector Origin = SpawnLoc;
			AI->PatrolPoints.Empty();
			for (int32 p = 0; p < 4; ++p)
			{
				FNavLocation PatrolNav;
				if (NavSys->GetRandomReachablePointInRadius(Origin, AIPatrolRadius, PatrolNav))
				{
					AI->PatrolPoints.Add(PatrolNav.Location);
				}
			}

			AI->FinishSpawning(SpawnTransform);
			SpawnedAI.Add(AI);
		}
	}
}
