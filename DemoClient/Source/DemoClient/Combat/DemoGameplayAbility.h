#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DemoGameplayAbility.generated.h"

UCLASS(Abstract)
class DEMOCLIENT_API UDemoGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDemoGameplayAbility();
};
