#include "Online/DemoLobbyGameMode.h"
#include "Online/DemoLobbyPlayerController.h"
#include "Online/DemoLobbyHUD.h"
#include "Online/DemoOnlineGameInstance.h"
#include "Online/DemoLobbyPlayerState.h"
#include "DemoClient.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"

ADemoLobbyGameMode::ADemoLobbyGameMode()
{
	GameStateClass = AGameStateBase::StaticClass();
	PlayerStateClass = ADemoLobbyPlayerState::StaticClass();
	PlayerControllerClass = ADemoLobbyPlayerController::StaticClass();
	HUDClass = ADemoLobbyHUD::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	bUseSeamlessTravel = false;
}

void ADemoLobbyGameMode::PerformServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(World->GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogDemoClient, Error, TEXT("PerformServerTravelToMatch: DemoOnlineGameInstance missing."));
		return;
	}

	GI->ServerStashSnapshotByPlayerId.Reset();

	if (AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ADemoLobbyPlayerState* LPS = Cast<ADemoLobbyPlayerState>(PS))
			{
				GI->ServerStashSnapshotByPlayerId.Add(LPS->GetPlayerId(), LPS->GetStashItems());
			}
		}
	}

	FString Options = FString::Printf(TEXT("?game=%s"), *MatchGameModePath);
	if (World->GetNetMode() == NM_ListenServer)
	{
		Options += TEXT("&listen");
	}

	const FString URL = MatchMapPath + Options;
	UE_LOG(LogDemoClient, Log, TEXT("Lobby ServerTravel -> %s"), *URL);
	World->ServerTravel(URL);
}
