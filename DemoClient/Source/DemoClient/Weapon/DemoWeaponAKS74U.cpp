#include "Weapon/DemoWeaponAKS74U.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

ADemoWeaponAKS74U::ADemoWeaponAKS74U()
{
	WeaponType = EDemoWeaponType::AssaultRifle;
	FireMode = EDemoWeaponFireMode::Automatic;
	bUseStructuredWeaponStats = true;
	StructuredBaseStats.Damage = 28.f;
	StructuredBaseStats.FireRate = 650.f;
	StructuredBaseStats.MagazineCapacity = 30;
	StructuredBaseStats.MaxReserveAmmo = 120;
	StructuredBaseStats.ReloadTime = 2.0f;
	StructuredBaseStats.BaseSpread = 0.42f;
	StructuredBaseStats.SpreadPerShot = 0.14f;
	StructuredBaseStats.MaxSpread = 4.0f;
	StructuredBaseStats.SpreadRecoverySpeed = 5.4f;
	StructuredBaseStats.ADSSpreadMultiplier = 0.28f;
	StructuredBaseStats.HipFireSpreadMultiplier = 1.18f;
	StructuredBaseStats.CrouchSpreadMultiplier = 0.84f;
	StructuredBaseStats.ADSRecoilMultiplier = 0.72f;
	StructuredBaseStats.HeadshotDamageMultiplier = 1.85f;
	StructuredBaseStats.LimbDamageMultiplier = 0.82f;
	ArmMontageSlotName = FName(TEXT("DefaultSlot"));
	MuzzleSocketName = FName(TEXT("Muzzle"));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GunMesh(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Meshes/SK_AK74U.SK_AK74U"));
	if (GunMesh.Succeeded())
	{
		WeaponMesh->SetSkeletalMesh(GunMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqFire(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Fire.A_FP_AKS74U_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqFireADS(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Fire_Aimed.A_FP_AKS74U_Fire_Aimed"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqReload(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Reload.A_FP_AKS74U_Reload"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqReloadADS(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Reload_Aimed.A_FP_AKS74U_Reload_Aimed"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqReloadEmpty(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Reload_Empty.A_FP_AKS74U_Reload_Empty"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqReloadEmptyADS(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Reload_Empty_Aimed.A_FP_AKS74U_Reload_Empty_Aimed"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqEquip(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Equipe.A_FP_AKS74U_Equipe"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqWFire(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_WBP_Reference_Fire.A_WBP_Reference_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqWReload(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_WBP_AKS74U_Reload_UnEmpty.A_WBP_AKS74U_Reload_UnEmpty"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SeqWReloadEmpty(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_WBP_AKS74U_Reload.A_WBP_AKS74U_Reload"));

	if (SeqFire.Succeeded()) { ArmsFireSequence = SeqFire.Object; }
	if (SeqFireADS.Succeeded()) { ArmsFireADSSequence = SeqFireADS.Object; }
	if (SeqReload.Succeeded()) { ArmsReloadSequence = SeqReload.Object; }
	if (SeqReloadADS.Succeeded()) { ArmsReloadADSSequence = SeqReloadADS.Object; }
	if (SeqReloadEmpty.Succeeded()) { ArmsReloadEmptySequence = SeqReloadEmpty.Object; }
	if (SeqReloadEmptyADS.Succeeded()) { ArmsReloadEmptyADSSequence = SeqReloadEmptyADS.Object; }
	if (SeqEquip.Succeeded()) { ArmsEquipSequence = SeqEquip.Object; }
	if (SeqWFire.Succeeded()) { WeaponFireSequence = SeqWFire.Object; }
	if (SeqWReload.Succeeded()) { WeaponReloadSequence = SeqWReload.Object; }
	if (SeqWReloadEmpty.Succeeded()) { WeaponReloadEmptySequence = SeqWReloadEmpty.Object; }
}

void ADemoWeaponAKS74U::SyncReloadTimeFromAnimation()
{
	UAnimSequence* S = ArmsReloadSequence;
	if (!S)
	{
		S = ArmsReloadADSSequence;
	}
	if (S)
	{
		const float ComputedReloadTime = FMath::Max(S->GetPlayLength(), 0.5f);
		if (bUseStructuredWeaponStats)
		{
			StructuredBaseStats.ReloadTime = ComputedReloadTime;
		}
		else
		{
			ReloadTime = ComputedReloadTime;
		}
	}
}
