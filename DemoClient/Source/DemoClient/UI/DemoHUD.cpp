#include "UI/DemoHUD.h"
#include "Character/DemoCharacter.h"
#include "Weapon/DemoWeaponBase.h"
#include "Inventory/DemoInventoryComponent.h"
#include "Inventory/DemoLootItem.h"
#include "Inventory/DemoLootContainer.h"
#include "GameModes/DemoExtractionZone.h"
#include "Core/DemoGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Online/DemoDedicatedServerContainerActor.h"
#include "Online/DemoDedicatedServerLootActor.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "Online/DemoOnlineGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/App.h"

ADemoHUD::ADemoHUD() {}

ADemoCharacter* ADemoHUD::GetDemoCharacter() const
{
	if (APlayerController* PC = GetOwningPlayerController())
	{
		return Cast<ADemoCharacter>(PC->GetPawn());
	}
	return nullptr;
}

ADemoGameMode* ADemoHUD::GetDemoGameMode() const
{
	if (UWorld* World = GetWorld())
	{
		return Cast<ADemoGameMode>(World->GetAuthGameMode());
	}
	return nullptr;
}

void ADemoHUD::BeginPlay()
{
	Super::BeginPlay();

	if (ADemoCharacter* Ch = GetDemoCharacter())
	{
		Ch->OnHitConfirmed.AddDynamic(this, &ADemoHUD::OnHitConfirmed);
		Ch->OnKillConfirmed.AddDynamic(this, &ADemoHUD::OnKillConfirmed);
		Ch->OnDamageTaken.AddDynamic(this, &ADemoHUD::OnDamageTaken);
	}

	if (ADemoGameMode* GM = GetDemoGameMode())
	{
		GM->OnPlayerExtractionComplete.AddDynamic(this, &ADemoHUD::OnExtractionComplete);
		GM->OnMatchTimeExpired.AddDynamic(this, &ADemoHUD::OnMatchExpired);
	}
}

void ADemoHUD::OnHitConfirmed(AActor* HitActor, const FHitResult& HitResult)
{
	HitMarkerTimer = 0.3f;
	bHitMarkerIsKill = false;
}

void ADemoHUD::OnKillConfirmed(AActor* KilledActor)
{
	HitMarkerTimer = 0.6f;
	bHitMarkerIsKill = true;

	FKillNotification Notif;
	Notif.Message = FString::Printf(TEXT("ELIMINATED"));
	Notif.Timestamp = GetWorld()->GetTimeSeconds();
	KillFeed.Add(Notif);
}

void ADemoHUD::OnDamageTaken(float Amount, AActor* Causer)
{
	if (!Causer) return;

	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	FDamageIndicator Indicator;
	Indicator.WorldDirection = (Causer->GetActorLocation() - Ch->GetActorLocation()).GetSafeNormal();
	Indicator.Timestamp = GetWorld()->GetTimeSeconds();
	Indicator.DamageAmount = Amount;
	DamageIndicators.Add(Indicator);
}

void ADemoHUD::OnExtractionComplete(AController* Player, int32 TotalValue)
{
	APlayerController* MyPC = GetOwningPlayerController();
	if (!MyPC || Player != MyPC) return;

	bShowExtractionResult = true;
	bExtractionSuccess = true;
	ExtractionTotalValue = TotalValue;
	ExtractionResultTimer = 0.f;

	if (UDemoInventoryComponent* Inv = MyPC->GetPawn() ? MyPC->GetPawn()->FindComponentByClass<UDemoInventoryComponent>() : nullptr)
	{
		ExtractionItemCount = Inv->GetAllItems().Num();
	}
}

void ADemoHUD::OnMatchExpired()
{
	bShowExtractionResult = true;
	bExtractionSuccess = false;
	ExtractionTotalValue = 0;
	ExtractionResultTimer = 0.f;
	ExtractionItemCount = 0;
}

void ADemoHUD::ProcessDedicatedServerSnapshotState()
{
	const UDemoOnlineGameInstance* GI = GetWorld() ? Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!GI || !GI->ShouldUseDedicatedServer() || !GetGameInstance())
	{
		return;
	}

	const UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
	if (!DedicatedServer)
	{
		return;
	}

	const FDemoDedicatedServerSnapshot& Snapshot = DedicatedServer->GetLastSnapshot();
	if (Snapshot.ServerTime <= 0 || Snapshot.ServerTime == LastProcessedDedicatedSnapshotServerTime)
	{
		return;
	}

	LastProcessedDedicatedSnapshotServerTime = Snapshot.ServerTime;

	const FString LocalPlayerId = DedicatedServer->GetPlayerId();
	bool bShouldShowMatchExpired = false;

	for (const FDemoDedicatedServerSnapshotEvent& SnapshotEvent : Snapshot.Events)
	{
		if (SnapshotEvent.Type.Equals(TEXT("player_extracted"), ESearchCase::IgnoreCase) &&
			SnapshotEvent.PlayerId == LocalPlayerId)
		{
			bShowExtractionResult = true;
			bExtractionSuccess = true;
			ExtractionTotalValue = SnapshotEvent.TotalValue;
			ExtractionItemCount = SnapshotEvent.ItemCount;
			ExtractionResultTimer = 0.f;
			LockLocalPawnForMatchResult();
			return;
		}

		if (SnapshotEvent.Type.Equals(TEXT("match_expired"), ESearchCase::IgnoreCase))
		{
			bShouldShowMatchExpired = true;
		}
	}

	if (bShouldShowMatchExpired && !bShowExtractionResult)
	{
		bShowExtractionResult = true;
		bExtractionSuccess = false;
		ExtractionTotalValue = 0;
		ExtractionItemCount = 0;
		ExtractionResultTimer = 0.f;
		LockLocalPawnForMatchResult();
	}
}

void ADemoHUD::LockLocalPawnForMatchResult()
{
	if (ADemoCharacter* Ch = GetDemoCharacter())
	{
		if (UCharacterMovementComponent* Movement = Ch->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
	}
}

void ADemoHUD::ReturnToLobbyAfterMatchResult()
{
	if (const UDemoOnlineGameInstance* GI = GetWorld() ? Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (GI->ShouldUseDedicatedServer())
		{
			if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
			{
				FString LeaveError;
				DedicatedServer->LeaveMatch(&LeaveError);
				DedicatedServer->Disconnect(TEXT("ReturnToLobby"));
			}

			UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/UI/Maps/MAP_LobbyHub")));
			return;
		}
	}

	if (UWorld* W = GetWorld())
	{
		FString MapName = W->GetMapName();
		MapName.RemoveFromStart(W->StreamingLevelsPrefix);
		UGameplayStatics::OpenLevel(W, FName(*MapName));
	}
}

void ADemoHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	ProcessDedicatedServerSnapshotState();

	ADemoCharacter* Ch = GetDemoCharacter();

	if (bShowExtractionResult)
	{
		DrawExtractionResult();
		ExtractionResultTimer += DeltaTime;
		if (ExtractionResultTimer > 10.f)
		{
			bShowExtractionResult = false;
			ReturnToLobbyAfterMatchResult();
		}
		return;
	}

	if (Ch && Ch->GetIsDead())
	{
		DrawDeathScreen();
		return;
	}

	DrawCrosshair();
	DrawHitMarker();
	DrawAmmoCounter();
	DrawHealthBar();
	DrawArmorBar();
	DrawStaminaBar();
	DrawDamageIndicators();
	DrawKillFeed();
	DrawGrenadeCount();
	DrawWeaponInfo();
	DrawMatchTimer();
	DrawExtractionInfo();
	DrawInventoryBar();
	DrawPerformanceOverlay();

	if (Ch && Ch->IsContainerOpen())
	{
		DrawContainerPanel();
	}
	else if (Ch && Ch->IsInventoryOpen())
	{
		DrawInventoryPanel();
	}
	else if (Ch && Ch->IsWeaponWorkbenchOpen())
	{
		DrawWeaponWorkbenchPanel();
	}
	else
	{
		DrawInteractionPrompt();
	}

	if (HitMarkerTimer > 0.f) HitMarkerTimer -= DeltaTime;
}

