// 仅在编辑器目标下编译：通过控制台命令生成完整 BT_DemoAI 图并保存。

#if WITH_EDITOR

#include "DemoClient.h"
#include "AI/DemoAIController.h"
#include "AI/DemoBTSetupNodes.h"
#include "AI/Tasks/BTTask_DemoFindCover.h"
#include "AI/Tasks/BTTask_DemoInvestigate.h"
#include "AI/Tasks/BTTask_DemoPatrol.h"
#include "AI/Tasks/BTTask_DemoShoot.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "HAL/IConsoleManager.h"
#include "FileHelpers.h"

static void DemoRebuildBTAssets(const TArray<FString>& Args)
{
	static const TCHAR* BTPath = TEXT("/Game/FirstPerson/AI/BT_DemoAI.BT_DemoAI");
	static const TCHAR* BBPath = TEXT("/Game/FirstPerson/AI/BB_DemoAI.BB_DemoAI");

	UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, BBPath);
	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, BTPath);
	if (!BB || !BT)
	{
		UE_LOG(LogDemoClient, Error, TEXT("Demo.RebuildBTAssets: 缺少资产。请先运行 SetupAI 脚本创建 BB_DemoAI / BT_DemoAI。BB=%d BT=%d"), BB != nullptr, BT != nullptr);
		return;
	}

	BT->Modify();
	BT->BlackboardAsset = BB;
	BT->RootNode = nullptr;

	UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(BT, TEXT("Root_Selector"), RF_Transactional);
	Root->NodeName = TEXT("Root");
	BT->RootNode = Root;

	// --- 分支 0：有目标 -> 找掩体 -> 移动到 PatrolLocation -> 射击 ---
	UBTComposite_Sequence* SeqCombat = NewObject<UBTComposite_Sequence>(BT, TEXT("Seq_Combat"), RF_Transactional);
	SeqCombat->NodeName = TEXT("Combat");

	UBTDecorator_DemoBBIsSet* DecTarget = NewObject<UBTDecorator_DemoBBIsSet>(BT, TEXT("Dec_TargetEnemy_Set"), RF_Transactional);
	DecTarget->InitializeForKey(ADemoAIController::KEY_TargetEnemy);

	FBTCompositeChild& BranchCombat = Root->Children.AddDefaulted_GetRef();
	BranchCombat.ChildComposite = SeqCombat;
	BranchCombat.Decorators.Add(DecTarget);

	{
		FBTCompositeChild& Ch = SeqCombat->Children.AddDefaulted_GetRef();
		Ch.ChildTask = NewObject<UBTTask_DemoFindCover>(BT, TEXT("Task_FindCover"), RF_Transactional);
	}
	{
		FBTCompositeChild& Ch = SeqCombat->Children.AddDefaulted_GetRef();
		Ch.ChildTask = NewObject<UBTTask_DemoMoveToPatrolKey>(BT, TEXT("Task_MoveTo_Cover"), RF_Transactional);
	}
	{
		FBTCompositeChild& Ch = SeqCombat->Children.AddDefaulted_GetRef();
		Ch.ChildTask = NewObject<UBTTask_DemoShoot>(BT, TEXT("Task_Shoot"), RF_Transactional);
	}

	// --- 分支 1：有 LastKnownLocation -> 调查 ---
	UBTDecorator_DemoBBIsSet* DecLK = NewObject<UBTDecorator_DemoBBIsSet>(BT, TEXT("Dec_LastKnown_Set"), RF_Transactional);
	DecLK->InitializeForKey(ADemoAIController::KEY_LastKnownLocation);

	FBTCompositeChild& BranchInvestigate = Root->Children.AddDefaulted_GetRef();
	BranchInvestigate.Decorators.Add(DecLK);
	BranchInvestigate.ChildTask = NewObject<UBTTask_DemoInvestigate>(BT, TEXT("Task_Investigate"), RF_Transactional);

	// --- 分支 2：巡逻 -> MoveTo PatrolLocation ---
	UBTComposite_Sequence* SeqPatrol = NewObject<UBTComposite_Sequence>(BT, TEXT("Seq_Patrol"), RF_Transactional);
	SeqPatrol->NodeName = TEXT("Patrol");

	FBTCompositeChild& BranchPatrol = Root->Children.AddDefaulted_GetRef();
	BranchPatrol.ChildComposite = SeqPatrol;

	{
		FBTCompositeChild& Ch = SeqPatrol->Children.AddDefaulted_GetRef();
		Ch.ChildTask = NewObject<UBTTask_DemoPatrol>(BT, TEXT("Task_Patrol"), RF_Transactional);
	}
	{
		FBTCompositeChild& Ch = SeqPatrol->Children.AddDefaulted_GetRef();
		Ch.ChildTask = NewObject<UBTTask_DemoMoveToPatrolKey>(BT, TEXT("Task_MoveTo_Patrol"), RF_Transactional);
	}

	BT->MarkPackageDirty();

	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(BT->GetOutermost());

	const bool bOk = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);
	UE_LOG(LogDemoClient, Log, TEXT("Demo.RebuildBTAssets: 行为树已写入 %s （保存 %s）"), BTPath, bOk ? TEXT("成功") : TEXT("失败，请查看日志"));
}

static FAutoConsoleCommand GDemoRebuildBTAssetsCmd(
	TEXT("Demo.RebuildBTAssets"),
	TEXT("重建 /Game/FirstPerson/AI/BT_DemoAI：战斗(找掩体/移动/射击) -> 调查 -> 巡逻。需已存在 BB_DemoAI 与 BT_DemoAI 资产。"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DemoRebuildBTAssets));

#endif // WITH_EDITOR
