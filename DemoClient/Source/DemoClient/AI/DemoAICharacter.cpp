#include "AI/DemoAICharacter.h"
#include "AI/DemoAIController.h"
#include "Combat/DemoAbilitySystemComponent.h"
#include "Combat/DemoAttributeSet.h"
#include "Weapon/DemoWeaponBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace DemoAICharacterPaths
{
	static const TCHAR* const MannyMesh = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	static const TCHAR* const MannyAnimClass = TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C");
}

namespace
{
	FString CombatStyleToString(EDemoAICombatStyle Style)
	{
		switch (Style)
		{
		case EDemoAICombatStyle::Assaulter:
			return TEXT("Assaulter");
		case EDemoAICombatStyle::Elite:
			return TEXT("Elite");
		case EDemoAICombatStyle::Rifleman:
		default:
			return TEXT("Rifleman");
		}
	}

	FString DifficultyToString(EDemoAIDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EDemoAIDifficulty::Easy:
			return TEXT("Easy");
		case EDemoAIDifficulty::Hard:
			return TEXT("Hard");
		case EDemoAIDifficulty::Elite:
			return TEXT("Elite");
		case EDemoAIDifficulty::Normal:
		default:
			return TEXT("Normal");
		}
	}
}

ADemoAICharacter::ADemoAICharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(120.0f);
	SetMinNetUpdateFrequency(60.0f);

	AIControllerClass = ADemoAIController::StaticClass();

	AbilitySystemComponent = CreateDefaultSubobject<UDemoAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UDemoAttributeSet>(TEXT("AttributeSet"));

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	GetCharacterMovement()->MaxWalkSpeed = 350.f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
	GetCharacterMovement()->NetworkMaxSmoothUpdateDistance = 384.0f;
	GetCharacterMovement()->NetworkNoSmoothUpdateDistance = 768.0f;

	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	static ConstructorHelpers::FClassFinder<ADemoWeaponBase> WepClass(TEXT("/Game/FirstPerson/Blueprints/BP_DemoWeapon_AR"));
	if (WepClass.Succeeded())
	{
		WeaponClass = WepClass.Class;
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(
		DemoAICharacterPaths::MannyMesh);
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimClass(
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		GetMesh()->SetOwnerNoSee(false);
		GetMesh()->SetOnlyOwnerSee(false);
		GetMesh()->SetHiddenInGame(false);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (BodyAnimClass.Succeeded() && BodyAnimClass.Class)
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(BodyAnimClass.Class);
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->bEnableUpdateRateOptimizations = false;
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

UAbilitySystemComponent* ADemoAICharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADemoAICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADemoAICharacter, Difficulty);
	DOREPLIFETIME(ADemoAICharacter, CombatStyle);
	DOREPLIFETIME(ADemoAICharacter, TacticalState);
	DOREPLIFETIME(ADemoAICharacter, TacticalRole);
	DOREPLIFETIME(ADemoAICharacter, bAlertedBySquad);
}

float ADemoAICharacter::GetHealthPercent() const
{
	if (!AttributeSet) return 0.f;
	if (AttributeSet->GetMaxHealth() <= 0.f) return 0.f;
	return AttributeSet->GetHealth() / AttributeSet->GetMaxHealth();
}

void ADemoAICharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilityActorInfo();
	RefreshThirdPersonBodyPresentation();
	SetTargetingState(false);

	if (AttributeSet)
	{
		AttributeSet->OnHealthDepleted.AddDynamic(this, &ADemoAICharacter::OnHealthDepleted);
	}

	ApplyDifficultySettings();
	SpawnWeapon();

	if (PatrolPoints.Num() == 0)
	{
		FVector Origin = GetActorLocation();
		PatrolPoints.Add(Origin + FVector(500.f, 0.f, 0.f));
		PatrolPoints.Add(Origin + FVector(0.f, 500.f, 0.f));
		PatrolPoints.Add(Origin + FVector(-500.f, 0.f, 0.f));
		PatrolPoints.Add(Origin + FVector(0.f, -500.f, 0.f));
	}
}

void ADemoAICharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead) return;

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		if (!CharacterMesh->GetAnimInstance())
		{
			RefreshThirdPersonBodyPresentation();
		}
	}

	if (bIsFiring && Weapon)
	{
		FireTimer -= DeltaTime;
		if (FireTimer <= 0.f)
		{
			if (BurstShotsRemaining > 0)
			{
				Weapon->StartFire();
				FTimerHandle StopHandle;
				GetWorldTimerManager().SetTimer(StopHandle, [this]()
				{
					if (Weapon) Weapon->StopFire();
				}, 0.05f, false);

				BurstShotsRemaining--;
				FireTimer = 60.f / FMath::Max(Weapon->GetFireRate(), 1.f);
			}
			else
			{
				float BurstDelay = FMath::RandRange(0.3f, 0.8f);
				FireTimer = BurstDelay;
				BurstShotsRemaining = FMath::RandRange(2, 5);
			}
		}
	}

	DrawTacticalDebug();
}

