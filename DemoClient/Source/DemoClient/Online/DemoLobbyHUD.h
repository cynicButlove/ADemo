#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DemoLobbyHUD.generated.h"

/** 大厅占位 HUD（后续可换 UMG）；当前仅 Canvas 文字说明与调试信息 */
UCLASS()
class DEMOCLIENT_API ADemoLobbyHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
