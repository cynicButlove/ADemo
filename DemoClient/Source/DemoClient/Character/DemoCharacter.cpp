#include "Character/DemoCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "AbilitySystemComponent.h"
#include "Combat/DemoAbilitySystemComponent.h"
#include "Combat/DemoAttributeSet.h"
#include "Weapon/DemoWeaponBase.h"
#include "Weapon/DemoWeaponAKS74U.h"
#include "Weapon/DemoGrenade.h"
#include "Inventory/DemoInventoryComponent.h"
#include "Inventory/DemoLootItem.h"
#include "Inventory/DemoLootContainer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Online/DemoDedicatedServerContainerActor.h"
#include "Online/DemoDedicatedServerLootActor.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "Online/DemoOnlineGameInstance.h"
#include "Net/UnrealNetwork.h"

namespace DemoCharacterPaths
{
	static const TCHAR* const MannyMesh = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	static const TCHAR* const MannyAnimClass = TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C");
}

ADemoCharacter::ADemoCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(33.0f);

	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetRelativeLocation(FVector(-0.96f, 0.f, -153.48f));
	FirstPersonMesh->SetRelativeRotation(FRotator(5.2f, -3.3f, 0.f));
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> ThirdPersonBodyMesh(
			DemoCharacterPaths::MannyMesh);
		static ConstructorHelpers::FClassFinder<UAnimInstance> ThirdPersonAnimClass(
			TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"));

		CharacterMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		CharacterMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		CharacterMesh->SetOwnerNoSee(true);
		CharacterMesh->SetOnlyOwnerSee(false);
		CharacterMesh->SetHiddenInGame(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (ThirdPersonBodyMesh.Succeeded())
		{
			CharacterMesh->SetSkeletalMesh(ThirdPersonBodyMesh.Object);
		}

		if (ThirdPersonAnimClass.Succeeded() && ThirdPersonAnimClass.Class)
		{
			CharacterMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			CharacterMesh->SetAnimInstanceClass(ThirdPersonAnimClass.Class);
		}

		CharacterMesh->bEnableUpdateRateOptimizations = false;
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
		MovementComponent->NetworkMaxSmoothUpdateDistance = 384.0f;
		MovementComponent->NetworkNoSmoothUpdateDistance = 768.0f;
	}

	// Attach remote-only presentation to the mesh so it benefits from ACharacter network smoothing.
	ThirdPersonPlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonPlaceholderMesh"));
	ThirdPersonPlaceholderMesh->SetupAttachment(GetMesh());
	ThirdPersonPlaceholderMesh->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
	ThirdPersonPlaceholderMesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.8f));
	ThirdPersonPlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonPlaceholderMesh->SetCastShadow(true);
	ThirdPersonPlaceholderMesh->SetOwnerNoSee(true);
	ThirdPersonPlaceholderMesh->SetHiddenInGame(true);

	ThirdPersonNameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ThirdPersonNameplate"));
	ThirdPersonNameplate->SetupAttachment(GetMesh());
	ThirdPersonNameplate->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	ThirdPersonNameplate->SetHorizontalAlignment(EHTA_Center);
	ThirdPersonNameplate->SetVerticalAlignment(EVRTA_TextCenter);
	ThirdPersonNameplate->SetWorldSize(28.f);
	ThirdPersonNameplate->SetTextRenderColor(FColor::White);
	ThirdPersonNameplate->SetText(FText::FromString(TEXT("Player")));
	ThirdPersonNameplate->SetOwnerNoSee(true);
	ThirdPersonNameplate->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(
		TEXT("/Engine/BasicShapes/Capsule.Capsule"));
	if (CapsuleMesh.Succeeded())
	{
		ThirdPersonPlaceholderMesh->SetStaticMesh(CapsuleMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FPMannyArms(
		TEXT("/Game/FP_AKS74U_Animation/Demo/FirstPersonArms/Character/Mesh/SK_FP_Manny_Simple.SK_FP_Manny_Simple"));
	if (FPMannyArms.Succeeded())
	{
		FirstPersonMesh->SetSkeletalMesh(FPMannyArms.Object);
		FirstPersonWeaponAttachSocketName = FName(TEXT("hand_r"));

		static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleLoopFinder(
			TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Idle_Loop.A_FP_AKS74U_Idle_Loop"));
		if (IdleLoopFinder.Succeeded())
		{
			AKSArmsIdleLoopSequence = IdleLoopFinder.Object;
		}

		static ConstructorHelpers::FObjectFinder<UAnimSequence> AimLoopFinder(
			TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Aim_Loop.A_FP_AKS74U_Aim_Loop"));
		if (AimLoopFinder.Succeeded())
		{
			AKSArmsAimLoopSequence = AimLoopFinder.Object;
		}

		// 与 FirstPerson_AnimBP 骨架/状态机不一致时会导致垂手 + 蒙太奇无效；SingleNode 可正确播放资源包 Idle 与动态 Montage
		FirstPersonMesh->SetAnimInstanceClass(UAnimSingleNodeInstance::StaticClass());
		FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegacyArms(
			TEXT("/Game/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms"));
		if (LegacyArms.Succeeded())
		{
			FirstPersonMesh->SetSkeletalMesh(LegacyArms.Object);
		}
		FirstPersonWeaponAttachSocketName = FName(TEXT("GripPoint"));

		static ConstructorHelpers::FObjectFinder<UAnimBlueprint> AnimBPFinder(
			TEXT("/Game/FirstPersonArms/Animations/FirstPerson_AnimBP"));
		if (AnimBPFinder.Succeeded())
		{
			FirstPersonMesh->SetAnimInstanceClass(AnimBPFinder.Object->GeneratedClass);
		}
	}

	UniformWeaponClass = ADemoWeaponAKS74U::StaticClass();

	AbilitySystemComponent = CreateDefaultSubobject<UDemoAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UDemoAttributeSet>(TEXT("AttributeSet"));
	InventoryComponent = CreateDefaultSubobject<UDemoInventoryComponent>(TEXT("InventoryComponent"));

	static ConstructorHelpers::FObjectFinder<UInputAction> IAInventory(
		TEXT("/Game/FirstPerson/Input/Actions/IA_Inventory"));
	if (IAInventory.Succeeded())
	{
		InventoryAction = IAInventory.Object;
	}

	GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = DefaultWalkSpeed * 0.5f;

	WeaponSlots.SetNum(3);
	CurrentGrenades = MaxGrenades;
}

UAbilitySystemComponent* ADemoCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ADemoCharacter::GetHealthPercent() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetHealth() / FMath::Max(AttributeSet->GetMaxHealth(), 1.f);
	}
	return 0.f;
}

float ADemoCharacter::GetArmorPercent() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetArmor() / FMath::Max(AttributeSet->GetMaxArmor(), 1.f);
	}
	return 0.f;
}

