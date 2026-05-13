#include "Weapon/DemoWeaponBase.h"
#include "AI/DemoAICharacter.h"
#include "Character/DemoCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/DemoAttributeSet.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	static constexpr float GDemoSprayHorizontalPattern[] =
	{
		0.0f, 0.22f, 0.48f, 0.82f, 1.00f, 0.88f, 0.60f, 0.22f,
		-0.18f, -0.52f, -0.78f, -0.70f, -0.46f, -0.16f, 0.06f, 0.15f,
		0.10f, 0.03f
	};

	static constexpr float GDemoSprayVerticalPattern[] =
	{
		0.0f, 0.44f, 0.92f, 1.46f, 2.04f, 2.66f, 3.34f, 4.08f,
		4.88f, 5.74f, 6.66f, 7.64f, 8.68f, 9.78f, 10.94f, 12.16f,
		13.44f, 14.78f
	};
}

ADemoWeaponBase::ADemoWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultBulletHoleDecalFinder(
		TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial"));
	if (DefaultBulletHoleDecalFinder.Succeeded())
	{
		BulletHoleDecal = DefaultBulletHoleDecalFinder.Object;
	}

	MuzzleAttachmentConfig.DisplayName = FText::FromString(TEXT("Muzzle Brake"));
	MuzzleAttachmentConfig.RecoilPitchMultiplier = 0.68f;
	MuzzleAttachmentConfig.RecoilYawMultiplier = 0.92f;
	MuzzleAttachmentConfig.SpreadRecoverySpeedMultiplier = 1.10f;

	GripAttachmentConfig.DisplayName = FText::FromString(TEXT("Vertical Grip"));
	GripAttachmentConfig.BaseSpreadMultiplier = 0.94f;
	GripAttachmentConfig.SpreadPerShotMultiplier = 0.76f;
	GripAttachmentConfig.CrouchSpreadMultiplier = 0.92f;
	GripAttachmentConfig.RecoilPitchMultiplier = 0.96f;
	GripAttachmentConfig.RecoilYawMultiplier = 0.60f;

	MagazineAttachmentConfig.DisplayName = FText::FromString(TEXT("Extended Mag"));
	MagazineAttachmentConfig.MagazineCapacityDelta = 15;
	MagazineAttachmentConfig.MaxReserveAmmoDelta = 30;
	MagazineAttachmentConfig.ReloadTimeMultiplier = 1.15f;

	OpticAttachmentConfig.DisplayName = FText::FromString(TEXT("Reflex Sight"));
	OpticAttachmentConfig.ADSSpreadMultiplier = 0.68f;
	OpticAttachmentConfig.BaseSpreadMultiplier = 0.95f;
}

void ADemoWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	SyncReloadTimeFromAnimation();
	InitializeBaseStatsFromSource();
	RebuildEffectiveStats();
	CurrentAmmo = MagazineCapacity;
	ReserveAmmo = MaxReserveAmmo;
	CurrentSpread = BaseSpread;
}

FVector2D ADemoWeaponBase::ComputeSprayPatternOffset(
	const FDemoWeaponStatConfig& Stats,
	int32 ShotIndex,
	float SpreadMultiplier,
	float RecoilMultiplier)
{
	ShotIndex = FMath::Max(0, ShotIndex);

	const int32 LastPatternIndex = UE_ARRAY_COUNT(GDemoSprayHorizontalPattern) - 1;
	const int32 ClampedPatternIndex = FMath::Clamp(ShotIndex, 0, LastPatternIndex);
	float HorizontalShape = GDemoSprayHorizontalPattern[ClampedPatternIndex];
	float VerticalShape = GDemoSprayVerticalPattern[ClampedPatternIndex];

	if (ShotIndex > LastPatternIndex)
	{
		const float TailIndex = static_cast<float>(ShotIndex - LastPatternIndex);
		HorizontalShape =
			GDemoSprayHorizontalPattern[LastPatternIndex] * FMath::Exp(-0.36f * TailIndex) +
			0.05f * FMath::Sin(TailIndex * 1.2f) * FMath::Exp(-0.24f * TailIndex);
		VerticalShape += TailIndex * 1.45f;
	}

	const float SafeSpreadMultiplier = FMath::Max(0.05f, SpreadMultiplier);
	const float SafeRecoilMultiplier = FMath::Max(0.05f, RecoilMultiplier);
	const float AveragePitchKick = FMath::Max(
		0.05f,
		(FMath::Abs(Stats.RecoilPitchMin) + FMath::Abs(Stats.RecoilPitchMax)) * 0.5f);
	const float AverageYawKick = FMath::Max(
		0.04f,
		(FMath::Abs(Stats.RecoilYawMin) + FMath::Abs(Stats.RecoilYawMax)) * 0.5f);
	const float SprayGrowth = Stats.BaseSpread + Stats.SpreadPerShot * static_cast<float>(ShotIndex);
	const float MaxSpreadRef = FMath::Max(Stats.BaseSpread + 0.01f, Stats.MaxSpread);
	const float SprayBloomAlpha = FMath::Clamp(SprayGrowth / MaxSpreadRef, 0.0f, 1.0f);

	const float HorizontalScale =
		(AverageYawKick * 1.20f + SprayBloomAlpha * SafeSpreadMultiplier * 0.85f) *
		SafeRecoilMultiplier;
	const float VerticalScale =
		(AveragePitchKick * 1.32f + SprayBloomAlpha * SafeSpreadMultiplier * 1.05f) *
		SafeRecoilMultiplier;

	return FVector2D(
		HorizontalShape * HorizontalScale,
		-VerticalShape * VerticalScale);
}

