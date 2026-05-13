#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/DemoTypes.h"
#include "DemoOnlineGameInstance.generated.h"

/**
 * 全局 GameInstance：登录用显示名、匹配前服务器侧仓库快照（按 PlayerId）。
 */
UCLASS(Config=Game)
class DEMOCLIENT_API UDemoOnlineGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	void SetPendingDisplayName(const FString& InName);
	FString GetPendingDisplayName() const { return PendingDisplayName; }
	bool ShouldUseDedicatedServer() const;
	bool ShouldUseUEDedicatedServerDirectConnect() const;
	const FString& GetDedicatedServerHost() const { return DedicatedServerHost; }
	int32 GetDedicatedServerPort() const { return DedicatedServerPort; }
	const FString& GetUEDedicatedServerHost() const { return UEDedicatedServerHost; }
	int32 GetUEDedicatedServerPort() const { return UEDedicatedServerPort; }

	/** 仅服务器：大厅开局前写入，进入对战图后由 DemoGameMode 消费并移除。不可 UPROPERTY：TMap 的值不能是 TArray（反射不支持嵌套容器）。 */
	TMap<int32, TArray<FDemoInventorySlot>> ServerStashSnapshotByPlayerId;

private:
	UPROPERTY()
	FString PendingDisplayName = TEXT("Player");

	UPROPERTY(Config, EditDefaultsOnly, Category = "DedicatedServer")
	bool bUseDedicatedDemoServer = true;

	UPROPERTY(Config, EditDefaultsOnly, Category = "DedicatedServer")
	FString DedicatedServerHost = TEXT("127.0.0.1");

	UPROPERTY(Config, EditDefaultsOnly, Category = "DedicatedServer")
	int32 DedicatedServerPort = 7777;

	UPROPERTY(Config, EditDefaultsOnly, Category = "DedicatedServer")
	bool bUseUEDedicatedServerDirectConnect = false;

	UPROPERTY(Config, EditDefaultsOnly, Category = "DedicatedServer")
	FString UEDedicatedServerHost = TEXT("127.0.0.1");

	UPROPERTY(Config, EditDefaultsOnly, Category = "DedicatedServer")
	int32 UEDedicatedServerPort = 7777;
};
