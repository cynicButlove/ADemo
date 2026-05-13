#pragma once

#include "CoreMinimal.h"
#include "DemoAITypes.generated.h"

UENUM(BlueprintType)
enum class EDemoAITacticalState : uint8
{
	Patrol,
	Alert,
	Engage,
	LostTarget,
	Investigate,
	Search,
	ReturnToPatrol
};

UENUM(BlueprintType)
enum class EDemoAITacticalRole : uint8
{
	None,
	Suppress,
	FlankLeft,
	FlankRight
};

namespace DemoAI
{
	inline FString TacticalStateToString(EDemoAITacticalState State)
	{
		switch (State)
		{
		case EDemoAITacticalState::Patrol:
			return TEXT("Patrol");
		case EDemoAITacticalState::Alert:
			return TEXT("Alert");
		case EDemoAITacticalState::Engage:
			return TEXT("Engage");
		case EDemoAITacticalState::LostTarget:
			return TEXT("LostTarget");
		case EDemoAITacticalState::Investigate:
			return TEXT("Investigate");
		case EDemoAITacticalState::Search:
			return TEXT("Search");
		case EDemoAITacticalState::ReturnToPatrol:
			return TEXT("ReturnToPatrol");
		default:
			return TEXT("Unknown");
		}
	}

	inline FString TacticalRoleToString(EDemoAITacticalRole Role)
	{
		switch (Role)
		{
		case EDemoAITacticalRole::Suppress:
			return TEXT("Suppress");
		case EDemoAITacticalRole::FlankLeft:
			return TEXT("FlankLeft");
		case EDemoAITacticalRole::FlankRight:
			return TEXT("FlankRight");
		case EDemoAITacticalRole::None:
		default:
			return TEXT("None");
		}
	}
}