void ADemoWeaponBase::SyncReloadTimeFromAnimation()
{
}

void ADemoWeaponBase::InitializeBaseStatsFromSource()
{
	if (bUseStructuredWeaponStats)
	{
		BaseWeaponStats = StructuredBaseStats;
	}
	else
	{
		BaseWeaponStats.Damage = Damage;
		BaseWeaponStats.FireRate = FireRate;
		BaseWeaponStats.MagazineCapacity = MagazineCapacity;
		BaseWeaponStats.MaxReserveAmmo = MaxReserveAmmo;
		BaseWeaponStats.ReloadTime = ReloadTime;
		BaseWeaponStats.MaxRange = MaxRange;
		BaseWeaponStats.RecoilPitchMin = RecoilPitchMin;
		BaseWeaponStats.RecoilPitchMax = RecoilPitchMax;
		BaseWeaponStats.RecoilYawMin = RecoilYawMin;
		BaseWeaponStats.RecoilYawMax = RecoilYawMax;
		BaseWeaponStats.ADSRecoilMultiplier = ADSRecoilMultiplier;
		BaseWeaponStats.BaseSpread = BaseSpread;
		BaseWeaponStats.SpreadPerShot = SpreadPerShot;
		BaseWeaponStats.MaxSpread = MaxSpread;
		BaseWeaponStats.SpreadRecoverySpeed = SpreadRecoverySpeed;
		BaseWeaponStats.ADSSpreadMultiplier = ADSSpreadMultiplier;
	}
}

