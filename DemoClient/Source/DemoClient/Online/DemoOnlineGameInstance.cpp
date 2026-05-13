#include "Online/DemoOnlineGameInstance.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"

void UDemoOnlineGameInstance::Init()
{
	Super::Init();

	FString CmdName;
	if (FParse::Value(FCommandLine::Get(), TEXT("DisplayName="), CmdName) && !CmdName.IsEmpty())
	{
		PendingDisplayName = CmdName;
	}

	FString CmdDedicatedFlag;
	if (FParse::Value(FCommandLine::Get(), TEXT("UseDedicatedDemoServer="), CmdDedicatedFlag))
	{
		bUseDedicatedDemoServer = CmdDedicatedFlag.ToBool();
	}

	FString CmdHost;
	if (FParse::Value(FCommandLine::Get(), TEXT("DedicatedServerHost="), CmdHost) && !CmdHost.IsEmpty())
	{
		DedicatedServerHost = CmdHost;
	}

	int32 CmdPort = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("DedicatedServerPort="), CmdPort) && CmdPort > 0)
	{
		DedicatedServerPort = CmdPort;
	}

	FString CmdUEDedicatedFlag;
	if (FParse::Value(FCommandLine::Get(), TEXT("UseUEDedicatedServerDirectConnect="), CmdUEDedicatedFlag))
	{
		bUseUEDedicatedServerDirectConnect = CmdUEDedicatedFlag.ToBool();
	}

	FString CmdUEHost;
	if (FParse::Value(FCommandLine::Get(), TEXT("UEDedicatedServerHost="), CmdUEHost) && !CmdUEHost.IsEmpty())
	{
		UEDedicatedServerHost = CmdUEHost;
	}

	int32 CmdUEPort = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("UEDedicatedServerPort="), CmdUEPort) && CmdUEPort > 0)
	{
		UEDedicatedServerPort = CmdUEPort;
	}
}

void UDemoOnlineGameInstance::SetPendingDisplayName(const FString& InName)
{
	PendingDisplayName = InName.IsEmpty() ? FString(TEXT("Player")) : InName;
}

bool UDemoOnlineGameInstance::ShouldUseDedicatedServer() const
{
	return bUseDedicatedDemoServer && !IsRunningDedicatedServer();
}

bool UDemoOnlineGameInstance::ShouldUseUEDedicatedServerDirectConnect() const
{
	return bUseUEDedicatedServerDirectConnect && !IsRunningDedicatedServer();
}