void ADemoHUD::DrawPerformanceOverlay()
{
	if (!Canvas || !GetWorld())
	{
		return;
	}

	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	const float CurrentFPS = DeltaTime > KINDA_SMALL_NUMBER ? 1.0f / DeltaTime : 0.0f;
	const float CurrentFrameTimeMs = DeltaTime * 1000.0f;

	if (SmoothedFPS <= 0.0f)
	{
		SmoothedFPS = CurrentFPS;
		SmoothedFrameTimeMs = CurrentFrameTimeMs;
	}
	else
	{
		SmoothedFPS = FMath::Lerp(SmoothedFPS, CurrentFPS, 0.1f);
		SmoothedFrameTimeMs = FMath::Lerp(SmoothedFrameTimeMs, CurrentFrameTimeMs, 0.1f);
	}

	float PingMs = 0.0f;
	FString PlayerName = TEXT("Player");
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			PingMs = PS->GetPingInMilliseconds();
			if (!PS->GetPlayerName().IsEmpty())
			{
				PlayerName = PS->GetPlayerName();
			}
		}
	}

	const FString FocusText = FApp::HasFocus() ? TEXT("FOCUS") : TEXT("BACKGROUND");
	const FString PerfText = FString::Printf(
		TEXT("%s | %s | FPS %.1f | Frame %.1f ms | Ping %.0f ms"),
		*PlayerName,
		*FocusText,
		SmoothedFPS,
		SmoothedFrameTimeMs,
		PingMs);

	FCanvasTextItem PerfItem(
		FVector2D(20.0f, 20.0f),
		FText::FromString(PerfText),
		GEngine->GetSmallFont(),
		FApp::HasFocus() ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(1.0f, 0.8f, 0.25f));
	PerfItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(PerfItem);
}

void ADemoHUD::DrawCrosshair()
{
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	float Gap = CrosshairGap;

	if (ADemoCharacter* Ch = GetDemoCharacter())
	{
		if (ADemoWeaponBase* W = Ch->GetCurrentWeapon())
		{
			Gap += W->GetCurrentSpread() * 5.f;
			if (W->GetIsADS()) Gap *= 0.5f;
		}
	}

	const float L = CrosshairLength;
	const FLinearColor C = CrosshairColor;

	DrawLine(CX, CY - Gap - L, CX, CY - Gap, C, CrosshairThickness);
	DrawLine(CX, CY + Gap,     CX, CY + Gap + L, C, CrosshairThickness);
	DrawLine(CX - Gap - L, CY, CX - Gap, CY, C, CrosshairThickness);
	DrawLine(CX + Gap,     CY, CX + Gap + L, CY, C, CrosshairThickness);

	DrawRect(CrosshairColor, CX - 1.f, CY - 1.f, 2.f, 2.f);
}

void ADemoHUD::DrawHitMarker()
{
	if (HitMarkerTimer <= 0.f) return;

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float Size = 12.f;
	const float Offset = 6.f;

	FLinearColor Color = bHitMarkerIsKill
		? FLinearColor(1.f, 0.2f, 0.2f, HitMarkerTimer * 3.f)
		: FLinearColor(1.f, 1.f, 1.f, HitMarkerTimer * 3.f);
	float Thickness = bHitMarkerIsKill ? 3.f : 2.f;

	// Four diagonal lines forming an X
	DrawLine(CX - Offset - Size, CY - Offset - Size, CX - Offset, CY - Offset, Color, Thickness);
	DrawLine(CX + Offset, CY - Offset - Size, CX + Offset + Size, CY - Offset, Color, Thickness);
	DrawLine(CX - Offset - Size, CY + Offset + Size, CX - Offset, CY + Offset, Color, Thickness);
	DrawLine(CX + Offset, CY + Offset + Size, CX + Offset + Size, CY + Offset, Color, Thickness);
}

void ADemoHUD::DrawAmmoCounter()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	const UDemoOnlineGameInstance* GI = GetWorld() ? Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (GI && GI->ShouldUseDedicatedServer() && GetWorld() && GetWorld()->GetGameInstance())
	{
		if (const UDemoDedicatedServerSubsystem* DedicatedServer =
				GetWorld()->GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
		{
			if (const FDemoDedicatedServerPlayerSnapshot* LocalSnapshot = DedicatedServer->GetLocalPlayerSnapshot())
			{
				const float TextX = Canvas->SizeX - 220.f;
				const float TextY = Canvas->SizeY - 80.f;
				const FString AmmoText = FString::Printf(
					TEXT("%d / %d"),
					LocalSnapshot->CurrentAmmo,
					LocalSnapshot->ReserveAmmo);
				FCanvasTextItem AmmoItem(
					FVector2D(TextX, TextY),
					FText::FromString(AmmoText),
					GEngine->GetLargeFont(),
					FLinearColor::White);
				AmmoItem.Scale = FVector2D(1.5f, 1.5f);
				AmmoItem.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem(AmmoItem);

				if (LocalSnapshot->bReloading)
				{
					FCanvasTextItem ReloadItem(
						FVector2D(Canvas->SizeX * 0.5f - 60.f, Canvas->SizeY * 0.5f + 50.f),
						FText::FromString(TEXT("RELOADING...")),
						GEngine->GetMediumFont(),
						FLinearColor::Yellow);
					ReloadItem.EnableShadow(FLinearColor::Black);
					Canvas->DrawItem(ReloadItem);
				}
				return;
			}
		}
	}

	ADemoWeaponBase* W = Ch->GetCurrentWeapon();
	if (!W) return;

	const float TextX = Canvas->SizeX - 220.f;
	const float TextY = Canvas->SizeY - 80.f;

	FString AmmoText = FString::Printf(TEXT("%d / %d"), W->GetCurrentAmmo(), W->GetReserveAmmo());
	FCanvasTextItem AmmoItem(
		FVector2D(TextX, TextY),
		FText::FromString(AmmoText),
		GEngine->GetLargeFont(),
		FLinearColor::White);
	AmmoItem.Scale = FVector2D(1.5f, 1.5f);
	AmmoItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(AmmoItem);

	if (W->GetIsReloading())
	{
		FCanvasTextItem ReloadItem(
			FVector2D(Canvas->SizeX * 0.5f - 60.f, Canvas->SizeY * 0.5f + 50.f),
			FText::FromString(TEXT("RELOADING...")),
			GEngine->GetMediumFont(),
			FLinearColor::Yellow);
		ReloadItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(ReloadItem);
	}
}

void ADemoHUD::DrawHealthBar()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	const float Pct = Ch->GetHealthPercent();
	const float BarX = 30.f;
	const float BarY = Canvas->SizeY - 55.f;
	const float BarW = 200.f;
	const float BarH = 18.f;

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), BarX, BarY, BarW, BarH);

	FLinearColor FillColor = Pct > 0.5f
		? FLinearColor(0.15f, 0.8f, 0.15f)
		: (Pct > 0.25f ? FLinearColor(0.9f, 0.8f, 0.1f) : FLinearColor(0.9f, 0.15f, 0.15f));
	DrawRect(FillColor, BarX, BarY, BarW * Pct, BarH);

	FString Txt = FString::Printf(TEXT("HP  %.0f"), Pct * 100.f);
	FCanvasTextItem Item(FVector2D(BarX + 5.f, BarY + 1.f),
		FText::FromString(Txt), GEngine->GetSmallFont(), FLinearColor::White);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

void ADemoHUD::DrawArmorBar()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	const float Pct = Ch->GetArmorPercent();
	if (Pct <= 0.f) return;

	const float BarX = 30.f;
	const float BarY = Canvas->SizeY - 78.f;
	const float BarW = 200.f;
	const float BarH = 18.f;

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), BarX, BarY, BarW, BarH);
	DrawRect(FLinearColor(0.3f, 0.55f, 0.9f, 0.9f), BarX, BarY, BarW * Pct, BarH);

	FString Txt = FString::Printf(TEXT("AP  %.0f"), Pct * 100.f);
	FCanvasTextItem Item(FVector2D(BarX + 5.f, BarY + 1.f),
		FText::FromString(Txt), GEngine->GetSmallFont(), FLinearColor::White);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

void ADemoHUD::DrawStaminaBar()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	const float Pct = Ch->GetStaminaPercent();
	if (Pct >= 1.f && !Ch->GetIsSprinting()) return;

	const float BarX = 30.f;
	const float BarY = Canvas->SizeY - 98.f;
	const float BarW = 200.f;
	const float BarH = 10.f;

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.6f), BarX, BarY, BarW, BarH);
	DrawRect(FLinearColor(0.2f, 0.6f, 1.0f, 0.8f), BarX, BarY, BarW * Pct, BarH);
}

