#pragma once

#include "CoreMinimal.h"
#include "Core/DemoTypes.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "DemoCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAbilitySystemComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UDemoAttributeSet;
class UDemoInventoryComponent;
class ADemoWeaponBase;
class ADemoGrenade;
class ADemoLootContainer;
class ADemoDedicatedServerContainerActor;
class ADemoDedicatedServerLootActor;
class UAnimSequence;
struct FDemoDedicatedServerPlayerSnapshot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitConfirmed, AActor*, HitActor, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillConfirmed, AActor*, KilledActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamageTaken, float, Amount, AActor*, Causer);

UCLASS()
class DEMOCLIENT_API ADemoCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADemoCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	ADemoWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetArmorPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetStaminaPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool GetIsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool GetIsADS() const { return bIsADS; }

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool GetIsDead() const { return bIsDead; }

	void ApplyRecoil(float PitchOffset, float YawOffset);
	void ApplyDedicatedServerSnapshot(const FDemoDedicatedServerPlayerSnapshot& Snapshot);

	/** 武器单段动画（如换弹）结束后恢复 FP 手臂 Idle / Aim 循环（SingleNode 模式） */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void RestoreFirstPersonArmBaseLoop();

	// Hit feedback — used by HUD
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHitConfirmed OnHitConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnKillConfirmed OnKillConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDamageTaken OnDamageTaken;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	TArray<FVector> GetRecentDamageDirections() const { return RecentDamageDirections; }

	void Respawn();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Workbench")
	bool IsWeaponWorkbenchOpen() const { return bWeaponWorkbenchOpen; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsContainerOpen() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	class ADemoLootContainer* GetCurrentContainer() const { return CurrentContainer; }

	const FString& GetCurrentDedicatedContainerId() const { return CurrentDedicatedContainerId; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseContainer();

	void TakeContainerItem(int32 Index);
	void HandleDedicatedInventorySlotAction(int32 Index);
	void ToggleWeaponAttachment(EDemoWeaponAttachmentSlot Slot);
	void ClearWeaponAttachments();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMeshComponent* ThirdPersonPlaceholderMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	UTextRenderComponent* ThirdPersonNameplate;

	/** SK_FP_Manny_Simple + SingleNode 时：资源包持枪待机动画 */
	UPROPERTY()
	UAnimSequence* AKSArmsIdleLoopSequence;

	UPROPERTY()
	UAnimSequence* AKSArmsAimLoopSequence;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY()
	UDemoAttributeSet* AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	UDemoInventoryComponent* InventoryComponent;

	// --- Input Actions ---
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* CombatMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* SprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* CrouchAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* FireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* ADSAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* ReloadAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* WeaponSlot1Action;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* WeaponSlot2Action;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* WeaponSlot3Action;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* GrenadeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* InventoryAction;

	// --- Movement ---
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float DefaultWalkSpeed = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeedMultiplier = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float ADSSpeedMultiplier = 0.6f;

	// --- Stamina ---
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float StaminaDrainRate = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float StaminaRecoveryRate = 10.f;

	// --- Weapons ---
	/** 若设置，则三个武器槽均使用该类型（用于统一换 FP_AKS74U 等资源）；留空则使用下方三种武器类 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ADemoWeaponBase> UniformWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ADemoWeaponBase> DefaultWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ADemoWeaponBase> SecondaryWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ADemoWeaponBase> SidearmWeaponClass;

	/** 第一人称武器挂载插槽（UE5 Manny 手臂常用 hand_r；旧模板为 GripPoint） */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName FirstPersonWeaponAttachSocketName = FName(TEXT("hand_r"));

	/** 挂到插槽后的局部变换微调（位置/旋转/缩放），在 BP_DemoCharacter 里边跑边调最方便 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FTransform FirstPersonWeaponSocketOffset = FTransform::Identity;

	UPROPERTY()
	TArray<ADemoWeaponBase*> WeaponSlots;

	UPROPERTY()
	ADemoWeaponBase* CurrentWeapon;

	int32 CurrentWeaponIndex = -1;

	// --- Grenade ---
	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	TSubclassOf<ADemoGrenade> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	int32 MaxGrenades = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	float GrenadeThrowForce = 2000.f;

	int32 CurrentGrenades;

	// --- Death ---
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float RespawnDelay = 3.0f;

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJump(const FInputActionValue& Value);
	void HandleStopJump(const FInputActionValue& Value);
	void HandleSprintStart(const FInputActionValue& Value);
	void HandleSprintStop(const FInputActionValue& Value);
	void HandleCrouch(const FInputActionValue& Value);
	void HandleFireStart(const FInputActionValue& Value);
	void HandleFireStop(const FInputActionValue& Value);
	void HandleADSStart(const FInputActionValue& Value);
	void HandleADSStop(const FInputActionValue& Value);
	void HandleReload(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);
	void HandleWeaponSlot1(const FInputActionValue& Value);
	void HandleWeaponSlot2(const FInputActionValue& Value);
	void HandleWeaponSlot3(const FInputActionValue& Value);
	void HandleGrenadeThrow(const FInputActionValue& Value);
	void HandleInventoryToggle(const FInputActionValue& Value);
	void HandleWeaponWorkbenchToggle();
	void HandleToggleMuzzleAttachment();
	void HandleToggleGripAttachment();
	void HandleToggleMagazineAttachment();
	void HandleToggleOpticAttachment();
	void HandleClearWeaponAttachments();
	void ProcessWorkbenchHotkeys();

	void SpawnWeapons();
	void SwitchToWeaponSlot(int32 SlotIndex);
	void UpdateMovementSpeed();
	void UpdateFirstPersonArmBaseLoop();
	void UpdateThirdPersonPresentation();
	void UpdateThirdPersonNameplate();
	void RefreshThirdPersonBodyPresentation();
	void InitializeAbilityActorInfo();
	void ApplyAliveState(bool bIsAlive);
	bool ShouldUseDedicatedServerPath() const;
	void SendDedicatedServerInput(float DeltaTime);
	void ApplyDedicatedServerAliveState(bool bIsAlive);

	UFUNCTION()
	void OnRep_CombatState();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION(Server, Reliable)
	void ServerSetSprintState(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetADSState(bool bNewADS);

	UFUNCTION(Server, Reliable)
	void ServerStartFire();

	UFUNCTION(Server, Reliable)
	void ServerStopFire();

	UFUNCTION(Server, Reliable)
	void ServerReloadCurrentWeapon();

	UFUNCTION(Server, Reliable)
	void ServerSwitchWeaponSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerToggleWeaponAttachment(int32 SlotIndex, EDemoWeaponAttachmentSlot Slot);

	UFUNCTION(Server, Reliable)
	void ServerClearWeaponAttachments(int32 SlotIndex);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyHitConfirmed(AActor* HitActor, bool bKilledTarget);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyDamageTaken(float DamageAmount, AActor* DamageCauser);

	UFUNCTION()
	void OnWeaponHitCallback(AActor* HitActor, const FHitResult& HitResult);

	UFUNCTION()
	void OnHealthDepleted(AActor* KillerActor);

	UFUNCTION()
	void OnDamageReceived(float DamageAmount, AActor* DamageCauser, const FHitResult& HitResult);

	void Die();

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	bool bIsSprinting = false;

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	bool bIsADS = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	FVector2D RecoilOffset;
	float RecoilRecoverySpeed = 8.f;

	TArray<FVector> RecentDamageDirections;
	TArray<float> DamageDirectionTimestamps;
	FVector2D CachedDedicatedMoveInput = FVector2D::ZeroVector;
	int32 DedicatedInputSequence = 0;
	float DedicatedInputSendAccumulator = 0.0f;
	float DedicatedInputSendInterval = 0.05f;

	bool bInventoryOpen = false;
	bool bWeaponWorkbenchOpen = false;
	FString CurrentDedicatedContainerId;

	UPROPERTY()
	ADemoLootContainer* CurrentContainer = nullptr;
};