void ADemoWeaponBase::RebuildEffectiveStats()
{
	EffectiveWeaponStats = BaseWeaponStats;

	auto ApplyModifier = [this](const FDemoWeaponAttachmentModifier* Modifier)
	{
		if (!Modifier)
		{
			return;
		}

		EffectiveWeaponStats.Damage *= Modifier->DamageMultiplier;
		EffectiveWeaponStats.FireRate *= Modifier->FireRateMultiplier;
		EffectiveWeaponStats.MagazineCapacity += Modifier->MagazineCapacityDelta;
		EffectiveWeaponStats.MaxReserveAmmo += Modifier->MaxReserveAmmoDelta;
		EffectiveWeaponStats.ReloadTime *= Modifier->ReloadTimeMultiplier;
		EffectiveWeaponStats.BaseSpread *= Modifier->BaseSpreadMultiplier;
		EffectiveWeaponStats.SpreadPerShot *= Modifier->SpreadPerShotMultiplier;
		EffectiveWeaponStats.MaxSpread *= Modifier->MaxSpreadMultiplier;
		EffectiveWeaponStats.SpreadRecoverySpeed *= Modifier->SpreadRecoverySpeedMultiplier;
		EffectiveWeaponStats.HipFireSpreadMultiplier *= Modifier->HipFireSpreadMultiplier;
		EffectiveWeaponStats.ADSSpreadMultiplier *= Modifier->ADSSpreadMultiplier;
		EffectiveWeaponStats.CrouchSpreadMultiplier *= Modifier->CrouchSpreadMultiplier;
		EffectiveWeaponStats.RecoilPitchMin *= Modifier->RecoilPitchMultiplier;
		EffectiveWeaponStats.RecoilPitchMax *= Modifier->RecoilPitchMultiplier;
		EffectiveWeaponStats.RecoilYawMin *= Modifier->RecoilYawMultiplier;
		EffectiveWeaponStats.RecoilYawMax *= Modifier->RecoilYawMultiplier;
		EffectiveWeaponStats.HeadshotDamageMultiplier *= Modifier->HeadshotDamageMultiplier;
		EffectiveWeaponStats.LimbDamageMultiplier *= Modifier->LimbDamageMultiplier;
	};

	if (bMuzzleAttachmentEquipped)
	{
		ApplyModifier(&MuzzleAttachmentConfig);
	}
	if (bGripAttachmentEquipped)
	{
		ApplyModifier(&GripAttachmentConfig);
	}
	if (bMagazineAttachmentEquipped)
	{
		ApplyModifier(&MagazineAttachmentConfig);
	}
	if (bOpticAttachmentEquipped)
	{
		ApplyModifier(&OpticAttachmentConfig);
	}

	EffectiveWeaponStats.FireRate = FMath::Max(60.f, EffectiveWeaponStats.FireRate);
	EffectiveWeaponStats.MagazineCapacity = FMath::Max(1, EffectiveWeaponStats.MagazineCapacity);
	EffectiveWeaponStats.MaxReserveAmmo = FMath::Max(0, EffectiveWeaponStats.MaxReserveAmmo);
	EffectiveWeaponStats.ReloadTime = FMath::Max(0.1f, EffectiveWeaponStats.ReloadTime);
	EffectiveWeaponStats.MaxRange = FMath::Max(100.f, EffectiveWeaponStats.MaxRange);
	EffectiveWeaponStats.BaseSpread = FMath::Max(0.01f, EffectiveWeaponStats.BaseSpread);
	EffectiveWeaponStats.SpreadPerShot = FMath::Max(0.01f, EffectiveWeaponStats.SpreadPerShot);
	EffectiveWeaponStats.MaxSpread = FMath::Max(EffectiveWeaponStats.BaseSpread, EffectiveWeaponStats.MaxSpread);
	EffectiveWeaponStats.SpreadRecoverySpeed = FMath::Max(0.1f, EffectiveWeaponStats.SpreadRecoverySpeed);
	EffectiveWeaponStats.ADSSpreadMultiplier = FMath::Max(0.05f, EffectiveWeaponStats.ADSSpreadMultiplier);
	EffectiveWeaponStats.HipFireSpreadMultiplier = FMath::Max(0.1f, EffectiveWeaponStats.HipFireSpreadMultiplier);
	EffectiveWeaponStats.CrouchSpreadMultiplier = FMath::Max(0.1f, EffectiveWeaponStats.CrouchSpreadMultiplier);
	EffectiveWeaponStats.HeadshotDamageMultiplier = FMath::Max(1.0f, EffectiveWeaponStats.HeadshotDamageMultiplier);
	EffectiveWeaponStats.LimbDamageMultiplier = FMath::Clamp(EffectiveWeaponStats.LimbDamageMultiplier, 0.1f, 1.0f);

	Damage = EffectiveWeaponStats.Damage;
	FireRate = EffectiveWeaponStats.FireRate;
	MagazineCapacity = EffectiveWeaponStats.MagazineCapacity;
	MaxReserveAmmo = EffectiveWeaponStats.MaxReserveAmmo;
	ReloadTime = EffectiveWeaponStats.ReloadTime;
	MaxRange = EffectiveWeaponStats.MaxRange;
	RecoilPitchMin = EffectiveWeaponStats.RecoilPitchMin;
	RecoilPitchMax = EffectiveWeaponStats.RecoilPitchMax;
	RecoilYawMin = EffectiveWeaponStats.RecoilYawMin;
	RecoilYawMax = EffectiveWeaponStats.RecoilYawMax;
	ADSRecoilMultiplier = EffectiveWeaponStats.ADSRecoilMultiplier;
	BaseSpread = EffectiveWeaponStats.BaseSpread;
	SpreadPerShot = EffectiveWeaponStats.SpreadPerShot;
	MaxSpread = EffectiveWeaponStats.MaxSpread;
	SpreadRecoverySpeed = EffectiveWeaponStats.SpreadRecoverySpeed;
	ADSSpreadMultiplier = EffectiveWeaponStats.ADSSpreadMultiplier;

	CurrentAmmo = FMath::Clamp(CurrentAmmo, 0, MagazineCapacity);
	ReserveAmmo = FMath::Clamp(ReserveAmmo, 0, MaxReserveAmmo);
	CurrentSpread = FMath::Clamp(FMath::Max(CurrentSpread, BaseSpread), BaseSpread, MaxSpread);
}

bool ADemoWeaponBase::ToggleAttachment(EDemoWeaponAttachmentSlot Slot)
{
	if (bool* AttachmentState = GetAttachmentStatePtr(Slot))
	{
		*AttachmentState = !*AttachmentState;
		RebuildEffectiveStats();
		return *AttachmentState;
	}

	return false;
}

void ADemoWeaponBase::ClearAttachments()
{
	bMuzzleAttachmentEquipped = false;
	bGripAttachmentEquipped = false;
	bMagazineAttachmentEquipped = false;
	bOpticAttachmentEquipped = false;
	RebuildEffectiveStats();
}

bool ADemoWeaponBase::IsAttachmentEquipped(EDemoWeaponAttachmentSlot Slot) const
{
	if (const bool* AttachmentState = GetAttachmentStatePtr(Slot))
	{
		return *AttachmentState;
	}

	return false;
}

