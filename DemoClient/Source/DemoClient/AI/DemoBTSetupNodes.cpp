#include "AI/DemoBTSetupNodes.h"
#include "AI/DemoAIController.h"

UBTDecorator_DemoBBIsSet::UBTDecorator_DemoBBIsSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBTDecorator_DemoBBIsSet::InitializeForKey(FName KeyName)
{
	BlackboardKey.SelectedKeyName = KeyName;
	OperationType = static_cast<uint8>(EBasicKeyOperation::Set);
#if WITH_EDITORONLY_DATA
	BasicOperation = EBasicKeyOperation::Set;
#endif
	NodeName = FString::Printf(TEXT("%s Is Set"), *KeyName.ToString());
}

UBTTask_DemoMoveToPatrolKey::UBTTask_DemoMoveToPatrolKey(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BlackboardKey.SelectedKeyName = ADemoAIController::KEY_PatrolLocation;
	AcceptableRadius = FValueOrBBKey_Float(100.f);
	NodeName = TEXT("Move To PatrolLocation");
}
