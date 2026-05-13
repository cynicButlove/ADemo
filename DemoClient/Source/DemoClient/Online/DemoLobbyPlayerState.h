#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/DemoTypes.h"
#include "DemoLobbyPlayerState.generated.h"

/** 大厅内玩家状态：显示名、登录标记、仓库（服务器权威，仅所有者复制） */
UCLASS()
class DEMOCLIENT_API ADemoLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ADemoLobbyPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool HasCompletedLogin() const { return bLoginCompleted; }

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	const TArray<FDemoInventorySlot>& GetStashItems() const { return StashItems; }

	/** 仅服务器：由大厅 PlayerController / GameMode 调用 */
	void AuthFinalizeLogin(const FString& InDisplayName);

	/** 仅服务器：向仓库添加（占位逻辑，后续接持久化） */
	void AuthAddStashItem(FName ItemID, int32 Quantity);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Login, BlueprintReadOnly, Category = "Lobby")
	bool bLoginCompleted = false;

	UPROPERTY(ReplicatedUsing = OnRep_Stash, BlueprintReadOnly, Category = "Lobby")
	TArray<FDemoInventorySlot> StashItems;

	UFUNCTION()
	void OnRep_Login();

	UFUNCTION()
	void OnRep_Stash();

private:
	static constexpr int32 MaxStashSlots = 48;
};