FText ADemoWeaponBase::GetAttachmentDisplayName(EDemoWeaponAttachmentSlot Slot) const
{
	if (const FDemoWeaponAttachmentModifier* Modifier = GetAttachmentModifier(Slot))
	{
		if (!Modifier->DisplayName.IsEmpty())
		{
			return Modifier->DisplayName;
		}
	}

	return GetDefaultAttachmentName(Slot);
}

FString ADemoWeaponBase::GetAttachmentEffectDescription(EDemoWeaponAttachmentSlot Slot) const
{
	switch (Slot)
	{
	case EDemoWeaponAttachmentSlot::Muzzle:
		return TEXT("Reduce vertical climb and recover faster between bursts");
	case EDemoWeaponAttachmentSlot::Grip:
		return TEXT("Reduce horizontal sway and tighten sustained fire bloom");
	case EDemoWeaponAttachmentSlot::Magazine:
		return TEXT("More rounds and reserve ammo, slower reload");
	case EDemoWeaponAttachmentSlot::Optic:
		return TEXT("Improve ADS precision and keep center hold steadier");
	default:
		return TEXT("No effect");
	}
}

void ADemoWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsFiring && CurrentSpread > BaseSpread)
	{
		CurrentSpread = FMath::FInterpTo(CurrentSpread, BaseSpread, DeltaTime, SpreadRecoverySpeed);
	}
}

void ADemoWeaponBase::SetOwningCharacter(ADemoCharacter* NewOwner)
{
	OwningCharacter = NewOwner;
	OwnerPawn = NewOwner;
}

void ADemoWeaponBase::SetOwnerPawn(APawn* NewOwner)
{
	OwnerPawn = NewOwner;
}

APawn* ADemoWeaponBase::GetOwnerPawn() const
{
	return OwnerPawn ? OwnerPawn : OwningCharacter;
}

void ADemoWeaponBase::StartFire()
{
	if (bIsReloading) return;

	if (CurrentAmmo <= 0)
	{
		StartReload();
		return;
	}

	bIsFiring = true;
	ShotsFiredInBurst = 0;

	FireShot();

	if (FireMode == EDemoWeaponFireMode::Automatic)
	{
		float Interval = 60.f / FMath::Max(FireRate, 1.f);
		GetWorldTimerManager().SetTimer(
			FireTimerHandle, this, &ADemoWeaponBase::FireShot, Interval, true);
	}
}

void ADemoWeaponBase::StopFire()
{
	bIsFiring = false;
	ShotsFiredInBurst = 0;
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

void ADemoWeaponBase::FireShot()
{
	if (CurrentAmmo <= 0)
	{
		StopFire();
		StartReload();
		return;
	}

	CurrentAmmo--;
	ShotsFiredInBurst++;
	LastFireTime = GetWorld()->GetTimeSeconds();

	int32 ShotsToFire = FMath::Max(PelletCount, 1);
	for (int32 i = 0; i < ShotsToFire; ++i)
	{
		PerformHitscan();
	}

	ApplyRecoil();

	float SpreadMult = GetCurrentSpreadMultiplier();
	CurrentSpread = FMath::Min(CurrentSpread + SpreadPerShot * SpreadMult, MaxSpread);

	if (MuzzleFlashEffect && WeaponMesh)
	{
		FVector Loc = WeaponMesh->GetSocketLocation(MuzzleSocketName);
		FRotator Rot = WeaponMesh->GetSocketRotation(MuzzleSocketName);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), MuzzleFlashEffect, Loc, Rot);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, GetActorLocation());
	}

	PlayFireMontage();

	if (FireMode == EDemoWeaponFireMode::SemiAutomatic)
	{
		StopFire();
	}
	else if (FireMode == EDemoWeaponFireMode::Burst && ShotsFiredInBurst >= 3)
	{
		StopFire();
	}
}