void ADemoAICharacter::StartFiring()
{
	bIsFiring = true;
	FireTimer = 0.f;
	BurstShotsRemaining = FMath::RandRange(3, 6);
}

void ADemoAICharacter::StopFiring()
{
	bIsFiring = false;
	if (Weapon) Weapon->StopFire();
}

void ADemoAICharacter::SetCombatTarget(AActor* NewCombatTarget)
{
	CombatTargetActor = NewCombatTarget;
}

void ADemoAICharacter::SetTacticalState(EDemoAITacticalState NewState)
{
	TacticalState = NewState;
}

void ADemoAICharacter::SetTacticalRole(EDemoAITacticalRole NewRole)
{
	TacticalRole = NewRole;
}

void ADemoAICharacter::SetAlertedBySquad(bool bNewAlertedBySquad)
{
	bAlertedBySquad = bNewAlertedBySquad;
}

FString ADemoAICharacter::GetTacticalDebugText() const
{
	return FString::Printf(
		TEXT("%s/%s | %s | %s%s"),
		*DifficultyToString(Difficulty),
		*CombatStyleToString(CombatStyle),
		*DemoAI::TacticalStateToString(TacticalState),
		*DemoAI::TacticalRoleToString(TacticalRole),
		bAlertedBySquad ? TEXT(" | SquadAlert") : TEXT(""));
}

void ADemoAICharacter::SetTargetingState(bool bNewHasCombatTarget)
{
	bHasCombatTarget = bNewHasCombatTarget && !bIsDead;
	if (!bHasCombatTarget)
	{
		CombatTargetActor.Reset();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = !bHasCombatTarget;
		MovementComponent->bUseControllerDesiredRotation = bHasCombatTarget;
		MovementComponent->RotationRate = bHasCombatTarget
			? FRotator(0.f, 540.f, 0.f)
			: FRotator(0.f, 360.f, 0.f);
	}

	bUseControllerRotationYaw = bHasCombatTarget;
}

void ADemoAICharacter::OnHealthDepleted(AActor* KillerActor)
{
	if (bIsDead) return;
	bIsDead = true;

	SetTargetingState(false);
	StopFiring();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->UnPossess();
	}

	SetLifeSpan(5.f);
}

void ADemoAICharacter::InitializeAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void ADemoAICharacter::RefreshThirdPersonBodyPresentation()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	CharacterMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	CharacterMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	CharacterMesh->SetOwnerNoSee(false);
	CharacterMesh->SetOnlyOwnerSee(false);
	CharacterMesh->SetHiddenInGame(false);
	CharacterMesh->SetComponentTickEnabled(true);
	CharacterMesh->bPauseAnims = false;
	CharacterMesh->bEnableUpdateRateOptimizations = false;
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (!CharacterMesh->GetSkeletalMeshAsset())
	{
		if (USkeletalMesh* MannyMesh = LoadObject<USkeletalMesh>(nullptr, DemoAICharacterPaths::MannyMesh))
		{
			CharacterMesh->SetSkeletalMesh(MannyMesh);
		}
	}

	if (UClass* MannyAnimClass = LoadClass<UAnimInstance>(nullptr, DemoAICharacterPaths::MannyAnimClass))
	{
		CharacterMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		CharacterMesh->SetAnimInstanceClass(MannyAnimClass);
	}

	CharacterMesh->InitAnim(true);
}

void ADemoAICharacter::SpawnWeapon()
{
	if (!WeaponClass || !GetWorld()) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Weapon = GetWorld()->SpawnActor<ADemoWeaponBase>(WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (Weapon)
	{
		Weapon->SetOwnerPawn(this);
		FName Sock = WeaponAttachSocketName;
		if (GetMesh()->GetSkeletalMeshAsset() && !GetMesh()->DoesSocketExist(WeaponAttachSocketName))
		{
			Sock = NAME_None;
		}
		Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Sock);
	}
}

void ADemoAICharacter::ApplyDifficultySettings()
{
	switch (Difficulty)
	{
	case EDemoAIDifficulty::Easy:
		AimAccuracy = 0.35f;
		ReactionTime = 1.0f;
		if (AttributeSet) { AttributeSet->SetHealth(80.f); AttributeSet->SetMaxHealth(80.f); }
		GetCharacterMovement()->MaxWalkSpeed = 280.f;
		break;
	case EDemoAIDifficulty::Normal:
		AimAccuracy = 0.55f;
		ReactionTime = 0.6f;
		break;
	case EDemoAIDifficulty::Hard:
		AimAccuracy = 0.75f;
		ReactionTime = 0.35f;
		if (AttributeSet) { AttributeSet->SetHealth(120.f); AttributeSet->SetMaxHealth(120.f); }
		GetCharacterMovement()->MaxWalkSpeed = 400.f;
		if (CombatStyle == EDemoAICombatStyle::Rifleman)
		{
			CombatStyle = EDemoAICombatStyle::Assaulter;
		}
		break;
	case EDemoAIDifficulty::Elite:
		CombatStyle = EDemoAICombatStyle::Elite;
		AimAccuracy = 0.9f;
		ReactionTime = 0.2f;
		if (AttributeSet) { AttributeSet->SetHealth(200.f); AttributeSet->SetMaxHealth(200.f); AttributeSet->SetArmor(50.f); }
		GetCharacterMovement()->MaxWalkSpeed = 450.f;
		break;
	}

	switch (CombatStyle)
	{
	case EDemoAICombatStyle::Assaulter:
		AimAccuracy = FMath::Clamp(AimAccuracy - 0.05f, 0.1f, 1.0f);
		ReactionTime *= 0.85f;
		GetCharacterMovement()->MaxWalkSpeed += 45.f;
		break;
	case EDemoAICombatStyle::Elite:
		AimAccuracy = FMath::Clamp(AimAccuracy + 0.06f, 0.1f, 1.0f);
		ReactionTime *= 0.75f;
		break;
	case EDemoAICombatStyle::Rifleman:
	default:
		break;
	}
}

