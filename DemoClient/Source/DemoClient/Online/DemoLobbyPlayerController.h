#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/DemoLobbyRootWidget.h"
#include "DemoLobbyPlayerController.generated.h"

UCLASS()
class DEMOCLIENT_API ADemoLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADemoLobbyPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** 供 UMG（C++ 基类）调用；内部走本地 Owned 的 Server RPC */
	UFUNCTION(BlueprintCallable, Category = "Lobby|UI")
	void UI_RequestSubmitLogin(const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Lobby|UI")
	void UI_RequestStartMatch();

	UFUNCTION(BlueprintCallable, Category = "Lobby|UI")
	void UI_RequestStashDemoItem();

protected:
	void OnRequestStartMatch();
	void OnAddDemoStashItem();
	void BindDedicatedServerDelegates();
	bool ShouldUseDedicatedServerPath() const;
	bool ShouldUseUEDedicatedServerDirectConnectPath() const;
	FString BuildUEDedicatedServerTravelURL() const;
	void HandleDedicatedServerStatusMessage(const FString& Message);
	void HandleDedicatedServerAuthSucceeded(const FString& PlayerId, const FString& DisplayName);
	void HandleDedicatedServerMatchJoined(const FString& MatchId);
	void HandleDedicatedServerError(const FString& Code, const FString& Message);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSubmitLogin(const FString& DisplayName);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestStartMatch();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddDemoStashItem(FName ItemID, int32 Quantity);

	UFUNCTION(Client, Reliable)
	void ClientNotifyLoginOk();

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	TSubclassOf<UDemoLobbyRootWidget> LobbyRootWidgetClass;

private:
	TObjectPtr<UDemoLobbyRootWidget> LobbyRootWidgetInstance;
	bool bDedicatedServerDelegatesBound = false;

	void TrySyncLobbyPanelFromPlayerState();

	FTimerHandle DeferredLobbySyncTimer;
};