void ADemoWeaponBase::PerformHitscan()
{
	APawn* ShooterPawn = GetOwnerPawn();
	if (!ShooterPawn) return;

	FVector CamLoc;
	FRotator CamRot;

	APlayerController* PC = Cast<APlayerController>(ShooterPawn->GetController());
	if (PC)
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
	}
	else
	{
		CamLoc = ShooterPawn->GetPawnViewLocation();
		CamRot = ShooterPawn->GetViewRotation();
	}

	float SpreadMult = GetCurrentSpreadMultiplier();
	FVector AimDirection = CamRot.Vector();

	if (!PC)
	{
		if (const ADemoAICharacter* AICharacter = Cast<ADemoAICharacter>(ShooterPawn))
		{
			if (AActor* CombatTarget = AICharacter->GetCombatTarget())
			{
				FVector TargetPoint = CombatTarget->GetActorLocation();
				if (const APawn* TargetPawn = Cast<APawn>(CombatTarget))
				{
					TargetPoint.Z += TargetPawn->BaseEyeHeight;
				}

				const FVector ToTarget = TargetPoint - CamLoc;
				if (!ToTarget.IsNearlyZero())
				{
					AimDirection = ToTarget.GetSafeNormal();
				}
			}
		}
	}

	FVector ShotDir = AimDirection;
	if (PC)
	{
		ShotDir = AimDirection;
	}
	else
	{
		const float RecoilMultiplier = bIsADS ? ADSRecoilMultiplier : 1.0f;
		const FVector2D SprayOffset = ComputeSprayPatternOffset(
			EffectiveWeaponStats,
			FMath::Max(0, ShotsFiredInBurst - 1),
			SpreadMult,
			RecoilMultiplier);
		FRotator ShotRotation = AimDirection.Rotation();
		ShotRotation.Yaw += SprayOffset.X;
		ShotRotation.Pitch += SprayOffset.Y;
		ShotDir = ShotRotation.Vector();
	}

	FVector TraceEnd = CamLoc + ShotDir * MaxRange;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(ShooterPawn);
	Params.bTraceComplex = false;
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, CamLoc, TraceEnd, ECC_Visibility, Params);

	FVector MuzzleLoc = WeaponMesh ? WeaponMesh->GetSocketLocation(MuzzleSocketName) : CamLoc;
	FVector TracerEnd = bHit ? Hit.ImpactPoint : TraceEnd;
	SpawnBulletTracer(MuzzleLoc, TracerEnd);

	if (bHit)
	{
		SpawnImpactEffects(Hit);

		if (AActor* HitActor = Hit.GetActor())
		{
			if (HasAuthority())
			{
				ApplyGASDamage(HitActor, Hit);
				OnWeaponHit.Broadcast(HitActor, Hit);
			}
		}
	}
}

void ADemoWeaponBase::ApplyGASDamage(AActor* HitActor, const FHitResult& HitResult)
{
	APawn* ShooterPawn = GetOwnerPawn();
	if (!ShooterPawn) return;

	IAbilitySystemInterface* ShooterASI = Cast<IAbilitySystemInterface>(ShooterPawn);
	IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(HitActor);
	if (!TargetASI) return;

	UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = ShooterASI ? ShooterASI->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC || !SourceASC) return;

	if (DamageEffectClass)
	{
		const float AppliedDamage = Damage * GetDamageMultiplierForHit(HitResult);
		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddSourceObject(this);
		ContextHandle.AddInstigator(ShooterPawn, this);
		ContextHandle.AddHitResult(HitResult);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
			DamageEffectClass, 1.f, ContextHandle);

		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Damage")),
				AppliedDamage);

			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}
	}
	else
	{
		const float AppliedDamage = Damage * GetDamageMultiplierForHit(HitResult);
		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddSourceObject(this);
		ContextHandle.AddInstigator(ShooterPawn, this);
		ContextHandle.AddHitResult(HitResult);

		UGameplayEffect* DmgEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("DirectDamage"));
		DmgEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		int32 Idx = DmgEffect->Modifiers.Num();
		DmgEffect->Modifiers.SetNum(Idx + 1);
		FGameplayModifierInfo& Mod = DmgEffect->Modifiers[Idx];
		Mod.Attribute = UDemoAttributeSet::GetHealthAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;

		FScalableFloat DmgValue;
		DmgValue.Value = -AppliedDamage;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(DmgValue);

		FGameplayEffectSpec* DmgSpec = new FGameplayEffectSpec(DmgEffect, ContextHandle, 1.f);
		SourceASC->ApplyGameplayEffectSpecToTarget(*DmgSpec, TargetASC);
		delete DmgSpec;
	}
}

void ADemoWeaponBase::ApplyRecoil()
{
	if (!OwningCharacter) return;

	const float RecoilMultiplier = bIsADS ? ADSRecoilMultiplier : 1.0f;
	const float SpreadMultiplier = GetCurrentSpreadMultiplier();
	const int32 CurrentPatternIndex = FMath::Max(0, ShotsFiredInBurst - 1);
	const FVector2D CurrentPatternOffset = ComputeSprayPatternOffset(
		EffectiveWeaponStats,
		CurrentPatternIndex,
		SpreadMultiplier,
		RecoilMultiplier);
	const FVector2D NextPatternOffset = ComputeSprayPatternOffset(
		EffectiveWeaponStats,
		CurrentPatternIndex + 1,
		SpreadMultiplier,
		RecoilMultiplier);
	const FVector2D RecoilDelta = NextPatternOffset - CurrentPatternOffset;
	OwningCharacter->ApplyRecoil(RecoilDelta.Y, RecoilDelta.X);
}

