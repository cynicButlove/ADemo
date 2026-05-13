#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AI/DemoAITypes.h"
#include "DemoAICharacter.generated.h"

class UAbilitySystemComponent;
class UDemoAttributeSet;
class ADemoWeaponBase;
class FLifetimeProperty;

UENUM(BlueprintType)
enum class EDemoAIDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
	Elite
};

UENUM(BlueprintType)
enum class EDemoAICombatStyle : uint8
{
	Rifleman,
	Assaulter,
	Elite
};

UCLASS()
class DEMOCLIENT_API ADemoAICharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADemoAICharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "AI")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, Category = "AI")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "AI")
	ADemoWeaponBase* GetWeapon() const { return Weapon; }

	UFUNCTION(BlueprintCallable, Category = "AI")
	EDemoAIDifficulty GetDifficulty() const { return Difficulty; }

	UFUNCTION(BlueprintCallable, Category = "AI")
	EDemoAICombatStyle GetCombatStyle() const { return CombatStyle; }

	UFUNCTION(BlueprintCallable, Category = "AI")
	bool GetIsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintCallable, Category = "AI")
	bool GetHasCombatTarget() const { return bHasCombatTarget; }

	UFUNCTION(BlueprintCallable, Category = "AI")
	AActor* GetCombatTarget() const { return CombatTargetActor.Get(); }

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	EDemoAITacticalState GetTacticalState() const { return TacticalState; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	EDemoAITacticalRole GetTacticalRole() const { return TacticalRole; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tactical")
	FString GetTacticalDebugText() const;

	void SetTacticalState(EDemoAITacticalState NewState);
	void SetTacticalRole(EDemoAITacticalRole NewRole);
	void SetAlertedBySquad(bool bNewAlertedBySquad);
	bool WasAlertedBySquad() const { return bAlertedBySquad; }

	void StartFiring();
	void StopFiring();
	void SetTargetingState(bool bNewHasCombatTarget);
	void SetCombatTarget(AActor* NewCombatTarget);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "AI")
	EDemoAIDifficulty Difficulty = EDemoAIDifficulty::Normal;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "AI")
	EDemoAICombatStyle CombatStyle = EDemoAICombatStyle::Rifleman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AimAccuracy = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float ReactionTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactical")
	bool bDrawTacticalDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactical")
	float TacticalDebugDrawDistance = 6500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Patrol")
	TArray<FVector> PatrolPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Patrol")
	int32 CurrentPatrolIndex = 0;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnHealthDepleted(AActor* KillerActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDemoAttributeSet* AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Weapon")
	TSubclassOf<ADemoWeaponBase> WeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Weapon")
	FName WeaponAttachSocketName = TEXT("hand_r");

	UPROPERTY()
	ADemoWeaponBase* Weapon;

	bool bIsDead = false;
	bool bIsFiring = false;
	bool bHasCombatTarget = false;
	float FireTimer = 0.f;
	float BurstTimer = 0.f;
	int32 BurstShotsRemaining = 0;

	TWeakObjectPtr<AActor> CombatTargetActor;

private:
	void InitializeAbilityActorInfo();
	void RefreshThirdPersonBodyPresentation();
	void SpawnWeapon();
	void ApplyDifficultySettings();
	void DrawTacticalDebug() const;
	FColor GetTacticalDebugColor() const;
	FColor GetCombatStyleDebugColor() const;

	UPROPERTY(Replicated)
	EDemoAITacticalState TacticalState = EDemoAITacticalState::Patrol;

	UPROPERTY(Replicated)
	EDemoAITacticalRole TacticalRole = EDemoAITacticalRole::None;

	UPROPERTY(Replicated)
	bool bAlertedBySquad = false;
};