void ADemoHUD::DrawDamageIndicators()
{
	float Now = GetWorld()->GetTimeSeconds();

	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float IndicatorDist = 120.f;

	for (int32 i = DamageIndicators.Num() - 1; i >= 0; --i)
	{
		float Age = Now - DamageIndicators[i].Timestamp;
		if (Age > 2.0f)
		{
			DamageIndicators.RemoveAt(i);
			continue;
		}

		float Alpha = FMath::Clamp(1.f - (Age / 2.0f), 0.f, 1.f);

		// Convert world direction to screen-relative angle
		FVector Dir = DamageIndicators[i].WorldDirection;
		FRotator PlayerRot = Ch->GetControlRotation();
		FVector Forward = PlayerRot.Vector();
		FVector Right = FRotationMatrix(PlayerRot).GetUnitAxis(EAxis::Y);

		float DotForward = FVector::DotProduct(Dir, Forward);
		float DotRight = FVector::DotProduct(Dir, Right);
		float Angle = FMath::Atan2(DotRight, DotForward);

		float IX = CX + FMath::Sin(Angle) * IndicatorDist;
		float IY = CY - FMath::Cos(Angle) * IndicatorDist;

		FLinearColor IndColor(1.f, 0.1f, 0.1f, Alpha * 0.8f);
		float ArrowSize = 20.f;

		// Draw a small rotated arrow
		float S = FMath::Sin(Angle);
		float C2 = FMath::Cos(Angle);

		FVector2D P1(IX + S * ArrowSize, IY - C2 * ArrowSize);
		FVector2D P2(IX - S * ArrowSize * 0.4f + C2 * ArrowSize * 0.3f,
		             IY + C2 * ArrowSize * 0.4f + S * ArrowSize * 0.3f);
		FVector2D P3(IX - S * ArrowSize * 0.4f - C2 * ArrowSize * 0.3f,
		             IY + C2 * ArrowSize * 0.4f - S * ArrowSize * 0.3f);

		DrawLine(P1.X, P1.Y, P2.X, P2.Y, IndColor, 3.f);
		DrawLine(P1.X, P1.Y, P3.X, P3.Y, IndColor, 3.f);
		DrawLine(P2.X, P2.Y, P3.X, P3.Y, IndColor, 2.f);
	}
}

void ADemoHUD::DrawKillFeed()
{
	float Now = GetWorld()->GetTimeSeconds();

	for (int32 i = KillFeed.Num() - 1; i >= 0; --i)
	{
		if (Now - KillFeed[i].Timestamp > 4.0f)
		{
			KillFeed.RemoveAt(i);
			continue;
		}

		float Alpha = FMath::Clamp(1.f - ((Now - KillFeed[i].Timestamp) / 4.0f), 0.f, 1.f);

		float Y = 100.f + (KillFeed.Num() - 1 - i) * 30.f;
		FCanvasTextItem Item(
			FVector2D(Canvas->SizeX * 0.5f - 60.f, Y),
			FText::FromString(KillFeed[i].Message),
			GEngine->GetMediumFont(),
			FLinearColor(1.f, 0.3f, 0.3f, Alpha));
		Item.EnableShadow(FLinearColor(0, 0, 0, Alpha));
		Canvas->DrawItem(Item);
	}
}

void ADemoHUD::DrawDeathScreen()
{
	DrawRect(FLinearColor(0.15f, 0.f, 0.f, 0.6f), 0, 0, Canvas->SizeX, Canvas->SizeY);

	FCanvasTextItem DeadText(
		FVector2D(Canvas->SizeX * 0.5f - 80.f, Canvas->SizeY * 0.5f - 30.f),
		FText::FromString(TEXT("YOU DIED")),
		GEngine->GetLargeFont(),
		FLinearColor(0.9f, 0.1f, 0.1f));
	DeadText.Scale = FVector2D(2.5f, 2.5f);
	DeadText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(DeadText);

	FCanvasTextItem RespawnText(
		FVector2D(Canvas->SizeX * 0.5f - 80.f, Canvas->SizeY * 0.5f + 40.f),
		FText::FromString(TEXT("Respawning...")),
		GEngine->GetMediumFont(),
		FLinearColor(0.7f, 0.7f, 0.7f));
	RespawnText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(RespawnText);
}

void ADemoHUD::DrawGrenadeCount()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	// Use a simple text display for grenade count
	// Access through a method that would need to be added
	const float X = Canvas->SizeX - 220.f;
	const float Y = Canvas->SizeY - 110.f;

	FCanvasTextItem Item(
		FVector2D(X, Y),
		FText::FromString(TEXT("G")),
		GEngine->GetSmallFont(),
		FLinearColor(0.5f, 0.8f, 0.5f));
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

void ADemoHUD::DrawWeaponInfo()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	ADemoWeaponBase* W = Ch->GetCurrentWeapon();
	if (!W) return;

	const float X = Canvas->SizeX - 220.f;
	const float Y = Canvas->SizeY - 50.f;

	FString WeaponName;
	switch (W->GetWeaponType())
	{
	case EDemoWeaponType::AssaultRifle: WeaponName = TEXT("ASSAULT RIFLE"); break;
	case EDemoWeaponType::SMG:          WeaponName = TEXT("SMG"); break;
	case EDemoWeaponType::SniperRifle:  WeaponName = TEXT("SNIPER"); break;
	case EDemoWeaponType::Shotgun:      WeaponName = TEXT("SHOTGUN"); break;
	case EDemoWeaponType::Pistol:       WeaponName = TEXT("PISTOL"); break;
	default:                            WeaponName = TEXT("WEAPON"); break;
	}

	FCanvasTextItem Item(
		FVector2D(X, Y),
		FText::FromString(WeaponName),
		GEngine->GetSmallFont(),
		FLinearColor(0.7f, 0.7f, 0.7f, 0.8f));
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

void ADemoHUD::DrawMatchTimer()
{
	if (const UDemoOnlineGameInstance* GI = GetWorld() ? Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (GI->ShouldUseDedicatedServer())
		{
			if (const UDemoDedicatedServerSubsystem* DedicatedServer = GetWorld()->GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
			{
				const float CX = Canvas->SizeX * 0.5f;
				const FDemoDedicatedServerSnapshot& Snapshot = DedicatedServer->GetLastSnapshot();
				if (DedicatedServer->IsInMatch())
				{
					const float Remaining = Snapshot.MatchRemainingSeconds;
					const int32 Minutes = FMath::FloorToInt(Remaining / 60.0f);
					const int32 Seconds = FMath::FloorToInt(FMath::Fmod(Remaining, 60.0f));

					FLinearColor TimerColor = FLinearColor::White;
					if (Remaining <= 300.0f)
					{
						TimerColor = FLinearColor(1.0f, 0.5f, 0.0f);
					}
					if (Remaining <= 60.0f || Snapshot.bMatchExpired)
					{
						TimerColor = FLinearColor::Red;
					}

					const FString TimeStr = FString::Printf(TEXT("%02d:%02d"), FMath::Max(Minutes, 0), FMath::Max(Seconds, 0));
					FCanvasTextItem TimerItem(
						FVector2D(CX - 30.0f, 20.0f),
						FText::FromString(TimeStr),
						GEngine->GetMediumFont(),
						TimerColor);
					TimerItem.EnableShadow(FLinearColor::Black);
					TimerItem.Scale = FVector2D(1.5f, 1.5f);
					Canvas->DrawItem(TimerItem);

					const FString StatusText = FString::Printf(TEXT("%s  TICK %d"), *DedicatedServer->GetCurrentMatchId(), Snapshot.Tick);
					FCanvasTextItem StatusItem(
						FVector2D(CX - 145.0f, 52.0f),
						FText::FromString(StatusText),
						GEngine->GetSmallFont(),
						FLinearColor(0.65f, 0.85f, 1.0f));
					StatusItem.EnableShadow(FLinearColor::Black);
					Canvas->DrawItem(StatusItem);

					if (Snapshot.bMatchExpired)
					{
						FCanvasTextItem ExpiredItem(
							FVector2D(CX - 82.0f, 70.0f),
							FText::FromString(TEXT("TIME EXPIRED")),
							GEngine->GetMediumFont(),
							FLinearColor::Red);
						ExpiredItem.EnableShadow(FLinearColor::Black);
						Canvas->DrawItem(ExpiredItem);
					}
				}
				else
				{
					FCanvasTextItem StatusItem(
						FVector2D(CX - 170.0f, 20.0f),
						FText::FromString(TEXT("CONNECTING TO DEDICATED SERVER")),
						GEngine->GetMediumFont(),
						FLinearColor(0.65f, 0.85f, 1.0f));
					StatusItem.EnableShadow(FLinearColor::Black);
					Canvas->DrawItem(StatusItem);
				}
				return;
			}
		}
	}

	ADemoGameMode* GM = GetDemoGameMode();
	if (!GM) return;

	float Remaining = GM->GetMatchRemainingTime();
	int32 Minutes = FMath::FloorToInt(Remaining / 60.f);
	int32 Seconds = FMath::FloorToInt(FMath::Fmod(Remaining, 60.f));

	FLinearColor TimerColor = FLinearColor::White;
	if (Remaining <= 300.f) TimerColor = FLinearColor(1.f, 0.5f, 0.f);
	if (Remaining <= 60.f)  TimerColor = FLinearColor::Red;

	FString TimeStr = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);

	float CX = Canvas->SizeX * 0.5f;
	FCanvasTextItem TimerItem(
		FVector2D(CX - 30.f, 20.f),
		FText::FromString(TimeStr),
		GEngine->GetMediumFont(),
		TimerColor);
	TimerItem.EnableShadow(FLinearColor::Black);
	TimerItem.Scale = FVector2D(1.5f, 1.5f);
	Canvas->DrawItem(TimerItem);

	if (!GM->IsMatchActive())
	{
		FCanvasTextItem ExpiredItem(
			FVector2D(CX - 80.f, 50.f),
			FText::FromString(TEXT("TIME EXPIRED")),
			GEngine->GetMediumFont(),
			FLinearColor::Red);
		ExpiredItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(ExpiredItem);
	}
}