void ADemoWeaponBase::StartReload()
{
	if (bIsReloading || CurrentAmmo == MagazineCapacity || ReserveAmmo <= 0) return;

	bIsReloading = true;
	StopFire();
	ActiveReloadMontage = nullptr;

	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ReloadSound, GetActorLocation());
	}

	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	bReloadDrivenBySingleNodeSequence = false;

	UAnimSequence* RelSeq = SelectReloadArmsSequence();

	// UAnimSingleNodeInstance 下 CurrentAsset 为循环 Sequence 时，评估不会走 Slot，Montage_Play 叠不上去；
	// 换弹改为整段替换为换弹 Sequence，结束后恢复 Idle/Aim。
	if (RelSeq && OwningCharacter && OwningCharacter->GetFirstPersonMesh())
	{
		if (UAnimSingleNodeInstance* SingleInst = Cast<UAnimSingleNodeInstance>(
				OwningCharacter->GetFirstPersonMesh()->GetAnimInstance()))
		{
			bReloadDrivenBySingleNodeSequence = true;
			ActiveReloadMontage = nullptr;
			SingleInst->SetAnimationAsset(RelSeq, false, 1.f);
			SingleInst->SetPlaying(true);

			const float Len = FMath::Max(RelSeq->GetPlayLength(), 0.05f);
			GetWorldTimerManager().SetTimer(
				ReloadTimerHandle, this, &ADemoWeaponBase::OnSingleNodeReloadSequenceFinished, Len, false);

			if (WeaponMesh)
			{
				if (CurrentAmmo <= 0 && WeaponReloadEmptySequence)
				{
					WeaponMesh->PlayAnimation(WeaponReloadEmptySequence, false);
				}
				else if (WeaponReloadSequence)
				{
					WeaponMesh->PlayAnimation(WeaponReloadSequence, false);
				}
			}
			return;
		}
	}

	UAnimMontage* ReloadMontage = RelSeq ? GetOrCreateSlotMontage(RelSeq) : nullptr;

	if (ReloadMontage && OwningCharacter && OwningCharacter->GetFirstPersonMesh())
	{
		if (UAnimInstance* AI = OwningCharacter->GetFirstPersonMesh()->GetAnimInstance())
		{
			ActiveReloadMontage = ReloadMontage;
			AI->Montage_Play(ReloadMontage, 1.f);
			FOnMontageEnded EndDel;
			EndDel.BindUObject(this, &ADemoWeaponBase::OnReloadMontageEnded);
			AI->Montage_SetEndDelegate(EndDel, ReloadMontage);

			if (WeaponMesh)
			{
				if (CurrentAmmo <= 0 && WeaponReloadEmptySequence)
				{
					WeaponMesh->PlayAnimation(WeaponReloadEmptySequence, false);
				}
				else if (WeaponReloadSequence)
				{
					WeaponMesh->PlayAnimation(WeaponReloadSequence, false);
				}
			}
			return;
		}
	}

	GetWorldTimerManager().SetTimer(
		ReloadTimerHandle, this, &ADemoWeaponBase::HandleReloadFinished, ReloadTime, false);
}

void ADemoWeaponBase::OnSingleNodeReloadSequenceFinished()
{
	bReloadDrivenBySingleNodeSequence = false;
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	if (OwningCharacter)
	{
		OwningCharacter->RestoreFirstPersonArmBaseLoop();
	}
	HandleReloadFinished();
}

void ADemoWeaponBase::OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ActiveReloadMontage && Montage != ActiveReloadMontage)
	{
		return;
	}
	ActiveReloadMontage = nullptr;
	if (!bIsReloading)
	{
		return;
	}
	if (bInterrupted)
	{
		bIsReloading = false;
		return;
	}
	HandleReloadFinished();
}

void ADemoWeaponBase::AbortReload()
{
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);

	if (bReloadDrivenBySingleNodeSequence)
	{
		bReloadDrivenBySingleNodeSequence = false;
		if (OwningCharacter)
		{
			OwningCharacter->RestoreFirstPersonArmBaseLoop();
		}
		bIsReloading = false;
		return;
	}

	if (OwningCharacter && ActiveReloadMontage)
	{
		if (UAnimInstance* AI = OwningCharacter->GetFirstPersonMesh()->GetAnimInstance())
		{
			AI->Montage_Stop(0.2f, ActiveReloadMontage);
		}
	}
	ActiveReloadMontage = nullptr;
	bIsReloading = false;
}

void ADemoWeaponBase::PlayEquipAnimation()
{
	if (!ArmsEquipSequence || !OwningCharacter) return;
	if (USkeletalMeshComponent* Arms = OwningCharacter->GetFirstPersonMesh())
	{
		if (UAnimInstance* AI = Arms->GetAnimInstance())
		{
			if (UAnimMontage* M = GetOrCreateSlotMontage(ArmsEquipSequence))
			{
				AI->Montage_Play(M, 1.f);
			}
		}
	}
}

