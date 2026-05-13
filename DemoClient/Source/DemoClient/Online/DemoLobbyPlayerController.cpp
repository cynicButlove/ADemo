#include "Online/DemoLobbyPlayerController.h"
#include "DemoClient.h"
#include "Online/DemoLobbyGameMode.h"
#include "Online/DemoLobbyPlayerState.h"
#include "Online/DemoDedicatedServerSubsystem.h"
#include "Online/DemoOnlineGameInstance.h"
#include "UI/DemoLobbyRootWidget.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName DemoStashItemId(TEXT("Item_DemoValuable"));

	FName ResolveDedicatedServerLevelName(const FString& RawMapName)
	{
		const FString UtopianCityMapPath(TEXT("/Game/UtopianCity/Maps/UtopianCityDemoMap"));

		if (RawMapName.IsEmpty())
		{
			return FName(*UtopianCityMapPath);
		}

		if (RawMapName.Equals(TEXT("L_DemoExtractionGraybox")) ||
			RawMapName.Equals(TEXT("/Game/Demo/Maps/L_DemoExtractionGraybox")) ||
			RawMapName.Contains(TEXT("UtopianCityDemoMap")))
		{
			return FName(*UtopianCityMapPath);
		}

		if (RawMapName.StartsWith(TEXT("/Game/")))
		{
			return FName(*RawMapName);
		}

		return FName(*FString::Printf(TEXT("/Game/Demo/Maps/%s"), *RawMapName));
	}
}

ADemoLobbyPlayerController::ADemoLobbyPlayerController()
{
	static ConstructorHelpers::FClassFinder<UDemoLobbyRootWidget> LobbyRootFinder(
		TEXT("/Game/UI/Lobby/WBP_LobbyRoot.WBP_LobbyRoot_C"));
	if (LobbyRootFinder.Succeeded())
	{
		LobbyRootWidgetClass = LobbyRootFinder.Class;
	}
}

void ADemoLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		if (LobbyRootWidgetClass)
		{
			LobbyRootWidgetInstance = CreateWidget<UDemoLobbyRootWidget>(this, LobbyRootWidgetClass);
			if (LobbyRootWidgetInstance)
			{
				LobbyRootWidgetInstance->AddToViewport(100);
				Mode.SetWidgetToFocus(LobbyRootWidgetInstance->TakeWidget());
			}
		}
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DeferredLobbySyncTimer,
				this,
				&ADemoLobbyPlayerController::TrySyncLobbyPanelFromPlayerState,
				0.35f,
				false);
		}

		BindDedicatedServerDelegates();

		if (ShouldUseUEDedicatedServerDirectConnectPath() && LobbyRootWidgetInstance)
		{
			LobbyRootWidgetInstance->ShowLobbyPanel();
			LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("UE Dedicated Server直连已启用，点击 Start Match 进入对局"));
		}
	}
	else
	{
		SetShowMouseCursor(false);
		FInputModeGameOnly Go;
		SetInputMode(Go);
	}
}

void ADemoLobbyPlayerController::TrySyncLobbyPanelFromPlayerState()
{
	if (!LobbyRootWidgetInstance)
	{
		return;
	}

	if (ShouldUseDedicatedServerPath())
	{
		if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
		{
			if (DedicatedServer->IsAuthenticated())
			{
				LobbyRootWidgetInstance->ShowLobbyPanel();
			}
		}
	}

	if (ShouldUseUEDedicatedServerDirectConnectPath())
	{
		if (const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance()))
		{
			if (!GI->GetPendingDisplayName().IsEmpty())
			{
				LobbyRootWidgetInstance->ShowLobbyPanel();
				LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("UE Dedicated Server直连已启用，点击 Start Match 进入对局"));
			}
		}
	}

	if (ADemoLobbyPlayerState* PS = GetPlayerState<ADemoLobbyPlayerState>())
	{
		if (PS->HasCompletedLogin())
		{
			LobbyRootWidgetInstance->ShowLobbyPanel();
		}
	}
}

void ADemoLobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ADemoLobbyPlayerController::OnRequestStartMatch);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ADemoLobbyPlayerController::OnAddDemoStashItem);
}