float ADemoCharacter::GetStaminaPercent() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetStamina() / FMath::Max(AttributeSet->GetMaxStamina(), 1.f);
	}
	return 0.f;
}

void ADemoCharacter::ApplyRecoil(float PitchOffset, float YawOffset)
{
	RecoilOffset.X += PitchOffset;
	RecoilOffset.Y += YawOffset;
	AddControllerPitchInput(PitchOffset);
	AddControllerYawInput(YawOffset);
}

void ADemoCharacter::ApplyDedicatedServerSnapshot(const FDemoDedicatedServerPlayerSnapshot& Snapshot)
{
	constexpr float PositionCorrectionThreshold = 12.0f;

	if (FVector::DistSquared(GetActorLocation(), Snapshot.Position) > FMath::Square(PositionCorrectionThreshold))
	{
		SetActorLocation(Snapshot.Position, false, nullptr, ETeleportType::TeleportPhysics);
	}

	SetActorRotation(FRotator(0.0f, Snapshot.Rotation.Yaw, 0.0f));

	if (Controller)
	{
		Controller->SetControlRotation(Snapshot.Rotation);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = Snapshot.Velocity;
	}

	if (AttributeSet)
	{
		AttributeSet->SetHealth(Snapshot.Health);
		AttributeSet->SetArmor(Snapshot.Armor);
		AttributeSet->SetStamina(Snapshot.Stamina);
	}

	ApplyDedicatedServerAliveState(Snapshot.bAlive);
}

void ADemoCharacter::RestoreFirstPersonArmBaseLoop()
{
	UpdateFirstPersonArmBaseLoop();
}

void ADemoCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilityActorInfo();
	RefreshThirdPersonBodyPresentation();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 1);
			}
			if (CombatMappingContext)
			{
				Subsystem->AddMappingContext(CombatMappingContext, 0);
			}
		}
	}

	if (AttributeSet)
	{
		AttributeSet->OnHealthDepleted.AddDynamic(this, &ADemoCharacter::OnHealthDepleted);
		AttributeSet->OnDamageReceived.AddDynamic(this, &ADemoCharacter::OnDamageReceived);
	}

	UpdateFirstPersonArmBaseLoop();
	ApplyAliveState(!bIsDead);
	UpdateThirdPersonPresentation();

	SpawnWeapons();
}

void ADemoCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		if (!CharacterMesh->GetAnimInstance())
		{
			RefreshThirdPersonBodyPresentation();
		}
	}

	UpdateThirdPersonPresentation();
	ProcessWorkbenchHotkeys();

	if (bIsDead) return;

	const bool bWeaponIsFiring = CurrentWeapon && CurrentWeapon->GetIsFiring();

	// Recoil recovery starts after the spray ends so the fixed pattern is not pulled off-track mid-burst.
	if (!bWeaponIsFiring && !RecoilOffset.IsNearlyZero(0.01f))
	{
		FVector2D Recovery = RecoilOffset * RecoilRecoverySpeed * DeltaTime;
		RecoilOffset -= Recovery;
		AddControllerPitchInput(-Recovery.X);
		AddControllerYawInput(-Recovery.Y);
	}

	if (ShouldUseDedicatedServerPath())
	{
		SendDedicatedServerInput(DeltaTime);
	}

	if (AttributeSet && !ShouldUseDedicatedServerPath())
	{
		// Stamina drain / recovery
		if (bIsSprinting && GetCharacterMovement()->IsMovingOnGround() && GetVelocity().SizeSquared() > 100.f)
		{
			float NewStamina = AttributeSet->GetStamina() - StaminaDrainRate * DeltaTime;
			if (NewStamina <= 0.f)
			{
				NewStamina = 0.f;
				bIsSprinting = false;
				UpdateMovementSpeed();
			}
			AttributeSet->SetStamina(NewStamina);
		}
		else if (AttributeSet->GetStamina() < AttributeSet->GetMaxStamina())
		{
			float NewStamina = FMath::Min(
				AttributeSet->GetStamina() + StaminaRecoveryRate * DeltaTime,
				AttributeSet->GetMaxStamina());
			AttributeSet->SetStamina(NewStamina);
		}
	}

	// Expire old damage direction indicators (keep for 2 seconds)
	float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = DamageDirectionTimestamps.Num() - 1; i >= 0; --i)
	{
		if (Now - DamageDirectionTimestamps[i] > 2.0f)
		{
			DamageDirectionTimestamps.RemoveAt(i);
			RecentDamageDirections.RemoveAt(i);
		}
	}
}

void ADemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADemoCharacter::HandleMove);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADemoCharacter::HandleMove);
		EIC->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ADemoCharacter::HandleMove);
	}
	if (LookAction)
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADemoCharacter::HandleLook);
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleJump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADemoCharacter::HandleStopJump);
	}
	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleSprintStart);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADemoCharacter::HandleSprintStop);
	}
	if (CrouchAction)
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleCrouch);
	if (FireAction)
	{
		EIC->BindAction(FireAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleFireStart);
		EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &ADemoCharacter::HandleFireStop);
	}
	if (ADSAction)
	{
		EIC->BindAction(ADSAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleADSStart);
		EIC->BindAction(ADSAction, ETriggerEvent::Completed, this, &ADemoCharacter::HandleADSStop);
	}
	if (ReloadAction)
		EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleReload);
	if (InteractAction)
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleInteract);
	if (WeaponSlot1Action)
		EIC->BindAction(WeaponSlot1Action, ETriggerEvent::Started, this, &ADemoCharacter::HandleWeaponSlot1);
	if (WeaponSlot2Action)
		EIC->BindAction(WeaponSlot2Action, ETriggerEvent::Started, this, &ADemoCharacter::HandleWeaponSlot2);
	if (WeaponSlot3Action)
		EIC->BindAction(WeaponSlot3Action, ETriggerEvent::Started, this, &ADemoCharacter::HandleWeaponSlot3);
	if (GrenadeAction)
		EIC->BindAction(GrenadeAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleGrenadeThrow);
	if (InventoryAction)
		EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &ADemoCharacter::HandleInventoryToggle);
}

void ADemoCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilityActorInfo();

	UpdateThirdPersonPresentation();
}

void ADemoCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilityActorInfo();
	UpdateThirdPersonPresentation();
}

void ADemoCharacter::InitializeAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void ADemoCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADemoCharacter, bIsSprinting);
	DOREPLIFETIME(ADemoCharacter, bIsADS);
	DOREPLIFETIME(ADemoCharacter, bIsDead);
}

// ---- Input Handlers ----

void ADemoCharacter::HandleMove(const FInputActionValue& Value)
{
	if (bIsDead) return;
	const FVector2D Input = Value.Get<FVector2D>();
	CachedDedicatedMoveInput = Input;
	if (!Controller) return;

	const FRotator YawRot(0, Controller->GetControlRotation().Yaw, 0);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ADemoCharacter::HandleLook(const FInputActionValue& Value)
{
	if (bIsDead) return;
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ADemoCharacter::HandleJump(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (bIsSprinting)
	{
		bIsSprinting = false;
		UpdateMovementSpeed();
		if (!HasAuthority() && !ShouldUseDedicatedServerPath())
		{
			ServerSetSprintState(false);
		}
	}
	Jump();
}

void ADemoCharacter::HandleStopJump(const FInputActionValue& Value)
{
	StopJumping();
}

void ADemoCharacter::HandleSprintStart(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (AttributeSet && AttributeSet->GetStamina() > 0.f && !bIsCrouched)
	{
		bIsSprinting = true;

		if (bIsADS)
		{
			bIsADS = false;
			if (CurrentWeapon) CurrentWeapon->SetADS(false);
		}
		if (CurrentWeapon && CurrentWeapon->GetIsFiring())
		{
			CurrentWeapon->StopFire();
		}
		UpdateMovementSpeed();
		if (!HasAuthority() && !ShouldUseDedicatedServerPath())
		{
			ServerSetSprintState(true);
		}
	}
}

void ADemoCharacter::HandleSprintStop(const FInputActionValue& Value)
{
	bIsSprinting = false;
	UpdateMovementSpeed();
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSetSprintState(false);
	}
}

void ADemoCharacter::HandleCrouch(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		if (bIsSprinting) bIsSprinting = false;
		Crouch();
	}
	UpdateMovementSpeed();
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSetSprintState(false);
	}
}

void ADemoCharacter::HandleFireStart(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (bIsSprinting)
	{
		bIsSprinting = false;
		UpdateMovementSpeed();
	}
	if (ShouldUseDedicatedServerPath())
	{
		if (GetGameInstance())
		{
			if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
			{
				DedicatedServer->SendFire();
			}
		}
		return;
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFire();
		if (!HasAuthority())
		{
			ServerStartFire();
		}
	}
}

void ADemoCharacter::HandleFireStop(const FInputActionValue& Value)
{
	if (ShouldUseDedicatedServerPath()) return;
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
		if (!HasAuthority())
		{
			ServerStopFire();
		}
	}
}

void ADemoCharacter::HandleADSStart(const FInputActionValue& Value)
{
	if (bIsDead || bIsSprinting) return;
	bIsADS = true;
	if (CurrentWeapon) CurrentWeapon->SetADS(true);
	UpdateMovementSpeed();
	UpdateFirstPersonArmBaseLoop();
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSetADSState(true);
	}
}

