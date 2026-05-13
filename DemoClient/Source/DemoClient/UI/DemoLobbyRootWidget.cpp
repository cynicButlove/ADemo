#include "UI/DemoLobbyRootWidget.h"
#include "Online/DemoLobbyPlayerController.h"
#include "Online/DemoOnlineGameInstance.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

void UDemoLobbyRootWidget::SetAuthError(const FString& Message)
{
	if (!TextAuthError)
	{
		return;
	}
	TextAuthError->SetText(FText::FromString(Message));
	TextAuthError->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UDemoLobbyRootWidget::SetFeedbackMessage(const FString& Message)
{
	if (TextServerStatus)
	{
		TextServerStatus->SetText(FText::FromString(Message));
		TextServerStatus->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		return;
	}

	SetAuthError(Message);
}

void UDemoLobbyRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetAuthError(TEXT(""));
	SetFeedbackMessage(TEXT(""));

	if (ButtonLogin)
	{
		ButtonLogin->OnClicked.AddDynamic(this, &UDemoLobbyRootWidget::OnLoginClicked);
	}
	if (ButtonRegister)
	{
		ButtonRegister->OnClicked.AddDynamic(this, &UDemoLobbyRootWidget::OnRegisterClicked);
	}
	if (ButtonStartMatch)
	{
		ButtonStartMatch->OnClicked.AddDynamic(this, &UDemoLobbyRootWidget::OnStartMatchClicked);
	}
	if (ButtonTabStash)
	{
		ButtonTabStash->OnClicked.AddDynamic(this, &UDemoLobbyRootWidget::OnStashTabClicked);
	}
	if (ButtonTabTrade)
	{
		ButtonTabTrade->OnClicked.AddDynamic(this, &UDemoLobbyRootWidget::OnTradeTabClicked);
	}
	if (ButtonAddStashDemo)
	{
		ButtonAddStashDemo->OnClicked.AddDynamic(this, &UDemoLobbyRootWidget::OnAddStashDemoClicked);
	}
}

void UDemoLobbyRootWidget::ShowLobbyPanel()
{
	if (PanelLogin)
	{
		PanelLogin->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PanelLobby)
	{
		PanelLobby->SetVisibility(ESlateVisibility::Visible);
	}
}

void UDemoLobbyRootWidget::OnLoginClicked()
{
	SetAuthError(TEXT(""));
	SetFeedbackMessage(TEXT(""));

	ADemoLobbyPlayerController* PC = GetOwningPlayer<ADemoLobbyPlayerController>();
	if (!PC)
	{
		return;
	}

	FString Name = EditableDisplayName ? EditableDisplayName->GetText().ToString().TrimStartAndEnd() : FString();
	if (Name.IsEmpty())
	{
		SetAuthError(TEXT("请输入显示名称"));
		return;
	}

	if (EditablePassword)
	{
		const FString Pwd = EditablePassword->GetText().ToString().TrimStartAndEnd();
		if (Pwd.IsEmpty())
		{
			SetAuthError(TEXT("请输入密码（当前为本地校验，不会上传服务器）"));
			return;
		}
		if (Pwd.Len() < 4)
		{
			SetAuthError(TEXT("密码至少 4 位"));
			return;
		}
	}

	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		Name = FString::Printf(TEXT("%s_%i"), *Name, LP->GetControllerId());
	}

	if (UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(PC->GetGameInstance()))
	{
		GI->SetPendingDisplayName(Name);
	}

	PC->UI_RequestSubmitLogin(Name);
}

void UDemoLobbyRootWidget::OnRegisterClicked()
{
	SetAuthError(TEXT(""));

	ADemoLobbyPlayerController* PC = GetOwningPlayer<ADemoLobbyPlayerController>();
	if (!PC)
	{
		return;
	}

	if (!EditablePassword && !EditablePasswordConfirm)
	{
		OnLoginClicked();
		return;
	}

	FString Name = EditableDisplayName ? EditableDisplayName->GetText().ToString().TrimStartAndEnd() : FString();
	if (Name.IsEmpty())
	{
		SetAuthError(TEXT("请设置显示名称"));
		return;
	}

	const FString Pwd = EditablePassword ? EditablePassword->GetText().ToString().TrimStartAndEnd() : FString();
	const FString Pwd2 = EditablePasswordConfirm ? EditablePasswordConfirm->GetText().ToString().TrimStartAndEnd() : FString();

	if (Pwd.Len() < 4)
	{
		SetAuthError(TEXT("密码至少 4 位"));
		return;
	}
	if (Pwd != Pwd2)
	{
		SetAuthError(TEXT("两次输入的密码不一致"));
		return;
	}

	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		Name = FString::Printf(TEXT("%s_%i"), *Name, LP->GetControllerId());
	}

	if (UDemoOnlineGameInstance* GI = Cast<UDemoOnlineGameInstance>(PC->GetGameInstance()))
	{
		GI->SetPendingDisplayName(Name);
	}

	PC->UI_RequestSubmitLogin(Name);
}

void UDemoLobbyRootWidget::OnStartMatchClicked()
{
	if (ADemoLobbyPlayerController* PC = GetOwningPlayer<ADemoLobbyPlayerController>())
	{
		PC->UI_RequestStartMatch();
	}
}

void UDemoLobbyRootWidget::OnStashTabClicked()
{
	if (SwitcherLobbySection)
	{
		SwitcherLobbySection->SetActiveWidgetIndex(0);
	}
}

void UDemoLobbyRootWidget::OnTradeTabClicked()
{
	if (SwitcherLobbySection)
	{
		SwitcherLobbySection->SetActiveWidgetIndex(1);
	}
}

void UDemoLobbyRootWidget::OnAddStashDemoClicked()
{
	if (ADemoLobbyPlayerController* PC = GetOwningPlayer<ADemoLobbyPlayerController>())
	{
		PC->UI_RequestStashDemoItem();
	}
}
