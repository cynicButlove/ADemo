#include "Online/DemoLobbyHUD.h"
#include "Online/DemoLobbyPlayerState.h"
#include "Online/DemoLobbyPlayerController.h"
#include "Engine/Engine.h"

void ADemoLobbyHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!GEngine)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();
	const float X = 32.f;
	float Y = 32.f;
	const float Line = 20.f;
	const FLinearColor Title(1.f, 0.85f, 0.2f);
	const FLinearColor Body(0.92f, 0.92f, 0.92f);

	auto LineText = [&](const FString& S, const FLinearColor& C)
	{
		DrawText(S, C, X, Y, Font, 1.f, false);
		Y += Line;
	};

	LineText(TEXT("Demo 大厅 — UMG：WBP_LobbyRoot（登录/仓库/交易/匹配）"), Title);
	LineText(TEXT("快捷键仍可用：M 开始匹配，K 加仓库测试物"), Body);
	LineText(TEXT("命令行 DisplayName= 为默认昵称；登录以界面输入为准"), Body);

	if (ADemoLobbyPlayerController* PC = Cast<ADemoLobbyPlayerController>(GetOwningPlayerController()))
	{
		if (ADemoLobbyPlayerState* PS = PC->GetPlayerState<ADemoLobbyPlayerState>())
		{
			LineText(
				FString::Printf(TEXT("玩家名: %s | 已登录: %s | 仓库槽位数: %d"),
					*PS->GetPlayerName(),
					PS->HasCompletedLogin() ? TEXT("是") : TEXT("否"),
					PS->GetStashItems().Num()),
				Body);
		}
	}
}