void ADemoHUD::DrawExtractionInfo()
{
	if (!GetWorld()) return;

	if (const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (GI->ShouldUseDedicatedServer())
		{
			if (const UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
			{
				float Y = 80.f;
				const FString LocalPlayerId = DedicatedServer->GetPlayerId();

				for (const FDemoDedicatedServerExtractionZoneSnapshot& Zone : DedicatedServer->GetLastSnapshot().ExtractionZones)
				{
					if (Zone.State == EDemoExtractionState::Inactive)
					{
						continue;
					}

					FLinearColor ZoneColor = FLinearColor::Green;
					FString StatusStr;

					switch (Zone.State)
					{
					case EDemoExtractionState::Available:
						StatusStr = TEXT("OPEN");
						ZoneColor = FLinearColor::Green;
						break;
					case EDemoExtractionState::Extracting:
						StatusStr = FString::Printf(TEXT("EXTRACTING %.0f%%"), Zone.Progress * 100.f);
						ZoneColor = FLinearColor(1.f, 0.8f, 0.f);

						if (Zone.ExtractingPlayerId == LocalPlayerId)
						{
							const float CX = Canvas->SizeX * 0.5f;
							const float CY = Canvas->SizeY * 0.5f;
							const float BarW = 200.f;
							const float BarH = 8.f;
							const float BarX = CX - BarW * 0.5f;
							const float BarY = CY + 80.f;

							DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f), BarX, BarY, BarW, BarH);
							DrawRect(FLinearColor(0.f, 1.f, 0.5f, 0.9f), BarX, BarY, BarW * Zone.Progress, BarH);

							FCanvasTextItem ProgItem(
								FVector2D(BarX, BarY - 20.f),
								FText::FromString(TEXT("HOLD POSITION...")),
								GEngine->GetSmallFont(),
								FLinearColor(0.f, 1.f, 0.5f));
							ProgItem.EnableShadow(FLinearColor::Black);
							Canvas->DrawItem(ProgItem);
						}
						break;
					case EDemoExtractionState::Completed:
						StatusStr = TEXT("EXTRACTED");
						ZoneColor = FLinearColor(0.5f, 0.5f, 0.5f);
						break;
					default:
						continue;
					}

					const FString ZoneName = Zone.DisplayName.IsEmpty() ? Zone.ZoneId : Zone.DisplayName;
					const FString ZoneStr = FString::Printf(TEXT("[%s] %s"), *ZoneName, *StatusStr);
					FCanvasTextItem ZoneItem(
						FVector2D(Canvas->SizeX - 280.f, Y),
						FText::FromString(ZoneStr),
						GEngine->GetSmallFont(),
						ZoneColor);
					ZoneItem.EnableShadow(FLinearColor::Black);
					Canvas->DrawItem(ZoneItem);
					Y += 20.f;
				}
			}
			return;
		}
	}

	float Y = 80.f;
	for (TActorIterator<ADemoExtractionZone> It(GetWorld()); It; ++It)
	{
		ADemoExtractionZone* Zone = *It;
		if (Zone->GetState() == EDemoExtractionState::Inactive) continue;

		FLinearColor ZoneColor = FLinearColor::Green;
		FString StatusStr;

		switch (Zone->GetState())
		{
		case EDemoExtractionState::Available:
			StatusStr = TEXT("OPEN");
			ZoneColor = FLinearColor::Green;
			break;
		case EDemoExtractionState::Extracting:
		{
			float Pct = Zone->GetExtractionProgress() * 100.f;
			StatusStr = FString::Printf(TEXT("EXTRACTING %.0f%%"), Pct);
			ZoneColor = FLinearColor(1.f, 0.8f, 0.f);

			float CX = Canvas->SizeX * 0.5f;
			float CY = Canvas->SizeY * 0.5f;
			float BarW = 200.f, BarH = 8.f;
			float BarX = CX - BarW * 0.5f;
			float BarY = CY + 80.f;

			DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f), BarX, BarY, BarW, BarH);
			DrawRect(FLinearColor(0.f, 1.f, 0.5f, 0.9f), BarX, BarY, BarW * Zone->GetExtractionProgress(), BarH);

			FCanvasTextItem ProgItem(
				FVector2D(BarX, BarY - 20.f),
				FText::FromString(TEXT("HOLD POSITION...")),
				GEngine->GetSmallFont(),
				FLinearColor(0.f, 1.f, 0.5f));
			ProgItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(ProgItem);
			break;
		}
		case EDemoExtractionState::Completed:
			StatusStr = TEXT("EXTRACTED");
			ZoneColor = FLinearColor(0.5f, 0.5f, 0.5f);
			break;
		default:
			continue;
		}

		FString ZoneStr = FString::Printf(TEXT("[%s] %s"), *Zone->GetZoneName().ToString(), *StatusStr);
		FCanvasTextItem ZoneItem(
			FVector2D(Canvas->SizeX - 280.f, Y),
			FText::FromString(ZoneStr),
			GEngine->GetSmallFont(),
			ZoneColor);
		ZoneItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(ZoneItem);
		Y += 20.f;
	}
}

void ADemoHUD::DrawInventoryBar()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	UDemoInventoryComponent* Inv = Ch->FindComponentByClass<UDemoInventoryComponent>();
	if (!Inv) return;

	float X = 20.f;
	float Y = Canvas->SizeY - 70.f;

	int32 ItemCount = Inv->GetAllItems().Num();
	float Weight = Inv->GetCurrentWeight();
	float MaxW = Inv->GetMaxWeight();
	int32 Value = Inv->GetTotalValue();

	FLinearColor WeightColor = (Weight / MaxW > 0.8f) ? FLinearColor(1.f, 0.3f, 0.f) : FLinearColor(0.8f, 0.8f, 0.8f);

	FString InvStr = FString::Printf(TEXT("ITEMS: %d  |  %.1f/%.0f kg  |  $%d"), ItemCount, Weight, MaxW, Value);
	FCanvasTextItem InvItem(
		FVector2D(X, Y),
		FText::FromString(InvStr),
		GEngine->GetSmallFont(),
		WeightColor);
	InvItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(InvItem);
}

void ADemoHUD::DrawExtractionResult()
{
	float CX = Canvas->SizeX * 0.5f;
	float CY = Canvas->SizeY * 0.5f;

	float FadeIn = FMath::Clamp(ExtractionResultTimer / 0.5f, 0.f, 1.f);

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f * FadeIn), 0, 0, Canvas->SizeX, Canvas->SizeY);

	float PanelW = 420.f;
	float PanelH = 280.f;
	float PX = CX - PanelW * 0.5f;
	float PY = CY - PanelH * 0.5f;

	DrawRect(FLinearColor(0.03f, 0.03f, 0.06f, 0.95f * FadeIn), PX, PY, PanelW, PanelH);

	FLinearColor AccentColor = bExtractionSuccess ? FLinearColor(0.2f, 0.9f, 0.3f) : FLinearColor(0.9f, 0.2f, 0.2f);
	DrawRect(AccentColor * FadeIn, PX, PY, PanelW, 3.f);

	FString TitleStr = bExtractionSuccess ? TEXT("EXTRACTION SUCCESSFUL") : TEXT("TIME EXPIRED - MIA");
	{
		FCanvasTextItem TitleItem(FVector2D(PX + 30.f, PY + 20.f),
			FText::FromString(TitleStr), GEngine->GetMediumFont(), AccentColor * FadeIn);
		TitleItem.EnableShadow(FLinearColor::Black);
		TitleItem.Scale = FVector2D(1.1f, 1.1f);
		Canvas->DrawItem(TitleItem);
	}

	DrawRect(FLinearColor(0.3f, 0.3f, 0.3f, 0.4f * FadeIn), PX + 20.f, PY + 60.f, PanelW - 40.f, 1.f);

	float Y = PY + 75.f;
	float LabelX = PX + 30.f;
	float ValueX = PX + PanelW - 140.f;
	FLinearColor LabelColor = FLinearColor(0.6f, 0.6f, 0.6f) * FadeIn;
	FLinearColor ValueColor = FLinearColor::White * FadeIn;

	auto DrawRow = [&](const FString& Label, const FString& Val, const FLinearColor& ValCol)
	{
		FCanvasTextItem LItem(FVector2D(LabelX, Y), FText::FromString(Label),
			GEngine->GetSmallFont(), LabelColor);
		LItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(LItem);

		FCanvasTextItem VItem(FVector2D(ValueX, Y), FText::FromString(Val),
			GEngine->GetSmallFont(), ValCol * FadeIn);
		VItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(VItem);
		Y += 28.f;
	};

	FString StatusStr = bExtractionSuccess ? TEXT("Extracted") : TEXT("Lost");
	DrawRow(TEXT("Status"), StatusStr, AccentColor);

	DrawRow(TEXT("Items Extracted"), FString::Printf(TEXT("%d"), ExtractionItemCount), ValueColor);

	FLinearColor GoldColor = FLinearColor(1.f, 0.85f, 0.f);
	DrawRow(TEXT("Total Value"), FString::Printf(TEXT("$%d"), ExtractionTotalValue), GoldColor);

	if (bExtractionSuccess)
	{
		FString RatingStr;
		if (ExtractionTotalValue >= 5000) RatingStr = TEXT("S");
		else if (ExtractionTotalValue >= 2000) RatingStr = TEXT("A");
		else if (ExtractionTotalValue >= 500) RatingStr = TEXT("B");
		else RatingStr = TEXT("C");
		DrawRow(TEXT("Rating"), RatingStr, GoldColor);
	}

	DrawRect(FLinearColor(0.3f, 0.3f, 0.3f, 0.4f * FadeIn), PX + 20.f, Y + 5.f, PanelW - 40.f, 1.f);

	float RestartCountdown = 10.f - ExtractionResultTimer;
	if (RestartCountdown > 0.f)
	{
		const bool bDedicatedPath =
			GetWorld() &&
			Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) &&
			Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance())->ShouldUseDedicatedServer();
		FString HintStr;
		if (bDedicatedPath)
		{
			HintStr = FString::Printf(TEXT("Returning to lobby in %.0f..."), FMath::Max(RestartCountdown, 0.f));
		}
		else
		{
			HintStr = FString::Printf(TEXT("Restarting in %.0f..."), FMath::Max(RestartCountdown, 0.f));
		}
		FCanvasTextItem HItem(FVector2D(PX + 30.f, PY + PanelH - 35.f),
			FText::FromString(HintStr), GEngine->GetSmallFont(),
			FLinearColor(0.5f, 0.5f, 0.5f) * FadeIn);
		HItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(HItem);
	}
}