void ADemoCharacter::HandleADSStop(const FInputActionValue& Value)
{
	bIsADS = false;
	if (CurrentWeapon) CurrentWeapon->SetADS(false);
	UpdateMovementSpeed();
	UpdateFirstPersonArmBaseLoop();
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSetADSState(false);
	}
}

void ADemoCharacter::HandleReload(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (ShouldUseDedicatedServerPath())
	{
		if (GetGameInstance())
		{
			if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
			{
				FString ErrorMessage;
				DedicatedServer->SendReload(&ErrorMessage);
			}
		}
		return;
	}
	if (CurrentWeapon) CurrentWeapon->StartReload();
	if (!HasAuthority())
	{
		ServerReloadCurrentWeapon();
	}
}

void ADemoCharacter::HandleInteract(const FInputActionValue& Value)
{
	if (bIsDead) return;

	if (bWeaponWorkbenchOpen)
	{
		bWeaponWorkbenchOpen = false;
		return;
	}

	if (ShouldUseDedicatedServerPath())
	{
		if (bInventoryOpen)
		{
			bInventoryOpen = false;
			return;
		}

		if (!CurrentDedicatedContainerId.IsEmpty())
		{
			CloseContainer();
			return;
		}

		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC || !GetWorld() || !GetGameInstance())
		{
			return;
		}

		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		if (GetWorld()->LineTraceSingleByChannel(
				Hit,
				CamLoc,
				CamLoc + CamRot.Vector() * 320.0f,
				ECC_Visibility,
				Params))
		{
			if (ADemoDedicatedServerLootActor* DedicatedLoot = Cast<ADemoDedicatedServerLootActor>(Hit.GetActor()))
			{
				if (UDemoDedicatedServerSubsystem* DedicatedServer =
						GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
				{
					FString ErrorMessage;
					DedicatedServer->PickupLoot(DedicatedLoot->GetLootId(), &ErrorMessage);
				}
				return;
			}

			if (ADemoDedicatedServerContainerActor* DedicatedContainer =
					Cast<ADemoDedicatedServerContainerActor>(Hit.GetActor()))
			{
				if (UDemoDedicatedServerSubsystem* DedicatedServer =
						GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
				{
					FString ErrorMessage;
					if (DedicatedServer->InteractContainer(DedicatedContainer->GetContainerId(), &ErrorMessage))
					{
						CurrentDedicatedContainerId = DedicatedContainer->GetContainerId();
					}
				}
			}
		}

		return;
	}

	if (bInventoryOpen)
	{
		bInventoryOpen = false;
		return;
	}

	if (CurrentContainer && CurrentContainer->IsOpened())
	{
		CloseContainer();
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FVector MyLoc = GetActorLocation();
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	FVector ViewDir = CamRot.Vector();

	float InteractRange = 400.f;
	float BestScore = -1.f;
	AActor* BestTarget = nullptr;

	for (TActorIterator<ADemoLootItem> It(GetWorld()); It; ++It)
	{
		float Dist = FVector::Dist(MyLoc, It->GetActorLocation());
		if (Dist > InteractRange) continue;

		FVector ToActor = (It->GetActorLocation() - CamLoc).GetSafeNormal();
		float Dot = FVector::DotProduct(ViewDir, ToActor);
		if (Dot < 0.3f) continue;

		float Score = Dot / FMath::Max(Dist, 1.f) * 1000.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = *It;
		}
	}

	for (TActorIterator<ADemoLootContainer> It(GetWorld()); It; ++It)
	{
		float Dist = FVector::Dist(MyLoc, It->GetActorLocation());
		if (Dist > InteractRange) continue;

		FVector ToActor = (It->GetActorLocation() - CamLoc).GetSafeNormal();
		float Dot = FVector::DotProduct(ViewDir, ToActor);
		if (Dot < 0.3f) continue;

		float Score = Dot / FMath::Max(Dist, 1.f) * 1000.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = *It;
		}
	}

	if (!BestTarget) return;

	if (ADemoLootItem* LootItem = Cast<ADemoLootItem>(BestTarget))
	{
		if (InventoryComponent)
		{
			if (InventoryComponent->AddItem(LootItem->GetItemID(), LootItem->GetQuantity()))
			{
				LootItem->Destroy();
			}
		}
		return;
	}

	if (ADemoLootContainer* Container = Cast<ADemoLootContainer>(BestTarget))
	{
		if (!Container->IsOpened())
		{
			Container->Interact(this);
		}
		if (Container->IsOpened())
		{
			CurrentContainer = Container;
		}
		return;
	}
}

void ADemoCharacter::HandleInventoryToggle(const FInputActionValue& Value)
{
	if (bIsDead) return;

	if (bWeaponWorkbenchOpen)
	{
		bWeaponWorkbenchOpen = false;
		return;
	}

	if (ShouldUseDedicatedServerPath())
	{
		if (!CurrentDedicatedContainerId.IsEmpty())
		{
			CloseContainer();
			return;
		}

		bInventoryOpen = !bInventoryOpen;
		return;
	}

	if (CurrentContainer)
	{
		CloseContainer();
		return;
	}

	bInventoryOpen = !bInventoryOpen;
}

void ADemoCharacter::HandleWeaponWorkbenchToggle()
{
	if (bIsDead)
	{
		return;
	}

	if (bInventoryOpen)
	{
		bInventoryOpen = false;
	}

	if (IsContainerOpen())
	{
		CloseContainer();
	}

	bWeaponWorkbenchOpen = !bWeaponWorkbenchOpen;
}