void ADemoAICharacter::DrawTacticalDebug() const
{
	if (!bDrawTacticalDebug || !GetWorld())
	{
		return;
	}

	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		const float DistSq = FVector::DistSquared(
			PlayerController->PlayerCameraManager->GetCameraLocation(),
			GetActorLocation());
		if (DistSq > FMath::Square(TacticalDebugDrawDistance))
		{
			return;
		}
	}

	const FVector HeadLocation = GetActorLocation() + FVector(0.f, 0.f, 120.f);
	const FColor DebugColor = GetTacticalDebugColor();
	const FColor StyleColor = GetCombatStyleDebugColor();
	DrawDebugString(GetWorld(), HeadLocation, GetTacticalDebugText(), nullptr, DebugColor, 0.f, true, 1.05f);
	DrawDebugSphere(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 96.f), 18.f, 10, DebugColor, false, 0.f, 0, 1.5f);
	DrawDebugSphere(GetWorld(), HeadLocation + FVector(28.f, 0.f, 4.f), 9.f, 8, StyleColor, false, 0.f, 0, 1.6f);

	const ADemoAIController* DemoController = Cast<ADemoAIController>(GetController());
	if (!DemoController)
	{
		return;
	}

	if (AActor* Target = DemoController->GetTargetEnemy())
	{
		DrawDebugLine(GetWorld(), HeadLocation, Target->GetActorLocation() + FVector(0.f, 0.f, 80.f), FColor::Red, false, 0.f, 0, 1.25f);
	}

	const FVector LastKnownLocation = DemoController->GetLastKnownEnemyLocation();
	if (!LastKnownLocation.IsNearlyZero())
	{
		DrawDebugLine(GetWorld(), HeadLocation, LastKnownLocation + FVector(0.f, 0.f, 40.f), FColor::Yellow, false, 0.f, 0, 1.0f);
		DrawDebugSphere(GetWorld(), LastKnownLocation + FVector(0.f, 0.f, 30.f), 32.f, 12, FColor::Yellow, false, 0.f, 0, 1.25f);
	}

	if (DemoController->HasTacticalDebugMoveLocation())
	{
		const FVector TacticalLocation = DemoController->GetTacticalDebugMoveLocation();
		const FColor RoleColor = DemoController->GetTacticalDebugColor();
		DrawDebugLine(GetWorld(), HeadLocation, TacticalLocation + FVector(0.f, 0.f, 36.f), RoleColor, false, 0.f, 0, 1.6f);
		DrawDebugSphere(GetWorld(), TacticalLocation + FVector(0.f, 0.f, 24.f), 34.f, 12, RoleColor, false, 0.f, 0, 2.0f);
		DrawDebugString(
			GetWorld(),
			TacticalLocation + FVector(0.f, 0.f, 72.f),
			DemoController->GetLastTacticalDebugSummary(),
			nullptr,
			RoleColor,
			0.f,
			true,
			0.9f);
	}
}

FColor ADemoAICharacter::GetTacticalDebugColor() const
{
	switch (TacticalState)
	{
	case EDemoAITacticalState::Engage:
		return FColor::Red;
	case EDemoAITacticalState::Alert:
		return FColor::Orange;
	case EDemoAITacticalState::LostTarget:
		return FColor::Purple;
	case EDemoAITacticalState::Investigate:
		return FColor::Yellow;
	case EDemoAITacticalState::Search:
		return FColor::Cyan;
	case EDemoAITacticalState::ReturnToPatrol:
		return FColor::Silver;
	case EDemoAITacticalState::Patrol:
	default:
		return FColor::Green;
	}
}

FColor ADemoAICharacter::GetCombatStyleDebugColor() const
{
	switch (CombatStyle)
	{
	case EDemoAICombatStyle::Assaulter:
		return FColor(255, 140, 0);
	case EDemoAICombatStyle::Elite:
		return FColor(255, 215, 0);
	case EDemoAICombatStyle::Rifleman:
	default:
		return FColor(64, 224, 208);
	}
}
