#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DemoLobbyRootWidget.generated.h"

class UButton;
class UEditableTextBox;
class UPanelWidget;
class UTextBlock;
class UWidgetSwitcher;

/**
 * 大厅根界面 C++ 基类：登录/注册区 + 大厅 Tab（仓库、交易行占位）+ 开始匹配。
 * 在 UMG 中创建 WBP 并设父类为本类；控件名与 BindWidgetOptional 一致即可自动绑定。
 */
UCLASS(Blueprintable, meta = (DisplayName = "Demo Lobby Root Widget"))
class DEMOCLIENT_API UDemoLobbyRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 登录成功后显示大厅区（由 PlayerController Client RPC 或本地检测调用） */
	UFUNCTION(BlueprintCallable, Category = "Lobby|UI")
	void ShowLobbyPanel();

	UFUNCTION(BlueprintCallable, Category = "Lobby|UI")
	void SetFeedbackMessage(const FString& Message);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnLoginClicked();

	UFUNCTION()
	void OnRegisterClicked();

	UFUNCTION()
	void OnStartMatchClicked();

	UFUNCTION()
	void OnStashTabClicked();

	UFUNCTION()
	void OnTradeTabClicked();

	UFUNCTION()
	void OnAddStashDemoClicked();

	void SetAuthError(const FString& Message);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UEditableTextBox* EditableDisplayName = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UEditableTextBox* EditablePassword = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UEditableTextBox* EditablePasswordConfirm = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UTextBlock* TextAuthError = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UTextBlock* TextServerStatus = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UButton* ButtonLogin = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UButton* ButtonRegister = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UPanelWidget* PanelLogin = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UPanelWidget* PanelLobby = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UButton* ButtonStartMatch = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UButton* ButtonTabStash = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UButton* ButtonTabTrade = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UButton* ButtonAddStashDemo = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|UI")
	UWidgetSwitcher* SwitcherLobbySection = nullptr;
};