void ADemoCharacter::HandleToggleMuzzleAttachment()
{
	if (!bWeaponWorkbenchOpen) return;
	ToggleWeaponAttachment(EDemoWeaponAttachmentSlot::Muzzle);
}

void ADemoCharacter::HandleToggleGripAttachment()
{
	if (!bWeaponWorkbenchOpen) return;
	ToggleWeaponAttachment(EDemoWeaponAttachmentSlot::Grip);
}

void ADemoCharacter::HandleToggleMagazineAttachment()
{
	if (!bWeaponWorkbenchOpen) return;
	ToggleWeaponAttachment(EDemoWeaponAttachmentSlot::Magazine);
}

void ADemoCharacter::HandleToggleOpticAttachment()
{
	if (!bWeaponWorkbenchOpen) return;
	ToggleWeaponAttachment(EDemoWeaponAttachmentSlot::Optic);
}

void ADemoCharacter::HandleClearWeaponAttachments()
{
	if (!bWeaponWorkbenchOpen) return;
	ClearWeaponAttachments();
}

void ADemoCharacter::ProcessWorkbenchHotkeys()
{
	if (!IsLocallyControlled() || bIsDead)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::B))
	{
		HandleWeaponWorkbenchToggle();
	}

	if (!bWeaponWorkbenchOpen)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::F1))
	{
		HandleToggleMuzzleAttachment();
	}
	if (PC->WasInputKeyJustPressed(EKeys::F2))
	{
		HandleToggleGripAttachment();
	}
	if (PC->WasInputKeyJustPressed(EKeys::F3))
	{
		HandleToggleMagazineAttachment();
	}
	if (PC->WasInputKeyJustPressed(EKeys::F4))
	{
		HandleToggleOpticAttachment();
	}
	if (PC->WasInputKeyJustPressed(EKeys::F5))
	{
		HandleClearWeaponAttachments();
	}
}

void ADemoCharacter::ToggleWeaponAttachment(EDemoWeaponAttachmentSlot Slot)
{
	if (!CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->ToggleAttachment(Slot);

	if (!HasAuthority())
	{
		ServerToggleWeaponAttachment(CurrentWeaponIndex, Slot);
	}
}

void ADemoCharacter::ClearWeaponAttachments()
{
	if (!CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->ClearAttachments();

	if (!HasAuthority())
	{
		ServerClearWeaponAttachments(CurrentWeaponIndex);
	}
}

bool ADemoCharacter::IsContainerOpen() const
{
	if (!CurrentDedicatedContainerId.IsEmpty())
	{
		return true;
	}
	return CurrentContainer != nullptr && CurrentContainer->IsOpened();
}

void ADemoCharacter::CloseContainer()
{
	CurrentContainer = nullptr;
	CurrentDedicatedContainerId.Reset();
}

void ADemoCharacter::TakeContainerItem(int32 Index)
{
	if (ShouldUseDedicatedServerPath())
	{
		if (CurrentDedicatedContainerId.IsEmpty() || !GetGameInstance())
		{
			return;
		}

		if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
		{
			FString ErrorMessage;
			DedicatedServer->TakeContainerItem(CurrentDedicatedContainerId, Index, &ErrorMessage);
		}
		return;
	}

	if (!CurrentContainer || !CurrentContainer->IsOpened()) return;
	if (!InventoryComponent) return;

	CurrentContainer->TakeItem(this, Index);

	bool bEmpty = true;
	for (const auto& Slot : CurrentContainer->GetContents())
	{
		if (!Slot.IsEmpty()) { bEmpty = false; break; }
	}
	if (bEmpty) CloseContainer();
}

void ADemoCharacter::HandleDedicatedInventorySlotAction(int32 Index)
{
	if (!ShouldUseDedicatedServerPath() || !bInventoryOpen || !GetGameInstance() || !InventoryComponent)
	{
		return;
	}

	const TArray<FDemoInventorySlot>& Items = InventoryComponent->GetAllItems();
	if (!Items.IsValidIndex(Index) || Items[Index].IsEmpty())
	{
		return;
	}

	if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
	{
		FString ErrorMessage;
		FDemoItemData ItemData;
		const bool bHasItemData = InventoryComponent->FindItemData(Items[Index].ItemID, ItemData);
		const FString ItemIdUpper = Items[Index].ItemID.ToString().ToUpper();
		const bool bIsMedicalItem =
			(bHasItemData &&
			 ItemData.ItemType == EDemoItemType::Medical &&
			 ItemData.HealAmount > 0.0f) ||
			ItemIdUpper.Contains(TEXT("BANDAGE")) ||
			ItemIdUpper.Contains(TEXT("MED")) ||
			ItemIdUpper.Contains(TEXT("STIM")) ||
			ItemIdUpper.Contains(TEXT("PAIN"));
		const bool bIsArmorItem =
			(bHasItemData &&
			 ItemData.ItemType == EDemoItemType::Armor &&
			 ItemData.ArmorValue > 0.0f) ||
			ItemIdUpper.Contains(TEXT("PLATE")) ||
			ItemIdUpper.Contains(TEXT("ARMOR")) ||
			ItemIdUpper.Contains(TEXT("VEST"));
		const bool bIsAmmoItem =
			(bHasItemData &&
			 ItemData.ItemType == EDemoItemType::Ammo &&
			 ItemData.AmmoCount > 0) ||
			ItemIdUpper.Contains(TEXT("AMMO")) ||
			ItemIdUpper.Contains(TEXT("545")) ||
			ItemIdUpper.Contains(TEXT("556")) ||
			ItemIdUpper.Contains(TEXT("762"));

		if (bIsMedicalItem || bIsArmorItem || bIsAmmoItem)
		{
			DedicatedServer->UseItem(Index, &ErrorMessage);
		}
		else
		{
			DedicatedServer->DropItem(Index, &ErrorMessage);
		}
	}
}

void ADemoCharacter::HandleWeaponSlot1(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (ShouldUseDedicatedServerPath() && bInventoryOpen)
	{
		HandleDedicatedInventorySlotAction(0);
		return;
	}
	if (IsContainerOpen()) { TakeContainerItem(0); return; }
	SwitchToWeaponSlot(0);
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSwitchWeaponSlot(0);
	}
}

void ADemoCharacter::HandleWeaponSlot2(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (ShouldUseDedicatedServerPath() && bInventoryOpen)
	{
		HandleDedicatedInventorySlotAction(1);
		return;
	}
	if (IsContainerOpen()) { TakeContainerItem(1); return; }
	SwitchToWeaponSlot(1);
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSwitchWeaponSlot(1);
	}
}

