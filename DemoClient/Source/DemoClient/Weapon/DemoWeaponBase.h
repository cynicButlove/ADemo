#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DemoTypes.h"
#include "DemoWeaponBase.generated.h"

class USkeletalMeshComponent;
class UNiagaraSystem;
class ADemoCharacter;
class UGameplayEffect;
class UAnimSequence;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponHit, AActor*, HitActor, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponKill, AActor*, KilledActor);

UCLASS(Abstract)
class DEMOCLIENT_API ADemoWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ADemoWeaponBase();

	virtual void Tick(float DeltaTime) override;

	void StartFire();
	void StopFire();
	void StartReload();
	void SetADS(bool bNewADS);
	void SetOwningCharacter(ADemoCharacter* NewOwner);
	void SetOwnerPawn(APawn* NewOwner);
	APawn* GetOwnerPawn() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool GetIsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool GetIsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool GetIsADS() const { return bIsADS; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 GetMagazineCapacity() const { return MagazineCapacity; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetCurrentSpread() const { return CurrentSpread; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetDamage() const { return Damage; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	EDemoWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRate() const { return FireRate; }

	const FDemoWeaponStatConfig& GetBaseWeaponStats() const { return BaseWeaponStats; }

	const FDemoWeaponStatConfig& GetEffectiveWeaponStats() const { return EffectiveWeaponStats; }

	static FVector2D ComputeSprayPatternOffset(const FDemoWeaponStatConfig& Stats, int32 ShotIndex, float SpreadMultiplier, float RecoilMultiplier);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	bool ToggleAttachment(EDemoWeaponAttachmentSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void ClearAttachments();

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	bool IsAttachmentEquipped(EDemoWeaponAttachmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	FText GetAttachmentDisplayName(EDemoWeaponAttachmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	FString GetAttachmentEffectDescription(EDemoWeaponAttachmentSlot Slot) const;

	/** 切换武器时播放持枪就位动画 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void PlayEquipAnimation();

	/** 取消换弹（切枪等），不补充弹药 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void AbortReload();

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponHit OnWeaponHit;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponKill OnWeaponKill;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	bool bUseStructuredWeaponStats = false;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config", meta = (EditCondition = "bUseStructuredWeaponStats"))
	FDemoWeaponStatConfig StructuredBaseStats;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attachments")
	FDemoWeaponAttachmentModifier MuzzleAttachmentConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attachments")
	FDemoWeaponAttachmentModifier GripAttachmentConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attachments")
	FDemoWeaponAttachmentModifier MagazineAttachmentConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attachments")
	FDemoWeaponAttachmentModifier OpticAttachmentConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	EDemoWeaponType WeaponType = EDemoWeaponType::AssaultRifle;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	EDemoWeaponFireMode FireMode = EDemoWeaponFireMode::Automatic;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats", meta = (ToolTip = "Rounds per minute"))
	float FireRate = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	int32 MagazineCapacity = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	int32 MaxReserveAmmo = 120;

	/** 无换弹蒙太奇时使用的换弹时长；有蒙太奇时仍可作为回退 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	float ReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	float MaxRange = 10000.f;

	// --- Recoil ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float RecoilPitchMin = -1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float RecoilPitchMax = -0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float RecoilYawMin = -0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float RecoilYawMax = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float ADSRecoilMultiplier = 0.7f;

	// --- Spread ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float BaseSpread = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadPerShot = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float MaxSpread = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadRecoverySpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float ADSSpreadMultiplier = 0.3f;

	// --- Shotgun pellets ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Shotgun")
	int32 PelletCount = 1;

	// --- GAS Damage ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|GAS")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// --- Effects ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	UNiagaraSystem* MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	USoundBase* FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	USoundBase* ReloadSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	UNiagaraSystem* ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	USoundBase* ImpactSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	UMaterialInterface* BulletHoleDecal;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	FVector BulletHoleDecalSize = FVector(6.0f, 10.0f, 10.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	float BulletHoleDecalLifetime = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	float BulletHoleDecalSurfaceOffset = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	UAnimMontage* FireMontage;

	/** 与 FireMontage 二选一：使用动画序列时在手臂网格的 Slot 上动态生成 Montage（需 AnimBP 含同名 Slot，默认 DefaultSlot） */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	FName ArmMontageSlotName = FName(TEXT("DefaultSlot"));

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsFireSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsFireADSSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsReloadSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsReloadADSSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsReloadEmptySequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsReloadEmptyADSSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* ArmsEquipSequence;

	/** 枪械骨骼网格上的附加动画（如拉机柄），无 AnimBP 时用 PlayAnimation */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* WeaponFireSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* WeaponReloadSequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	UAnimSequence* WeaponReloadEmptySequence;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	FLinearColor TracerColor = FLinearColor(1.f, 0.8f, 0.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
	FName MuzzleSocketName = TEXT("Muzzle");

	virtual void SyncReloadTimeFromAnimation();

private:
	void InitializeBaseStatsFromSource();
	void RebuildEffectiveStats();
	void FireShot();
	void PerformHitscan();
	void ApplyGASDamage(AActor* HitActor, const FHitResult& HitResult);
	void ApplyRecoil();
	void HandleReloadFinished();
	void OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnSingleNodeReloadSequenceFinished();
	void SpawnImpactEffects(const FHitResult& HitResult);
	void SpawnBulletTracer(const FVector& Start, const FVector& End);
	void PlayFireMontage();
	void PlayWeaponFireAnimation();
	float GetCurrentSpreadMultiplier() const;
	float GetDamageMultiplierForHit(const FHitResult& HitResult) const;
	const FDemoWeaponAttachmentModifier* GetAttachmentModifier(EDemoWeaponAttachmentSlot Slot) const;
	FDemoWeaponAttachmentModifier* GetAttachmentModifier(EDemoWeaponAttachmentSlot Slot);
	bool* GetAttachmentStatePtr(EDemoWeaponAttachmentSlot Slot);
	const bool* GetAttachmentStatePtr(EDemoWeaponAttachmentSlot Slot) const;
	static FText GetDefaultAttachmentName(EDemoWeaponAttachmentSlot Slot);

	UAnimMontage* GetOrCreateSlotMontage(UAnimSequence* Sequence);
	UAnimSequence* SelectReloadArmsSequence() const;

	TMap<UAnimSequence*, UAnimMontage*> CachedSlotMontages;

	UPROPERTY()
	TArray<UAnimMontage*> GeneratedMontageRefs;

	UPROPERTY()
	UAnimMontage* ActiveReloadMontage = nullptr;

	UPROPERTY()
	ADemoCharacter* OwningCharacter;

	UPROPERTY()
	APawn* OwnerPawn;

	FDemoWeaponStatConfig BaseWeaponStats;
	FDemoWeaponStatConfig EffectiveWeaponStats;

	int32 CurrentAmmo = 0;
	int32 ReserveAmmo = 0;
	float CurrentSpread = 0.f;
	bool bIsFiring = false;
	bool bIsReloading = false;
	bool bReloadDrivenBySingleNodeSequence = false;
	bool bIsADS = false;
	bool bMuzzleAttachmentEquipped = false;
	bool bGripAttachmentEquipped = false;
	bool bMagazineAttachmentEquipped = false;
	bool bOpticAttachmentEquipped = false;
	float LastFireTime = 0.f;
	int32 ShotsFiredInBurst = 0;

	FTimerHandle FireTimerHandle;
	FTimerHandle ReloadTimerHandle;
};