void ADemoWeaponBase::HandleReloadFinished()
{
	int32 Needed    = MagazineCapacity - CurrentAmmo;
	int32 Available = FMath::Min(Needed, ReserveAmmo);

	CurrentAmmo  += Available;
	ReserveAmmo  -= Available;
	bIsReloading  = false;
	ActiveReloadMontage = nullptr;
}

void ADemoWeaponBase::SetADS(bool bNewADS)
{
	bIsADS = bNewADS;
}

float ADemoWeaponBase::GetCurrentSpreadMultiplier() const
{
	float SpreadMultiplier = bIsADS ? EffectiveWeaponStats.ADSSpreadMultiplier : EffectiveWeaponStats.HipFireSpreadMultiplier;

	if (const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwnerPawn()))
	{
		if (const UCharacterMovementComponent* MovementComponent = CharacterOwner->GetCharacterMovement())
		{
			if (MovementComponent->IsCrouching())
			{
				SpreadMultiplier *= EffectiveWeaponStats.CrouchSpreadMultiplier;
			}
		}
	}

	return FMath::Max(0.05f, SpreadMultiplier);
}

float ADemoWeaponBase::GetDamageMultiplierForHit(const FHitResult& HitResult) const
{
	if (HitResult.BoneName.IsNone())
	{
		return 1.0f;
	}

	const FString BoneNameLower = HitResult.BoneName.ToString().ToLower();
	if (BoneNameLower.Contains(TEXT("head")))
	{
		return EffectiveWeaponStats.HeadshotDamageMultiplier;
	}

	if (BoneNameLower.Contains(TEXT("arm")) ||
		BoneNameLower.Contains(TEXT("hand")) ||
		BoneNameLower.Contains(TEXT("leg")) ||
		BoneNameLower.Contains(TEXT("foot")) ||
		BoneNameLower.Contains(TEXT("calf")) ||
		BoneNameLower.Contains(TEXT("thigh")))
	{
		return EffectiveWeaponStats.LimbDamageMultiplier;
	}

	return 1.0f;
}

const FDemoWeaponAttachmentModifier* ADemoWeaponBase::GetAttachmentModifier(EDemoWeaponAttachmentSlot Slot) const
{
	switch (Slot)
	{
	case EDemoWeaponAttachmentSlot::Muzzle:
		return &MuzzleAttachmentConfig;
	case EDemoWeaponAttachmentSlot::Grip:
		return &GripAttachmentConfig;
	case EDemoWeaponAttachmentSlot::Magazine:
		return &MagazineAttachmentConfig;
	case EDemoWeaponAttachmentSlot::Optic:
		return &OpticAttachmentConfig;
	default:
		return nullptr;
	}
}

FDemoWeaponAttachmentModifier* ADemoWeaponBase::GetAttachmentModifier(EDemoWeaponAttachmentSlot Slot)
{
	switch (Slot)
	{
	case EDemoWeaponAttachmentSlot::Muzzle:
		return &MuzzleAttachmentConfig;
	case EDemoWeaponAttachmentSlot::Grip:
		return &GripAttachmentConfig;
	case EDemoWeaponAttachmentSlot::Magazine:
		return &MagazineAttachmentConfig;
	case EDemoWeaponAttachmentSlot::Optic:
		return &OpticAttachmentConfig;
	default:
		return nullptr;
	}
}

bool* ADemoWeaponBase::GetAttachmentStatePtr(EDemoWeaponAttachmentSlot Slot)
{
	switch (Slot)
	{
	case EDemoWeaponAttachmentSlot::Muzzle:
		return &bMuzzleAttachmentEquipped;
	case EDemoWeaponAttachmentSlot::Grip:
		return &bGripAttachmentEquipped;
	case EDemoWeaponAttachmentSlot::Magazine:
		return &bMagazineAttachmentEquipped;
	case EDemoWeaponAttachmentSlot::Optic:
		return &bOpticAttachmentEquipped;
	default:
		return nullptr;
	}
}

const bool* ADemoWeaponBase::GetAttachmentStatePtr(EDemoWeaponAttachmentSlot Slot) const
{
	switch (Slot)
	{
	case EDemoWeaponAttachmentSlot::Muzzle:
		return &bMuzzleAttachmentEquipped;
	case EDemoWeaponAttachmentSlot::Grip:
		return &bGripAttachmentEquipped;
	case EDemoWeaponAttachmentSlot::Magazine:
		return &bMagazineAttachmentEquipped;
	case EDemoWeaponAttachmentSlot::Optic:
		return &bOpticAttachmentEquipped;
	default:
		return nullptr;
	}
}