void ADemoHUD::DrawInteractionPrompt()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	APlayerController* PC = Cast<APlayerController>(Ch->GetController());
	if (!PC) return;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ch);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, CamLoc, CamLoc + CamRot.Vector() * 300.f, ECC_Visibility, Params);

	if (!bHit || !Hit.GetActor()) return;

	FString PromptStr;

	if (const UDemoOnlineGameInstance* GI = GetWorld() ? Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (GI->ShouldUseDedicatedServer())
		{
			if (const ADemoDedicatedServerLootActor* DedicatedLoot = Cast<ADemoDedicatedServerLootActor>(Hit.GetActor()))
			{
				FString ItemName = DedicatedLoot->GetItemId().ToString();
				if (UDemoInventoryComponent* Inv = Ch->FindComponentByClass<UDemoInventoryComponent>())
				{
					FDemoItemData ItemData;
					if (Inv->FindItemData(DedicatedLoot->GetItemId(), ItemData) && !ItemData.DisplayName.IsEmpty())
					{
						ItemName = ItemData.DisplayName.ToString();
					}
				}

				PromptStr = FString::Printf(TEXT("[F] Pick up %s x%d"), *ItemName, DedicatedLoot->GetQuantity());
			}
			else
			if (const ADemoDedicatedServerContainerActor* DedicatedContainer =
					Cast<ADemoDedicatedServerContainerActor>(Hit.GetActor()))
			{
				if (!DedicatedContainer->IsOpened())
				{
					PromptStr = TEXT("[F] Search");
				}
				else if (DedicatedContainer->GetRemainingItemCount() > 0)
				{
					PromptStr = FString::Printf(
						TEXT("[F] Open container (%d remaining)"),
						DedicatedContainer->GetRemainingItemCount());
				}
				else
				{
					PromptStr = TEXT("[F] Open empty container");
				}
			}
		}
	}

	if (PromptStr.IsEmpty())
	{
		if (ADemoLootItem* LootItem = Cast<ADemoLootItem>(Hit.GetActor()))
		{
			PromptStr = FString::Printf(TEXT("[F] Pick up %s"), *LootItem->GetDisplayName().ToString());
		}
		else if (ADemoLootContainer* Container = Cast<ADemoLootContainer>(Hit.GetActor()))
		{
			if (!Container->IsOpened())
			{
				if (Container->IsBeingSearched())
				{
					float Pct = Container->GetSearchProgress() * 100.f;
					PromptStr = FString::Printf(TEXT("Searching... %.0f%%"), Pct);
				}
				else
				{
					PromptStr = TEXT("[F] Search");
				}
			}
			else
			{
				int32 Count = 0;
				for (const auto& Slot : Container->GetContents())
				{
					if (!Slot.IsEmpty()) Count++;
				}
				if (Count > 0)
					PromptStr = FString::Printf(TEXT("[F] Take item (%d remaining)"), Count);
			}
		}
	}

	if (PromptStr.IsEmpty()) return;

	float CX = Canvas->SizeX * 0.5f;
	float CY = Canvas->SizeY * 0.5f;

	FCanvasTextItem PromptItem(
		FVector2D(CX - 80.f, CY + 40.f),
		FText::FromString(PromptStr),
		GEngine->GetSmallFont(),
		FLinearColor(1.f, 1.f, 1.f, 0.9f));
	PromptItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(PromptItem);
}

TArray<FVector2D> ADemoHUD::BuildContinuousFirePreviewSamples(const FDemoWeaponStatConfig& Stats, int32 ShotCount) const
{
	TArray<FVector2D> Samples;
	if (ShotCount <= 0)
	{
		return Samples;
	}

	Samples.Reserve(ShotCount);

	for (int32 ShotIndex = 0; ShotIndex < ShotCount; ++ShotIndex)
	{
		Samples.Add(ADemoWeaponBase::ComputeSprayPatternOffset(
			Stats,
			ShotIndex,
			FMath::Max(0.1f, Stats.HipFireSpreadMultiplier),
			1.0f));
	}

	return Samples;
}

