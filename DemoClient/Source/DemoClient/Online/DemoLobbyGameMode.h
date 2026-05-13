#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DemoLobbyGameMode.generated.h"

/**
 * 大厅 GameMode（服务器权威）：登录、仓库数据、匹配后 ServerTravel 进入唯一对战地图。
 */
UCLASS()
class DEMOCLIENT_API ADemoLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADemoLobbyGameMode();

	/** 服务器：将各玩家仓库写入 GameInstance 并切图到对战关卡 */
	void PerformServerTravelToMatch();

protected:
	/** 对战地图（无扩展名软路径） */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Match")
	FString MatchMapPath = TEXT("/Game/UtopianCity/Maps/UtopianCityDemoMap");

	/** 对战 GameMode（蓝图） */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Match")
	FString MatchGameModePath = TEXT("/Game/FirstPerson/Blueprints/BP_DemoGameMode.BP_DemoGameMode_C");
};
