#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DemoHUD.generated.h"

class ADemoCharacter;
class ADemoGameMode;
struct FDemoWeaponStatConfig;

USTRUCT()
struct FKillNotification
{
	GENERATED_BODY()
	FString Message;
	float Timestamp = 0.f;
};

UCLASS()
class DEMOCLIENT_API ADemoHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADemoHUD();

protected:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	void DrawCrosshair();
	void DrawHitMarker();
	void DrawAmmoCounter();
	void DrawHealthBar();
	void DrawArmorBar();
	void DrawStaminaBar();
	void DrawDamageIndicators();
	void DrawKillFeed();
	void DrawDeathScreen();
	void DrawGrenadeCount();
	void DrawWeaponInfo();
	void DrawMatchTimer();
	void DrawExtractionInfo();
	void DrawInventoryBar();
	void DrawPerformanceOverlay();
	void DrawExtractionResult();
	void DrawInteractionPrompt();
	void DrawContainerPanel();
	void DrawInventoryPanel();
	void DrawWeaponWorkbenchPanel();
	void DrawSpreadPreviewBoard(const FString& Title, const FString& SubTitle, const FDemoWeaponStatConfig& Stats, float X, float Y, float W, float H, const FLinearColor& AccentColor);
	TArray<FVector2D> BuildContinuousFirePreviewSamples(const FDemoWeaponStatConfig& Stats, int32 ShotCount) const;
	void DrawItemSlot(float X, float Y, float W, float H, const FString& Name, int32 Qty, int32 Index, bool bSelected, const FLinearColor& RarityColor);
	void ProcessDedicatedServerSnapshotState();
	void LockLocalPawnForMatchResult();
	void ReturnToLobbyAfterMatchResult();

	UFUNCTION()
	void OnHitConfirmed(AActor* HitActor, const FHitResult& HitResult);

	UFUNCTION()
	void OnKillConfirmed(AActor* KilledActor);

	UFUNCTION()
	void OnDamageTaken(float Amount, AActor* Causer);

	UFUNCTION()
	void OnExtractionComplete(AController* Player, int32 TotalValue);

	UFUNCTION()
	void OnMatchExpired();

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FLinearColor CrosshairColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float CrosshairLength = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float CrosshairGap = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float CrosshairThickness = 2.f;

	ADemoCharacter* GetDemoCharacter() const;
	ADemoGameMode* GetDemoGameMode() const;

	float HitMarkerTimer = 0.f;
	bool bHitMarkerIsKill = false;

	TArray<FKillNotification> KillFeed;

	struct FDamageIndicator
	{
		FVector WorldDirection;
		float Timestamp;
		float DamageAmount;
	};
	TArray<FDamageIndicator> DamageIndicators;

	bool bShowExtractionResult = false;
	bool bExtractionSuccess = false;
	int32 ExtractionTotalValue = 0;
	float ExtractionResultTimer = 0.f;
	int32 ExtractionItemCount = 0;
	int64 LastProcessedDedicatedSnapshotServerTime = -1;
	float SmoothedFPS = 0.f;
	float SmoothedFrameTimeMs = 0.f;
};