void ADemoLobbyPlayerController::UI_RequestSubmitLogin(const FString& DisplayName)
{
	if (!IsLocalController())
	{
		return;
	}

	if (ShouldUseUEDedicatedServerDirectConnectPath())
	{
		if (LobbyRootWidgetInstance)
		{
			LobbyRootWidgetInstance->ShowLobbyPanel();
			LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("宸插噯澶囩洿杩?UE Dedicated Server锛岀偣鍑?Start Match 杩涘叆瀵规垬"));
		}
		return;
	}

	if (ShouldUseDedicatedServerPath())
	{
		BindDedicatedServerDelegates();

		UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance());
		UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
		if (!GI || !DedicatedServer)
		{
			if (LobbyRootWidgetInstance)
			{
				LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("专用服务器子系统初始化失败"));
			}
			return;
		}

		FString ErrorMessage;
		if (!DedicatedServer->Connect(GI->GetDedicatedServerHost(), GI->GetDedicatedServerPort(), &ErrorMessage))
		{
			if (LobbyRootWidgetInstance)
			{
				LobbyRootWidgetInstance->SetFeedbackMessage(ErrorMessage);
			}
			return;
		}

		if (!DedicatedServer->Login(DisplayName, &ErrorMessage))
		{
			if (LobbyRootWidgetInstance)
			{
				LobbyRootWidgetInstance->SetFeedbackMessage(ErrorMessage);
			}
			return;
		}

		if (LobbyRootWidgetInstance)
		{
			LobbyRootWidgetInstance->SetFeedbackMessage(FString::Printf(
				TEXT("已连接专用服务器 %s:%d，正在登录..."),
				*GI->GetDedicatedServerHost(),
				GI->GetDedicatedServerPort()));
		}
		return;
	}

	ServerSubmitLogin(DisplayName);
}

void ADemoLobbyPlayerController::UI_RequestStartMatch()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ShouldUseUEDedicatedServerDirectConnectPath())
	{
		if (LobbyRootWidgetInstance)
		{
			LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("姝ｅ湪鐩磋繛 UE Dedicated Server..."));
		}

		ClientTravel(BuildUEDedicatedServerTravelURL(), TRAVEL_Absolute);
		return;
	}

	if (ShouldUseDedicatedServerPath())
	{
		UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>();
		if (!DedicatedServer)
		{
			return;
		}

		FString ErrorMessage;
		if (!DedicatedServer->JoinMatch(TEXT(""), &ErrorMessage))
		{
			if (LobbyRootWidgetInstance)
			{
				LobbyRootWidgetInstance->SetFeedbackMessage(ErrorMessage);
			}
			return;
		}

		if (LobbyRootWidgetInstance)
		{
			LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("正在加入专用服务器对局..."));
		}
		return;
	}

	ServerRequestStartMatch();
}

void ADemoLobbyPlayerController::UI_RequestStashDemoItem()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ShouldUseDedicatedServerPath())
	{
		if (LobbyRootWidgetInstance)
		{
			LobbyRootWidgetInstance->SetFeedbackMessage(TEXT("专用服务器路径暂未接入仓库/补给接口"));
		}
		return;
	}

	ServerAddDemoStashItem(DemoStashItemId, 1);
}

void ADemoLobbyPlayerController::OnRequestStartMatch()
{
	UI_RequestStartMatch();
}

void ADemoLobbyPlayerController::OnAddDemoStashItem()
{
	UI_RequestStashDemoItem();
}

void ADemoLobbyPlayerController::BindDedicatedServerDelegates()
{
	if (bDedicatedServerDelegatesBound || !ShouldUseDedicatedServerPath())
	{
		return;
	}

	if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
	{
		DedicatedServer->OnStatusMessage.AddUObject(this, &ADemoLobbyPlayerController::HandleDedicatedServerStatusMessage);
		DedicatedServer->OnAuthSucceeded.AddUObject(this, &ADemoLobbyPlayerController::HandleDedicatedServerAuthSucceeded);
		DedicatedServer->OnMatchJoined.AddUObject(this, &ADemoLobbyPlayerController::HandleDedicatedServerMatchJoined);
		DedicatedServer->OnServerError.AddUObject(this, &ADemoLobbyPlayerController::HandleDedicatedServerError);
		bDedicatedServerDelegatesBound = true;
	}
}

bool ADemoLobbyPlayerController::ShouldUseDedicatedServerPath() const
{
	const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance());
	return GI && GI->ShouldUseDedicatedServer();
}

