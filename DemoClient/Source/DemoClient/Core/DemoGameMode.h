#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/DemoTypes.h"
#include "DemoGameMode.generated.h"

class ADemoExtractionZone;
class ADemoAICharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchTimeWarning, float, RemainingTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchTimeExpired);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerExtractionComplete, AController*, Player, int32, TotalValue);

UCLASS()
class DEMOCLIENT_API ADemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADemoGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void StartPlay() override;
	virtual FString InitNewPlayer(
		APlayerController* NewPlayerController,
		const FUniqueNetIdRepl& UniqueId,
		const FString& Options,
		const FString& Portal) override;

	UFUNCTION(BlueprintCallable, Category = "Match")
	float GetMatchRemainingTime() const { return FMath::Max(MatchDuration - MatchElapsedTime, 0.f); }

	UFUNCTION(BlueprintCallable, Category = "Match")
	float GetMatchDuration() const { return MatchDuration; }

	UFUNCTION(BlueprintCallable, Category = "Match")
	float GetMatchElapsedTime() const { return MatchElapsedTime; }

	UFUNCTION(BlueprintCallable, Category = "Match")
	bool IsMatchActive() const { return bMatchActive; }

	UFUNCTION(BlueprintCallable, Category = "Match")
	void HandlePlayerExtracted(ADemoExtractionZone* Zone, AActor* Player);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnMatchTimeWarning OnMatchTimeWarning;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnMatchTimeExpired OnMatchTimeExpired;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnPlayerExtractionComplete OnPlayerExtractionComplete;

	UPROPERTY(EditDefaultsOnly, Category = "GameMode")
	float DefaultRespawnDelay = 3.0f;

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Timer")
	float MatchDuration = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Timer")
	float WarningTime = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Extraction")
	int32 ActiveExtractionCount = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Extraction")
	float ExtractionActivationDelay = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|AI")
	TSubclassOf<ADemoAICharacter> AICharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Match|AI")
	int32 AICount = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Match|AI", meta = (ClampMin = "0"))
	int32 EasyAICount = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Match|AI", meta = (ClampMin = "0"))
	int32 AssaulterAICount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Match|AI")
	int32 EliteAICount = 1;

	/** 与 NavMesh 上随机选点配合使用；灰盒关卡常以世界原点为中心时改为 (0,0,100) 并增大半径 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|AI")
	FVector AISpawnCenter = FVector(6600.f, -2265.f, 2505.f);

	UPROPERTY(EditDefaultsOnly, Category = "Match|AI", meta = (ClampMin = "100"))
	float AISpawnRadius = 1200.f;

	/** 每个 AI 巡逻点相对其出生点的随机半径 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|AI", meta = (ClampMin = "50"))
	float AIPatrolRadius = 600.f;

private:
	void ActivateExtractionZones();
	void HandleMatchTimeExpired();
	void SpawnAIEnemies();
	void ApplyRaidInventoryFromLobbySnapshot(AController* NewPlayer);

	float MatchElapsedTime = 0.f;
	bool bMatchActive = false;
	bool bWarningFired = false;
	bool bExtractionsActivated = false;

	UPROPERTY()
	TArray<ADemoExtractionZone*> AllExtractionZones;

	UPROPERTY()
	TArray<ADemoAICharacter*> SpawnedAI;
};