FText ADemoWeaponBase::GetDefaultAttachmentName(EDemoWeaponAttachmentSlot Slot)
{
	switch (Slot)
	{
	case EDemoWeaponAttachmentSlot::Muzzle:
		return FText::FromString(TEXT("Muzzle Brake"));
	case EDemoWeaponAttachmentSlot::Grip:
		return FText::FromString(TEXT("Vertical Grip"));
	case EDemoWeaponAttachmentSlot::Magazine:
		return FText::FromString(TEXT("Extended Mag"));
	case EDemoWeaponAttachmentSlot::Optic:
		return FText::FromString(TEXT("Reflex Sight"));
	default:
		return FText::FromString(TEXT("Attachment"));
	}
}

void ADemoWeaponBase::SpawnBulletTracer(const FVector& Start, const FVector& End)
{
	if (!GetWorld()) return;
	FColor Color = TracerColor.ToFColor(true);
	DrawDebugLine(GetWorld(), Start, End, Color, false, 0.06f, 0, 0.75f);
}

void ADemoWeaponBase::PlayFireMontage()
{
	if (!OwningCharacter) return;

	USkeletalMeshComponent* ArmsMesh = OwningCharacter->GetFirstPersonMesh();
	if (!ArmsMesh) return;

	UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance();
	if (!AnimInst) return;

	if (FireMontage)
	{
		AnimInst->Montage_Play(FireMontage, 1.0f);
	}
	else
	{
		UAnimSequence* Seq = nullptr;
		if (bIsADS && ArmsFireADSSequence)
		{
			Seq = ArmsFireADSSequence;
		}
		else if (ArmsFireSequence)
		{
			Seq = ArmsFireSequence;
		}
		if (Seq)
		{
			if (UAnimMontage* M = GetOrCreateSlotMontage(Seq))
			{
				AnimInst->Montage_Play(M, 1.f);
			}
		}
	}

	PlayWeaponFireAnimation();
}

void ADemoWeaponBase::PlayWeaponFireAnimation()
{
	if (!WeaponFireSequence || !WeaponMesh) return;
	WeaponMesh->PlayAnimation(WeaponFireSequence, false);
}

UAnimMontage* ADemoWeaponBase::GetOrCreateSlotMontage(UAnimSequence* Sequence)
{
	if (!Sequence) return nullptr;
	if (UAnimMontage** Found = CachedSlotMontages.Find(Sequence))
	{
		return *Found;
	}

	UAnimMontage* M = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		Sequence,
		ArmMontageSlotName,
		0.05f,
		0.12f,
		1.f,
		1,
		-1.f,
		0.f);

	if (M)
	{
		CachedSlotMontages.Add(Sequence, M);
		GeneratedMontageRefs.Add(M);
	}
	return M;
}

UAnimSequence* ADemoWeaponBase::SelectReloadArmsSequence() const
{
	const bool bMagEmpty = (CurrentAmmo <= 0);

	if (bIsADS)
	{
		if (bMagEmpty)
		{
			if (ArmsReloadEmptyADSSequence) return ArmsReloadEmptyADSSequence;
			if (ArmsReloadADSSequence) return ArmsReloadADSSequence;
		}
		else
		{
			if (ArmsReloadADSSequence) return ArmsReloadADSSequence;
		}
	}
	else
	{
		if (bMagEmpty)
		{
			if (ArmsReloadEmptySequence) return ArmsReloadEmptySequence;
			if (ArmsReloadSequence) return ArmsReloadSequence;
		}
		else
		{
			if (ArmsReloadSequence) return ArmsReloadSequence;
		}
	}

	if (ArmsReloadADSSequence) return ArmsReloadADSSequence;
	return ArmsReloadSequence;
}

void ADemoWeaponBase::SpawnImpactEffects(const FHitResult& HitResult)
{
	const FVector ImpactPoint = HitResult.ImpactPoint;
	const FVector ImpactNormal = HitResult.ImpactNormal.GetSafeNormal();
	const FVector DecalLocation = ImpactPoint + ImpactNormal * BulletHoleDecalSurfaceOffset;
	const FRotator ImpactRot = (-ImpactNormal).Rotation();

	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ImpactEffect, ImpactPoint, ImpactRot);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, ImpactPoint);
	}

	if (BulletHoleDecal)
	{
		const AActor* HitActor = HitResult.GetActor();
		const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
		const bool bHitCharacterSurface = Cast<APawn>(HitActor) != nullptr || Cast<USkeletalMeshComponent>(HitComponent) != nullptr;
		if (!bHitCharacterSurface && HitComponent)
		{
			if (UDecalComponent* BulletHole = UGameplayStatics::SpawnDecalAtLocation(
				GetWorld(),
				BulletHoleDecal,
				BulletHoleDecalSize,
				DecalLocation,
				ImpactRot,
				0.0f))
			{
				BulletHole->SetFadeScreenSize(0.0001f);
				BulletHole->SetFadeOut(BulletHoleDecalLifetime, 0.15f, false);
			}
		}
	}
}
