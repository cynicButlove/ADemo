#include "Combat/DemoDamageExecution.h"
#include "Combat/DemoAttributeSet.h"
#include "AbilitySystemComponent.h"

struct FDemoDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Health);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
	DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamage);

	FDemoDamageStatics()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UDemoAttributeSet, Health, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UDemoAttributeSet, Armor, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UDemoAttributeSet, IncomingDamage, Target, false);
	}
};

static const FDemoDamageStatics& DamageStatics()
{
	static FDemoDamageStatics Statics;
	return Statics;
}

UDemoDamageExecution::UDemoDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().HealthDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().IncomingDamageDef);
}

void UDemoDamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!TargetASC) return;

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float IncomingDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().IncomingDamageDef, EvalParams, IncomingDamage);

	float CurrentArmor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().ArmorDef, EvalParams, CurrentArmor);

	if (IncomingDamage <= 0.f) return;

	// Armor absorbs damage: each armor point reduces 1 damage, armor is consumed
	float ArmorAbsorbed = FMath::Min(CurrentArmor, IncomingDamage * 0.5f);
	float FinalDamage = IncomingDamage - ArmorAbsorbed;

	if (ArmorAbsorbed > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				DamageStatics().ArmorProperty,
				EGameplayModOp::Additive,
				-ArmorAbsorbed));
	}

	if (FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				DamageStatics().HealthProperty,
				EGameplayModOp::Additive,
				-FinalDamage));
	}
}