void ADemoHUD::DrawSpreadPreviewBoard(
	const FString& Title,
	const FString& SubTitle,
	const FDemoWeaponStatConfig& Stats,
	float X,
	float Y,
	float W,
	float H,
	const FLinearColor& AccentColor)
{
	if (!Canvas)
	{
		return;
	}

	DrawRect(FLinearColor(0.07f, 0.09f, 0.13f, 0.95f), X, Y, W, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.9f), X, Y, W, 2.0f);

	FCanvasTextItem TitleItem(
		FVector2D(X + 14.0f, Y + 10.0f),
		FText::FromString(Title),
		GEngine->GetSmallFont(),
		FLinearColor::White);
	TitleItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(TitleItem);

	FCanvasTextItem SubTitleItem(
		FVector2D(X + 14.0f, Y + 28.0f),
		FText::FromString(SubTitle),
		GEngine->GetSmallFont(),
		FLinearColor(0.67f, 0.72f, 0.78f));
	SubTitleItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(SubTitleItem);

	const float PlotX = X + 14.0f;
	const float PlotY = Y + 50.0f;
	const float PlotW = W - 28.0f;
	const float PlotH = H - 92.0f;
	const float PlotOriginX = PlotX + PlotW * 0.5f;
	const float PlotOriginY = PlotY + PlotH * 0.78f;

	DrawRect(FLinearColor(0.03f, 0.04f, 0.06f, 0.92f), PlotX, PlotY, PlotW, PlotH);
	DrawRect(FLinearColor(0.22f, 0.24f, 0.29f, 0.8f), PlotX, PlotY, PlotW, 1.0f);
	DrawLine(PlotOriginX, PlotY, PlotOriginX, PlotY + PlotH, FLinearColor(0.18f, 0.22f, 0.28f, 0.85f), 1.0f);
	DrawLine(PlotX, PlotOriginY, PlotX + PlotW, PlotOriginY, FLinearColor(0.18f, 0.22f, 0.28f, 0.85f), 1.0f);
	DrawLine(PlotX + PlotW * 0.25f, PlotY, PlotX + PlotW * 0.25f, PlotY + PlotH, FLinearColor(0.10f, 0.12f, 0.16f, 0.75f), 1.0f);
	DrawLine(PlotX + PlotW * 0.75f, PlotY, PlotX + PlotW * 0.75f, PlotY + PlotH, FLinearColor(0.10f, 0.12f, 0.16f, 0.75f), 1.0f);
	DrawLine(PlotX, PlotY + PlotH * 0.25f, PlotX + PlotW, PlotY + PlotH * 0.25f, FLinearColor(0.10f, 0.12f, 0.16f, 0.75f), 1.0f);
	DrawLine(PlotX, PlotY + PlotH * 0.75f, PlotX + PlotW, PlotY + PlotH * 0.75f, FLinearColor(0.10f, 0.12f, 0.16f, 0.75f), 1.0f);

	const TArray<FVector2D> Samples = BuildContinuousFirePreviewSamples(Stats, 18);
	float MaxExtent = 1.0f;
	for (const FVector2D& Sample : Samples)
	{
		MaxExtent = FMath::Max(MaxExtent, FMath::Abs(Sample.X));
		MaxExtent = FMath::Max(MaxExtent, FMath::Abs(Sample.Y));
	}

	const float PlotScale = (FMath::Min(PlotW, PlotH) * 0.36f) / MaxExtent;
	bool bHasPreviousPoint = false;
	FVector2D PreviousPoint = FVector2D::ZeroVector;

	for (int32 ShotIndex = 0; ShotIndex < Samples.Num(); ++ShotIndex)
	{
		const float BlendAlpha = Samples.Num() > 1 ? static_cast<float>(ShotIndex) / static_cast<float>(Samples.Num() - 1) : 1.0f;
		const FVector2D ScreenPoint(
			PlotOriginX + Samples[ShotIndex].X * PlotScale,
			PlotOriginY + Samples[ShotIndex].Y * PlotScale);

		if (bHasPreviousPoint)
		{
			DrawLine(
				PreviousPoint.X,
				PreviousPoint.Y,
				ScreenPoint.X,
				ScreenPoint.Y,
				FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.35f + 0.45f * BlendAlpha),
				1.5f);
		}

		const float MarkerSize = ShotIndex == Samples.Num() - 1 ? 7.0f : 5.0f;
		const FLinearColor MarkerColor = FLinearColor(
			FMath::Lerp(AccentColor.R, 1.0f, 0.25f),
			FMath::Lerp(AccentColor.G, 1.0f, 0.25f),
			FMath::Lerp(AccentColor.B, 1.0f, 0.25f),
			0.95f);
		DrawRect(MarkerColor, ScreenPoint.X - MarkerSize * 0.5f, ScreenPoint.Y - MarkerSize * 0.5f, MarkerSize, MarkerSize);

		if (ShotIndex == 0 || ShotIndex == Samples.Num() - 1)
		{
			const FString Label = ShotIndex == 0 ? TEXT("1") : FString::Printf(TEXT("%d"), ShotIndex + 1);
			FCanvasTextItem ShotLabel(
				FVector2D(ScreenPoint.X + 6.0f, ScreenPoint.Y - 10.0f),
				FText::FromString(Label),
				GEngine->GetSmallFont(),
				MarkerColor);
			ShotLabel.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(ShotLabel);
		}

		PreviousPoint = ScreenPoint;
		bHasPreviousPoint = true;
	}

	float MaxHorizontalDrift = 0.0f;
	float VerticalClimb = 0.0f;
	if (Samples.Num() > 0)
	{
		for (const FVector2D& Sample : Samples)
		{
			MaxHorizontalDrift = FMath::Max(MaxHorizontalDrift, FMath::Abs(Sample.X));
		}
		VerticalClimb = FMath::Abs(Samples.Last().Y - Samples[0].Y);
	}

	const FString SummaryText = FString::Printf(
		TEXT("18-shot fixed spray | horizontal %.2f | vertical %.2f | base %.2f | per-shot %.2f"),
		MaxHorizontalDrift,
		VerticalClimb,
		Stats.BaseSpread * Stats.HipFireSpreadMultiplier,
		Stats.SpreadPerShot * Stats.HipFireSpreadMultiplier);
	FCanvasTextItem SummaryItem(
		FVector2D(X + 14.0f, Y + H - 28.0f),
		FText::FromString(SummaryText),
		GEngine->GetSmallFont(),
		FLinearColor(0.70f, 0.74f, 0.80f));
	SummaryItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(SummaryItem);
}