void ADemoCharacter::HandleWeaponSlot3(const FInputActionValue& Value)
{
	if (bIsDead) return;
	if (ShouldUseDedicatedServerPath() && bInventoryOpen)
	{
		HandleDedicatedInventorySlotAction(2);
		return;
	}
	if (IsContainerOpen()) { TakeContainerItem(2); return; }
	SwitchToWeaponSlot(2);
	if (!HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ServerSwitchWeaponSlot(2);
	}
}

void ADemoCharacter::HandleGrenadeThrow(const FInputActionValue& Value)
{
	if (ShouldUseDedicatedServerPath()) return;
	if (bIsDead || CurrentGrenades <= 0 || !GrenadeClass || !GetWorld()) return;

	FVector SpawnLoc = FirstPersonCamera->GetComponentLocation() +
		FirstPersonCamera->GetForwardVector() * 80.f;
	FRotator SpawnRot = FirstPersonCamera->GetComponentRotation();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADemoGrenade* Grenade = GetWorld()->SpawnActor<ADemoGrenade>(GrenadeClass, SpawnLoc, SpawnRot, Params);
	if (Grenade)
	{
		FVector ThrowDir = FirstPersonCamera->GetForwardVector() + FVector(0, 0, 0.2f);
		ThrowDir.Normalize();
		Grenade->Launch(ThrowDir * GrenadeThrowForce);
		CurrentGrenades--;
	}
}

// ---- Weapon Management ----

void ADemoCharacter::SpawnWeapons()
{
	if (!GetWorld()) return;

	auto SpawnWeaponToSlot = [this](TSubclassOf<ADemoWeaponBase> WeaponClass, int32 Slot) -> ADemoWeaponBase*
	{
		if (!WeaponClass) return nullptr;

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADemoWeaponBase* Weapon = GetWorld()->SpawnActor<ADemoWeaponBase>(
			WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);

		if (Weapon)
		{
			WeaponSlots[Slot] = Weapon;
			Weapon->SetOwningCharacter(this);
			Weapon->AttachToComponent(
				FirstPersonMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				FirstPersonWeaponAttachSocketName);
			Weapon->SetActorRelativeTransform(FirstPersonWeaponSocketOffset, false, nullptr, ETeleportType::ResetPhysics);

			Weapon->OnWeaponHit.AddDynamic(this, &ADemoCharacter::OnWeaponHitCallback);
			Weapon->SetActorHiddenInGame(true);
		}
		return Weapon;
	};

	if (UniformWeaponClass)
	{
		SpawnWeaponToSlot(UniformWeaponClass, 0);
		SpawnWeaponToSlot(UniformWeaponClass, 1);
		SpawnWeaponToSlot(UniformWeaponClass, 2);
	}
	else
	{
		SpawnWeaponToSlot(DefaultWeaponClass, 0);
		SpawnWeaponToSlot(SecondaryWeaponClass, 1);
		SpawnWeaponToSlot(SidearmWeaponClass, 2);
	}

	SwitchToWeaponSlot(0);
}

void ADemoCharacter::SwitchToWeaponSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= WeaponSlots.Num()) return;
	if (!WeaponSlots[SlotIndex]) return;
	if (CurrentWeaponIndex == SlotIndex) return;

	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
		CurrentWeapon->SetADS(false);
		CurrentWeapon->AbortReload();
		CurrentWeapon->SetActorHiddenInGame(true);
	}

	bIsADS = false;
	CurrentWeaponIndex = SlotIndex;
	CurrentWeapon = WeaponSlots[SlotIndex];
	CurrentWeapon->SetActorHiddenInGame(false);
	CurrentWeapon->PlayEquipAnimation();
	UpdateMovementSpeed();
}

