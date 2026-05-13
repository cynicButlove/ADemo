#include "Online/DemoLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

ADemoLobbyPlayerState::ADemoLobbyPlayerState()
{
	SetNetUpdateFrequency(10.f);
}

void ADemoLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADemoLobbyPlayerState, bLoginCompleted);
	DOREPLIFETIME_CONDITION(ADemoLobbyPlayerState, StashItems, COND_OwnerOnly);
}

void ADemoLobbyPlayerState::OnRep_Login() {}

void ADemoLobbyPlayerState::OnRep_Stash() {}

void ADemoLobbyPlayerState::AuthFinalizeLogin(const FString& InDisplayName)
{
	FString UseName = InDisplayName.TrimStartAndEnd();
	if (UseName.IsEmpty())
	{
		UseName = TEXT("Player");
	}
	SetPlayerName(UseName);
	bLoginCompleted = true;
}

void ADemoLobbyPlayerState::AuthAddStashItem(FName ItemID, int32 Quantity)
{
	if (ItemID.IsNone() || Quantity <= 0 || StashItems.Num() >= MaxStashSlots)
	{
		return;
	}
	FDemoInventorySlot Slot;
	Slot.ItemID = ItemID;
	Slot.Quantity = Quantity;
	StashItems.Add(Slot);
}