void ADemoHUD::DrawWeaponWorkbenchPanel()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch || !Canvas)
	{
		return;
	}

	ADemoWeaponBase* Weapon = Ch->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	const FDemoWeaponStatConfig& BaseStats = Weapon->GetBaseWeaponStats();
	const FDemoWeaponStatConfig& EffectiveStats = Weapon->GetEffectiveWeaponStats();

	const float PanelW = FMath::Min(Canvas->SizeX - 60.0f, 1120.0f);
	const float PanelH = FMath::Min(Canvas->SizeY - 60.0f, 670.0f);
	const float PX = (Canvas->SizeX - PanelW) * 0.5f;
	const float PY = (Canvas->SizeY - PanelH) * 0.5f;
	const float InnerPadding = 20.0f;
	const float TopSectionY = PY + 84.0f;
	const float AttachmentAreaW = 320.0f;
	const float StatsAreaX = PX + InnerPadding + AttachmentAreaW + 20.0f;
	const float StatsAreaW = PanelW - (StatsAreaX - PX) - InnerPadding;
	const float PreviewY = PY + 330.0f;
	const float PreviewGap = 18.0f;
	const float PreviewW = (PanelW - InnerPadding * 2.0f - PreviewGap) * 0.5f;
	const float PreviewH = PanelH - (PreviewY - PY) - 56.0f;

	DrawRect(FLinearColor(0.03f, 0.04f, 0.07f, 0.95f), PX, PY, PanelW, PanelH);
	DrawRect(FLinearColor(0.85f, 0.55f, 0.15f, 0.9f), PX, PY, PanelW, 3.f);

	FCanvasTextItem Title(
		FVector2D(PX + 18.f, PY + 14.f),
		FText::FromString(TEXT("WEAPON WORKBENCH")),
		GEngine->GetMediumFont(),
		FLinearColor(1.0f, 0.85f, 0.4f));
	Title.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Title);

	FCanvasTextItem SubTitle(
		FVector2D(PX + 18.f, PY + 42.f),
		FText::FromString(TEXT("Runtime attachment tuning: visuals stay unchanged, while stats and spread preview update immediately")),
		GEngine->GetSmallFont(),
		FLinearColor(0.7f, 0.75f, 0.8f));
	SubTitle.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(SubTitle);

	FCanvasTextItem HotkeyHint(
		FVector2D(PX + 18.f, PY + 60.f),
		FText::FromString(TEXT("[B] Open / Close  [F1-F4] Toggle Attachment  [F5] Reset Build")),
		GEngine->GetSmallFont(),
		FLinearColor(0.95f, 0.84f, 0.58f));
	HotkeyHint.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(HotkeyHint);

	auto DrawButtonRow = [&](const int32 RowIndex, const EDemoWeaponAttachmentSlot Slot, const FString& Hotkey)
	{
		const bool bEquipped = Weapon->IsAttachmentEquipped(Slot);
		const float ButtonX = PX + InnerPadding;
		const float ButtonY = TopSectionY + RowIndex * 64.f;
		const float ButtonW = AttachmentAreaW;
		const float ButtonH = 52.f;
		const FLinearColor ButtonColor = bEquipped
			? FLinearColor(0.18f, 0.55f, 0.28f, 0.95f)
			: FLinearColor(0.16f, 0.18f, 0.24f, 0.95f);

		DrawRect(ButtonColor, ButtonX, ButtonY, ButtonW, ButtonH);
		DrawRect(FLinearColor(0.45f, 0.48f, 0.55f, 0.7f), ButtonX, ButtonY, ButtonW, 1.f);

		const FString AttachmentTitle = FString::Printf(
			TEXT("[%s] %s  %s"),
			*Hotkey,
			*Weapon->GetAttachmentDisplayName(Slot).ToString(),
			bEquipped ? TEXT("(EQUIPPED)") : TEXT("(OFF)"));

		FCanvasTextItem ButtonTitle(
			FVector2D(ButtonX + 10.f, ButtonY + 8.f),
			FText::FromString(AttachmentTitle),
			GEngine->GetSmallFont(),
			FLinearColor::White);
		ButtonTitle.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(ButtonTitle);

		FCanvasTextItem Desc(
			FVector2D(ButtonX + 10.f, ButtonY + 27.f),
			FText::FromString(Weapon->GetAttachmentEffectDescription(Slot)),
			GEngine->GetSmallFont(),
			FLinearColor(0.68f, 0.72f, 0.78f));
		Desc.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Desc);
	};

	DrawButtonRow(0, EDemoWeaponAttachmentSlot::Muzzle, TEXT("F1"));
	DrawButtonRow(1, EDemoWeaponAttachmentSlot::Grip, TEXT("F2"));
	DrawButtonRow(2, EDemoWeaponAttachmentSlot::Magazine, TEXT("F3"));
	DrawButtonRow(3, EDemoWeaponAttachmentSlot::Optic, TEXT("F4"));

	FCanvasTextItem AttachmentTitle(
		FVector2D(PX + InnerPadding, TopSectionY - 24.0f),
		FText::FromString(TEXT("ATTACHMENTS")),
		GEngine->GetSmallFont(),
		FLinearColor(0.95f, 0.88f, 0.55f));
	AttachmentTitle.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(AttachmentTitle);

	DrawRect(FLinearColor(0.18f, 0.20f, 0.24f, 0.85f), StatsAreaX, TopSectionY - 4.0f, StatsAreaW, 260.0f);
	DrawRect(FLinearColor(0.95f, 0.88f, 0.55f, 0.7f), StatsAreaX, TopSectionY - 4.0f, StatsAreaW, 1.0f);

	const float StatsX = StatsAreaX + 14.0f;
	float StatsY = TopSectionY + 18.0f;

	FCanvasTextItem StatsTitle(
		FVector2D(StatsX, TopSectionY - 28.0f),
		FText::FromString(TEXT("BASELINE VS CURRENT BUILD")),
		GEngine->GetSmallFont(),
		FLinearColor(0.95f, 0.88f, 0.55f));
	StatsTitle.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(StatsTitle);

	FCanvasTextItem BaseColumn(
		FVector2D(StatsX + 140.0f, TopSectionY - 2.0f),
		FText::FromString(TEXT("BASE")),
		GEngine->GetSmallFont(),
		FLinearColor(0.55f, 0.58f, 0.64f));
	BaseColumn.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(BaseColumn);

	FCanvasTextItem CurrentColumn(
		FVector2D(StatsX + 235.0f, TopSectionY - 2.0f),
		FText::FromString(TEXT("CURRENT")),
		GEngine->GetSmallFont(),
		FLinearColor(0.88f, 0.91f, 0.96f));
	CurrentColumn.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(CurrentColumn);

	auto DrawStatRow = [&](const FString& Label, const FString& BaseValue, const FString& EffectiveValue, bool bImproved)
	{
		const FLinearColor ValueColor = (BaseValue == EffectiveValue)
			? FLinearColor::White
			: (bImproved ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(1.0f, 0.7f, 0.35f));

		FCanvasTextItem LabelItem(
			FVector2D(StatsX, StatsY),
			FText::FromString(Label),
			GEngine->GetSmallFont(),
			FLinearColor(0.7f, 0.73f, 0.8f));
		LabelItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(LabelItem);

		FCanvasTextItem BaseItem(
			FVector2D(StatsX + 130.f, StatsY),
			FText::FromString(BaseValue),
			GEngine->GetSmallFont(),
			FLinearColor(0.55f, 0.58f, 0.64f));
		BaseItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(BaseItem);

		FCanvasTextItem EffectiveItem(
			FVector2D(StatsX + 220.f, StatsY),
			FText::FromString(EffectiveValue),
			GEngine->GetSmallFont(),
			ValueColor);
		EffectiveItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(EffectiveItem);

		StatsY += 24.f;
	};

	DrawStatRow(
		TEXT("Damage"),
		FString::Printf(TEXT("%.1f"), BaseStats.Damage),
		FString::Printf(TEXT("%.1f"), EffectiveStats.Damage),
		EffectiveStats.Damage >= BaseStats.Damage);
	DrawStatRow(
		TEXT("RPM"),
		FString::Printf(TEXT("%.0f"), BaseStats.FireRate),
		FString::Printf(TEXT("%.0f"), EffectiveStats.FireRate),
		EffectiveStats.FireRate >= BaseStats.FireRate);
	DrawStatRow(
		TEXT("Magazine"),
		FString::Printf(TEXT("%d"), BaseStats.MagazineCapacity),
		FString::Printf(TEXT("%d"), EffectiveStats.MagazineCapacity),
		EffectiveStats.MagazineCapacity >= BaseStats.MagazineCapacity);
	DrawStatRow(
		TEXT("Reserve"),
		FString::Printf(TEXT("%d"), BaseStats.MaxReserveAmmo),
		FString::Printf(TEXT("%d"), EffectiveStats.MaxReserveAmmo),
		EffectiveStats.MaxReserveAmmo >= BaseStats.MaxReserveAmmo);
	DrawStatRow(
		TEXT("Reload"),
		FString::Printf(TEXT("%.2fs"), BaseStats.ReloadTime),
		FString::Printf(TEXT("%.2fs"), EffectiveStats.ReloadTime),
		EffectiveStats.ReloadTime <= BaseStats.ReloadTime);
	DrawStatRow(
		TEXT("Hip Spread"),
		FString::Printf(TEXT("%.2f"), BaseStats.HipFireSpreadMultiplier),
		FString::Printf(TEXT("%.2f"), EffectiveStats.HipFireSpreadMultiplier),
		EffectiveStats.HipFireSpreadMultiplier <= BaseStats.HipFireSpreadMultiplier);
	DrawStatRow(
		TEXT("ADS Spread"),
		FString::Printf(TEXT("%.2f"), BaseStats.ADSSpreadMultiplier),
		FString::Printf(TEXT("%.2f"), EffectiveStats.ADSSpreadMultiplier),
		EffectiveStats.ADSSpreadMultiplier <= BaseStats.ADSSpreadMultiplier);
	DrawStatRow(
		TEXT("Crouch Spread"),
		FString::Printf(TEXT("%.2f"), BaseStats.CrouchSpreadMultiplier),
		FString::Printf(TEXT("%.2f"), EffectiveStats.CrouchSpreadMultiplier),
		EffectiveStats.CrouchSpreadMultiplier <= BaseStats.CrouchSpreadMultiplier);
	DrawStatRow(
		TEXT("Max Spread"),
		FString::Printf(TEXT("%.2f"), BaseStats.MaxSpread),
		FString::Printf(TEXT("%.2f"), EffectiveStats.MaxSpread),
		EffectiveStats.MaxSpread <= BaseStats.MaxSpread);
	DrawStatRow(
		TEXT("Headshot"),
		FString::Printf(TEXT("%.2fx"), BaseStats.HeadshotDamageMultiplier),
		FString::Printf(TEXT("%.2fx"), EffectiveStats.HeadshotDamageMultiplier),
		EffectiveStats.HeadshotDamageMultiplier >= BaseStats.HeadshotDamageMultiplier);
	DrawStatRow(
		TEXT("Limb"),
		FString::Printf(TEXT("%.2fx"), BaseStats.LimbDamageMultiplier),
		FString::Printf(TEXT("%.2fx"), EffectiveStats.LimbDamageMultiplier),
		EffectiveStats.LimbDamageMultiplier >= BaseStats.LimbDamageMultiplier);

	DrawSpreadPreviewBoard(
		TEXT("BASELINE SPREAD"),
		TEXT("Bare gun sustained fire trail"),
		BaseStats,
		PX + InnerPadding,
		PreviewY,
		PreviewW,
		PreviewH,
		FLinearColor(0.32f, 0.78f, 1.0f));

	DrawSpreadPreviewBoard(
		TEXT("CURRENT BUILD SPREAD"),
		TEXT("Live result after equipped attachments"),
		EffectiveStats,
		PX + InnerPadding + PreviewW + PreviewGap,
		PreviewY,
		PreviewW,
		PreviewH,
		FLinearColor(0.95f, 0.72f, 0.22f));

	DrawRect(FLinearColor(0.28f, 0.30f, 0.35f, 0.55f), PX + 20.f, PY + PanelH - 44.f, PanelW - 40.f, 1.f);

	FCanvasTextItem Hint(
		FVector2D(PX + 20.f, PY + PanelH - 32.f),
		FText::FromString(TEXT("Preview reads as shot-order trajectory: tighter grouping and slower climb mean the build is easier to control.")),
		GEngine->GetSmallFont(),
		FLinearColor(0.68f, 0.72f, 0.78f));
	Hint.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Hint);
}

void ADemoHUD::DrawItemSlot(float X, float Y, float W, float H, const FString& Name, int32 Qty, int32 Index, bool bSelected, const FLinearColor& RarityColor)
{
	FLinearColor BgColor = bSelected ? FLinearColor(0.3f, 0.3f, 0.5f, 0.85f) : FLinearColor(0.08f, 0.08f, 0.12f, 0.85f);
	DrawRect(BgColor, X, Y, W, H);
	DrawRect(RarityColor * 0.8f, X, Y, 3.f, H);

	FString IndexStr = FString::Printf(TEXT("[%d]"), Index + 1);
	FCanvasTextItem IdxItem(FVector2D(X + 8.f, Y + 4.f), FText::FromString(IndexStr),
		GEngine->GetSmallFont(), FLinearColor(0.6f, 0.6f, 0.6f));
	IdxItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(IdxItem);

	FCanvasTextItem NameItem(FVector2D(X + 40.f, Y + 4.f), FText::FromString(Name),
		GEngine->GetSmallFont(), FLinearColor::White);
	NameItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(NameItem);

	if (Qty > 1)
	{
		FString QtyStr = FString::Printf(TEXT("x%d"), Qty);
		FCanvasTextItem QtyItem(FVector2D(X + W - 45.f, Y + 4.f), FText::FromString(QtyStr),
			GEngine->GetSmallFont(), FLinearColor(0.8f, 0.8f, 0.8f));
		QtyItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(QtyItem);
	}
}