void ADemoCharacter::UpdateMovementSpeed()
{
	float Speed = DefaultWalkSpeed;

	if (bIsSprinting)
		Speed *= SprintSpeedMultiplier;
	else if (bIsADS)
		Speed *= ADSSpeedMultiplier;

	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void ADemoCharacter::UpdateFirstPersonArmBaseLoop()
{
	if (!FirstPersonMesh || FirstPersonMesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		return;
	}
	UAnimSingleNodeInstance* SingleInst = FirstPersonMesh->GetSingleNodeInstance();
	if (!SingleInst)
	{
		return;
	}
	UAnimSequence* Loop = nullptr;
	if (bIsADS && AKSArmsAimLoopSequence)
	{
		Loop = AKSArmsAimLoopSequence;
	}
	else if (AKSArmsIdleLoopSequence)
	{
		Loop = AKSArmsIdleLoopSequence;
	}
	if (Loop)
	{
		SingleInst->SetAnimationAsset(Loop, true, 1.f);
	}
}

void ADemoCharacter::UpdateThirdPersonPresentation()
{
	UpdateThirdPersonNameplate();

	if (!ThirdPersonPlaceholderMesh || !ThirdPersonNameplate)
	{
		return;
	}

	const bool bHasRenderableMesh =
		GetMesh() &&
		GetMesh()->GetSkeletalMeshAsset() != nullptr;
	const bool bShouldShowRemotePresentation =
		GetNetMode() != NM_DedicatedServer &&
		!IsLocallyControlled() &&
		!bIsDead;
	const bool bShouldShowPlaceholder =
		bShouldShowRemotePresentation &&
		!bHasRenderableMesh;
	const bool bShouldShowNameplate =
		bShouldShowRemotePresentation;

	ThirdPersonPlaceholderMesh->SetHiddenInGame(!bShouldShowPlaceholder);
	ThirdPersonNameplate->SetHiddenInGame(!bShouldShowNameplate);
}

void ADemoCharacter::RefreshThirdPersonBodyPresentation()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	CharacterMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	CharacterMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	CharacterMesh->SetOwnerNoSee(true);
	CharacterMesh->SetOnlyOwnerSee(false);
	CharacterMesh->SetHiddenInGame(false);
	CharacterMesh->SetComponentTickEnabled(true);
	CharacterMesh->bPauseAnims = false;
	CharacterMesh->bEnableUpdateRateOptimizations = false;
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (!CharacterMesh->GetSkeletalMeshAsset())
	{
		if (USkeletalMesh* MannyMesh = LoadObject<USkeletalMesh>(nullptr, DemoCharacterPaths::MannyMesh))
		{
			CharacterMesh->SetSkeletalMesh(MannyMesh);
		}
	}

	if (UClass* MannyAnimClass = LoadClass<UAnimInstance>(nullptr, DemoCharacterPaths::MannyAnimClass))
	{
		CharacterMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		CharacterMesh->SetAnimInstanceClass(MannyAnimClass);
	}

	CharacterMesh->InitAnim(true);
}

void ADemoCharacter::UpdateThirdPersonNameplate()
{
	if (!ThirdPersonNameplate)
	{
		return;
	}

	FString DisplayName = TEXT("Player");
	if (const APlayerState* PS = GetPlayerState())
	{
		if (!PS->GetPlayerName().IsEmpty())
		{
			DisplayName = PS->GetPlayerName();
		}
	}

	ThirdPersonNameplate->SetText(FText::FromString(DisplayName));
}

void ADemoCharacter::ApplyAliveState(bool bIsAlive)
{
	if (bIsAlive)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		if (GetMesh())
		{
			GetMesh()->SetHiddenInGame(false, true);
		}
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(true);
		}
		if (CurrentWeapon)
		{
			CurrentWeapon->SetActorHiddenInGame(false);
		}
		UpdateThirdPersonPresentation();
		return;
	}

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetMesh())
	{
		GetMesh()->SetHiddenInGame(true, true);
	}
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(false);
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
		CurrentWeapon->SetActorHiddenInGame(true);
	}
	UpdateThirdPersonPresentation();
}

bool ADemoCharacter::ShouldUseDedicatedServerPath() const
{
	const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance());
	if (!GI || !GI->ShouldUseDedicatedServer() || !IsLocallyControlled() || !GetGameInstance())
	{
		return false;
	}

	const UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
	return DedicatedServer && DedicatedServer->IsInMatch();
}

void ADemoCharacter::SendDedicatedServerInput(float DeltaTime)
{
	if (!ShouldUseDedicatedServerPath() || !Controller || !GetGameInstance())
	{
		return;
	}

	DedicatedInputSendAccumulator += DeltaTime;
	if (DedicatedInputSendAccumulator < DedicatedInputSendInterval)
	{
		return;
	}

	DedicatedInputSendAccumulator = FMath::Max(0.0f, DedicatedInputSendAccumulator - DedicatedInputSendInterval);

	if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
	{
		DedicatedServer->SendInput(
			++DedicatedInputSequence,
			CachedDedicatedMoveInput,
			Controller->GetControlRotation(),
			bIsSprinting,
			bIsCrouched);
	}
}

void ADemoCharacter::ApplyDedicatedServerAliveState(bool bIsAlive)
{
	bIsDead = !bIsAlive;
	ApplyAliveState(bIsAlive);
}

void ADemoCharacter::OnRep_CombatState()
{
	UpdateMovementSpeed();
	UpdateFirstPersonArmBaseLoop();
}

void ADemoCharacter::OnRep_IsDead()
{
	ApplyAliveState(!bIsDead);
}

void ADemoCharacter::ServerSetSprintState_Implementation(bool bNewSprinting)
{
	if (ShouldUseDedicatedServerPath() || bIsDead)
	{
		return;
	}

	if (bNewSprinting && bIsCrouched)
	{
		bNewSprinting = false;
	}

	if (bNewSprinting && AttributeSet && AttributeSet->GetStamina() <= 0.f)
	{
		bNewSprinting = false;
	}

	bIsSprinting = bNewSprinting;
	if (bIsSprinting)
	{
		bIsADS = false;
		if (CurrentWeapon)
		{
			CurrentWeapon->SetADS(false);
			CurrentWeapon->StopFire();
		}
	}
	UpdateMovementSpeed();
}

