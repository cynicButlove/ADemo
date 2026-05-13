#pragma once

#include "CoreMinimal.h"
#include "Weapon/DemoWeaponBase.h"
#include "DemoWeaponAKS74U.generated.h"

/**
 * FP_AKS74U_Animation 资源包：金色 AKS74U + UE5 手臂动画序列。
 * 依赖手臂 AnimBP 中含与 ArmMontageSlotName 一致的 Slot（默认 DefaultSlot）。
 */
UCLASS(Blueprintable)
class DEMOCLIENT_API ADemoWeaponAKS74U : public ADemoWeaponBase
{
	GENERATED_BODY()

public:
	ADemoWeaponAKS74U();

protected:
	virtual void SyncReloadTimeFromAnimation() override;
};