bool ADemoLobbyPlayerController::ShouldUseUEDedicatedServerDirectConnectPath() const
{
	const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance());
	return GI && GI->ShouldUseUEDedicatedServerDirectConnect();
}

FString ADemoLobbyPlayerController::BuildUEDedicatedServerTravelURL() const
{
	const UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(GetGameInstance());
	if (!GI)
	{
		return FString(TEXT("127.0.0.1:7777"));
	}

	const FString DisplayName = GI->GetPendingDisplayName().IsEmpty() ? FString(TEXT("Player")) : GI->GetPendingDisplayName();
	return FString::Printf(
		TEXT("%s:%d?DisplayName=%s"),
		*GI->GetUEDedicatedServerHost(),
		GI->GetUEDedicatedServerPort(),
		*DisplayName);
}

void ADemoLobbyPlayerController::HandleDedicatedServerStatusMessage(const FString& Message)
{
	if (LobbyRootWidgetInstance)
	{
		LobbyRootWidgetInstance->SetFeedbackMessage(Message);
	}
}

void ADemoLobbyPlayerController::HandleDedicatedServerAuthSucceeded(const FString& PlayerId, const FString& DisplayName)
{
	if (LobbyRootWidgetInstance)
	{
		LobbyRootWidgetInstance->ShowLobbyPanel();
		LobbyRootWidgetInstance->SetFeedbackMessage(FString::Printf(
			TEXT("专用服务器登录成功：%s (%s)"),
			*DisplayName,
			*PlayerId));
	}
}

void ADemoLobbyPlayerController::HandleDedicatedServerMatchJoined(const FString& MatchId)
{
	if (LobbyRootWidgetInstance)
	{
		LobbyRootWidgetInstance->SetFeedbackMessage(FString::Printf(
			TEXT("已加入专用服务器对局 %s。下一步将把玩法地图与快照驱动接上。"),
			*MatchId));
	}

	if (GetGameInstance())
	{
		if (UDemoDedicatedServerSubsystem* DedicatedServer = GetGameInstance()->GetSubsystem<UDemoDedicatedServerSubsystem>())
		{
			UGameplayStatics::OpenLevel(this, ResolveDedicatedServerLevelName(DedicatedServer->GetCurrentMapName()));
		}
	}
}

void ADemoLobbyPlayerController::HandleDedicatedServerError(const FString& Code, const FString& Message)
{
	UE_LOG(LogDemoClient, Warning, TEXT("[DedicatedServer][%s] %s"), *Code, *Message);
	if (LobbyRootWidgetInstance)
	{
		LobbyRootWidgetInstance->SetFeedbackMessage(Message);
	}
}

bool ADemoLobbyPlayerController::ServerSubmitLogin_Validate(const FString& DisplayName)
{
	return DisplayName.Len() > 0 && DisplayName.Len() <= 48;
}

void ADemoLobbyPlayerController::ServerSubmitLogin_Implementation(const FString& DisplayName)
{
	if (ADemoLobbyPlayerState* PS = GetPlayerState<ADemoLobbyPlayerState>())
	{
		PS->AuthFinalizeLogin(DisplayName);
		ClientNotifyLoginOk();
	}
}

void ADemoLobbyPlayerController::ClientNotifyLoginOk_Implementation()
{
	if (LobbyRootWidgetInstance)
	{
		LobbyRootWidgetInstance->ShowLobbyPanel();
	}
}

bool ADemoLobbyPlayerController::ServerRequestStartMatch_Validate()
{
	return true;
}

void ADemoLobbyPlayerController::ServerRequestStartMatch_Implementation()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (ADemoLobbyGameMode* GM = Cast<ADemoLobbyGameMode>(World->GetAuthGameMode()))
	{
		GM->PerformServerTravelToMatch();
	}
}

bool ADemoLobbyPlayerController::ServerAddDemoStashItem_Validate(FName ItemID, int32 Quantity)
{
	return !ItemID.IsNone() && Quantity > 0 && Quantity <= 99;
}

void ADemoLobbyPlayerController::ServerAddDemoStashItem_Implementation(FName ItemID, int32 Quantity)
{
	if (ADemoLobbyPlayerState* PS = GetPlayerState<ADemoLobbyPlayerState>())
	{
		PS->AuthAddStashItem(ItemID, Quantity);
	}
}