void ADemoCharacter::ServerSetADSState_Implementation(bool bNewADS)
{
	if (ShouldUseDedicatedServerPath() || bIsDead)
	{
		return;
	}

	bIsADS = bNewADS && !bIsSprinting;
	if (CurrentWeapon)
	{
		CurrentWeapon->SetADS(bIsADS);
	}
	UpdateMovementSpeed();
}

void ADemoCharacter::ServerStartFire_Implementation()
{
	if (ShouldUseDedicatedServerPath() || bIsDead || !CurrentWeapon)
	{
		return;
	}

	if (bIsSprinting)
	{
		bIsSprinting = false;
		UpdateMovementSpeed();
	}

	CurrentWeapon->StartFire();
}

void ADemoCharacter::ServerStopFire_Implementation()
{
	if (ShouldUseDedicatedServerPath() || !CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->StopFire();
}

void ADemoCharacter::ServerReloadCurrentWeapon_Implementation()
{
	if (ShouldUseDedicatedServerPath() || bIsDead || !CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->StartReload();
}

void ADemoCharacter::ServerSwitchWeaponSlot_Implementation(int32 SlotIndex)
{
	if (ShouldUseDedicatedServerPath() || bIsDead)
	{
		return;
	}

	SwitchToWeaponSlot(SlotIndex);
}

void ADemoCharacter::ServerToggleWeaponAttachment_Implementation(int32 SlotIndex, EDemoWeaponAttachmentSlot Slot)
{
	if (ShouldUseDedicatedServerPath() || bIsDead)
	{
		return;
	}

	if (!WeaponSlots.IsValidIndex(SlotIndex) || !WeaponSlots[SlotIndex])
	{
		return;
	}

	WeaponSlots[SlotIndex]->ToggleAttachment(Slot);
}

void ADemoCharacter::ServerClearWeaponAttachments_Implementation(int32 SlotIndex)
{
	if (ShouldUseDedicatedServerPath() || bIsDead)
	{
		return;
	}

	if (!WeaponSlots.IsValidIndex(SlotIndex) || !WeaponSlots[SlotIndex])
	{
		return;
	}

	WeaponSlots[SlotIndex]->ClearAttachments();
}

void ADemoCharacter::ClientNotifyHitConfirmed_Implementation(AActor* HitActor, bool bKilledTarget)
{
	OnHitConfirmed.Broadcast(HitActor, FHitResult());
	if (bKilledTarget)
	{
		OnKillConfirmed.Broadcast(HitActor);
	}
}

void ADemoCharacter::ClientNotifyDamageTaken_Implementation(float DamageAmount, AActor* DamageCauser)
{
	OnDamageTaken.Broadcast(DamageAmount, DamageCauser);

	if (DamageCauser)
	{
		const FVector Dir = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		RecentDamageDirections.Add(Dir);
		DamageDirectionTimestamps.Add(GetWorld()->GetTimeSeconds());
	}
}

// ---- Combat Callbacks ----

void ADemoCharacter::OnWeaponHitCallback(AActor* HitActor, const FHitResult& HitResult)
{
	ADemoCharacter* HitCharacter = Cast<ADemoCharacter>(HitActor);
	const bool bKilledCharacter = HitCharacter && HitCharacter->GetIsDead();

	if (HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ClientNotifyHitConfirmed(HitActor, bKilledCharacter);
	}
	else
	{
		OnHitConfirmed.Broadcast(HitActor, HitResult);
		if (bKilledCharacter)
		{
			OnKillConfirmed.Broadcast(HitActor);
		}
	}
}

void ADemoCharacter::OnHealthDepleted(AActor* KillerActor)
{
	if (bIsDead) return;

	if (!ShouldUseDedicatedServerPath() && !HasAuthority())
	{
		return;
	}

	Die();
}

void ADemoCharacter::OnDamageReceived(float DamageAmount, AActor* DamageCauser, const FHitResult& HitResult)
{
	if (HasAuthority() && !ShouldUseDedicatedServerPath())
	{
		ClientNotifyDamageTaken(DamageAmount, DamageCauser);
		return;
	}

	OnDamageTaken.Broadcast(DamageAmount, DamageCauser);

	if (DamageCauser)
	{
		const FVector Dir = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		RecentDamageDirections.Add(Dir);
		DamageDirectionTimestamps.Add(GetWorld()->GetTimeSeconds());
	}
}

void ADemoCharacter::Die()
{
	bIsDead = true;
	bWeaponWorkbenchOpen = false;
	ApplyAliveState(false);

	FTimerHandle RespawnTimer;
	GetWorldTimerManager().SetTimer(
		RespawnTimer,
		this,
		&ADemoCharacter::Respawn,
		RespawnDelay,
		false);
}

void ADemoCharacter::Respawn()
{
	bIsDead = false;

	if (AttributeSet)
	{
		AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
		AttributeSet->SetArmor(0.f);
		AttributeSet->SetStamina(AttributeSet->GetMaxStamina());
	}

	ApplyAliveState(true);

	CurrentGrenades = MaxGrenades;

	AActor* PlayerStart = UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass());
	if (PlayerStart)
	{
		SetActorLocationAndRotation(
			PlayerStart->GetActorLocation(),
			PlayerStart->GetActorRotation());
	}
}