static FLinearColor GetRarityColor(EDemoItemRarity Rarity)
{
	switch (Rarity)
	{
	case EDemoItemRarity::Common:    return FLinearColor(0.7f, 0.7f, 0.7f);
	case EDemoItemRarity::Uncommon:  return FLinearColor(0.2f, 0.8f, 0.2f);
	case EDemoItemRarity::Rare:      return FLinearColor(0.3f, 0.5f, 1.f);
	case EDemoItemRarity::Epic:      return FLinearColor(0.7f, 0.3f, 1.f);
	case EDemoItemRarity::Legendary: return FLinearColor(1.f, 0.7f, 0.f);
	default: return FLinearColor::White;
	}
}

void ADemoHUD::DrawContainerPanel()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	UDemoInventoryComponent* Inv = Ch->FindComponentByClass<UDemoInventoryComponent>();
	TArray<FDemoInventorySlot> DedicatedContents;

	if (const UDemoOnlineGameInstance* GI = GetWorld() ? Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (GI->ShouldUseDedicatedServer())
		{
			if (const UDemoDedicatedServerSubsystem* DedicatedServer = GetWorld()->GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
			{
				const FDemoDedicatedServerContainerSnapshot* ContainerSnapshot =
					DedicatedServer->FindContainerSnapshot(Ch->GetCurrentDedicatedContainerId());
				if (!ContainerSnapshot)
				{
					return;
				}

				DedicatedContents = ContainerSnapshot->Contents;
			}
		}
	}

	ADemoLootContainer* Container = Ch->GetCurrentContainer();
	if (!Container && DedicatedContents.Num() == 0 && Ch->GetCurrentDedicatedContainerId().IsEmpty()) return;
	const bool bUseDedicatedContainer = !Ch->GetCurrentDedicatedContainerId().IsEmpty();

	float PanelW = 560.f;
	float PanelH = 450.f;
	float PX = (Canvas->SizeX - PanelW) * 0.5f;
	float PY = (Canvas->SizeY - PanelH) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.92f), PX, PY, PanelW, PanelH);
	DrawRect(FLinearColor(0.4f, 0.6f, 0.8f, 0.6f), PX, PY, PanelW, 2.f);

	float HalfW = PanelW * 0.5f - 10.f;

	{
		FCanvasTextItem Title(FVector2D(PX + 10.f, PY + 8.f), FText::FromString(TEXT("CONTAINER")),
			GEngine->GetSmallFont(), FLinearColor(0.4f, 0.8f, 1.f));
		Title.EnableShadow(FLinearColor::Black);
		Title.Scale = FVector2D(1.2f, 1.2f);
		Canvas->DrawItem(Title);
	}

	{
		FCanvasTextItem Title(FVector2D(PX + HalfW + 20.f, PY + 8.f), FText::FromString(TEXT("BACKPACK")),
			GEngine->GetSmallFont(), FLinearColor(0.8f, 0.8f, 0.4f));
		Title.EnableShadow(FLinearColor::Black);
		Title.Scale = FVector2D(1.2f, 1.2f);
		Canvas->DrawItem(Title);
	}

	DrawRect(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f), PX + HalfW + 10.f, PY + 32.f, 1.f, PanelH - 40.f);

	float SlotH = 26.f;
	float Gap = 3.f;
	float Y = PY + 38.f;

	const TArray<FDemoInventorySlot>& Contents =
		bUseDedicatedContainer ? DedicatedContents : Container->GetContents();
	for (int32 i = 0; i < Contents.Num(); ++i)
	{
		if (Contents[i].IsEmpty()) continue;

		FString ItemName = Contents[i].ItemID.ToString();
		FLinearColor RarColor = FLinearColor(0.7f, 0.7f, 0.7f);

		if (Inv)
		{
			FDemoItemData Data;
			if (Inv->FindItemData(Contents[i].ItemID, Data))
			{
				ItemName = Data.DisplayName.IsEmpty() ? Contents[i].ItemID.ToString() : Data.DisplayName.ToString();
				RarColor = GetRarityColor(Data.Rarity);
			}
		}

		DrawItemSlot(PX + 8.f, Y, HalfW, SlotH, ItemName, Contents[i].Quantity, i, false, RarColor);
		Y += SlotH + Gap;
	}

	Y = PY + 38.f;
	if (Inv)
	{
		const TArray<FDemoInventorySlot>& Items = Inv->GetAllItems();
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			FString ItemName = Items[i].ItemID.ToString();
			FLinearColor RarColor = FLinearColor(0.7f, 0.7f, 0.7f);

			FDemoItemData Data;
			if (Inv->FindItemData(Items[i].ItemID, Data))
			{
				ItemName = Data.DisplayName.IsEmpty() ? Items[i].ItemID.ToString() : Data.DisplayName.ToString();
				RarColor = GetRarityColor(Data.Rarity);
			}

			DrawItemSlot(PX + HalfW + 18.f, Y, HalfW, SlotH, ItemName, Items[i].Quantity, i, false, RarColor);
			Y += SlotH + Gap;
		}

		float InvY = PY + PanelH - 22.f;
		FString WeightStr = FString::Printf(TEXT("%.1f / %.0f kg"), Inv->GetCurrentWeight(), Inv->GetMaxWeight());
		FCanvasTextItem WItem(FVector2D(PX + HalfW + 18.f, InvY), FText::FromString(WeightStr),
			GEngine->GetSmallFont(), FLinearColor(0.6f, 0.6f, 0.6f));
		WItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(WItem);
	}

	{
		FCanvasTextItem Hint(FVector2D(PX + 8.f, PY + PanelH - 22.f),
			FText::FromString(TEXT("[1-9] Take item   [F/Tab] Close")),
			GEngine->GetSmallFont(), FLinearColor(0.5f, 0.5f, 0.5f));
		Hint.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Hint);
	}
}

void ADemoHUD::DrawInventoryPanel()
{
	ADemoCharacter* Ch = GetDemoCharacter();
	if (!Ch) return;

	UDemoInventoryComponent* Inv = Ch->FindComponentByClass<UDemoInventoryComponent>();
	if (!Inv) return;

	float PanelW = 320.f;
	float PanelH = 400.f;
	float PX = (Canvas->SizeX - PanelW) * 0.5f;
	float PY = (Canvas->SizeY - PanelH) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.92f), PX, PY, PanelW, PanelH);
	DrawRect(FLinearColor(0.8f, 0.8f, 0.3f, 0.6f), PX, PY, PanelW, 2.f);

	{
		FCanvasTextItem Title(FVector2D(PX + 10.f, PY + 8.f), FText::FromString(TEXT("BACKPACK")),
			GEngine->GetSmallFont(), FLinearColor(0.9f, 0.9f, 0.4f));
		Title.EnableShadow(FLinearColor::Black);
		Title.Scale = FVector2D(1.2f, 1.2f);
		Canvas->DrawItem(Title);
	}

	FString WeightStr = FString::Printf(TEXT("%.1f / %.0f kg   $%d"), Inv->GetCurrentWeight(), Inv->GetMaxWeight(), Inv->GetTotalValue());
	FCanvasTextItem WItem(FVector2D(PX + PanelW - 180.f, PY + 10.f), FText::FromString(WeightStr),
		GEngine->GetSmallFont(), FLinearColor(0.6f, 0.6f, 0.6f));
	WItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(WItem);

	float SlotH = 26.f;
	float Gap = 3.f;
	float Y = PY + 38.f;

	const TArray<FDemoInventorySlot>& Items = Inv->GetAllItems();

	if (Items.Num() == 0)
	{
		FCanvasTextItem Empty(FVector2D(PX + 30.f, Y + 20.f),
			FText::FromString(TEXT("Empty")),
			GEngine->GetSmallFont(), FLinearColor(0.4f, 0.4f, 0.4f));
		Empty.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Empty);
	}

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		FString ItemName = Items[i].ItemID.ToString();
		FLinearColor RarColor = FLinearColor(0.7f, 0.7f, 0.7f);

		FDemoItemData Data;
		if (Inv->FindItemData(Items[i].ItemID, Data))
		{
			ItemName = Data.DisplayName.IsEmpty() ? Items[i].ItemID.ToString() : Data.DisplayName.ToString();
			RarColor = GetRarityColor(Data.Rarity);
		}

		DrawItemSlot(PX + 8.f, Y, PanelW - 16.f, SlotH, ItemName, Items[i].Quantity, i, false, RarColor);
		Y += SlotH + Gap;
	}

	{
		const bool bDedicatedPath =
			GetWorld() &&
			Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance()) &&
			Cast<UDemoOnlineGameInstance>(GetWorld()->GetGameInstance())->ShouldUseDedicatedServer();
		FCanvasTextItem Hint(FVector2D(PX + 8.f, PY + PanelH - 22.f),
			FText::FromString(
				bDedicatedPath
					? TEXT("[1-3] Use med/armor/ammo or drop   [Tab/F] Close")
					: TEXT("[Tab/F] Close")),
			GEngine->GetSmallFont(), FLinearColor(0.5f, 0.5f, 0.5f));
		Hint.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Hint);
	}
}
