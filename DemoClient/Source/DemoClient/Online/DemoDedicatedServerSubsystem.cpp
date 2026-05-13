#include "Online/DemoDedicatedServerSubsystem.h"

#include "DemoClient.h"
#include "Containers/Ticker.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
	const FString WelcomePrefix(TEXT("WELCOME "));
	const FString AuthOkPrefix(TEXT("AUTH_OK "));
	const FString MatchJoinedPrefix(TEXT("MATCH_JOINED "));
	const FString MatchLeftPrefix(TEXT("MATCH_LEFT "));
	const FString SnapshotPrefix(TEXT("SNAPSHOT "));
	const FString InventoryPrefix(TEXT("INVENTORY "));
	const FString ContainerStatePrefix(TEXT("CONTAINER_STATE "));
	const FString ErrorPrefix(TEXT("ERROR "));
	const FString StatusPrefix(TEXT("STATUS "));
	const FString PongPrefix(TEXT("PONG "));
}

void UDemoDedicatedServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UDemoDedicatedServerSubsystem::Tick));
}

void UDemoDedicatedServerSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	Disconnect(TEXT("SubsystemShutdown"));
	Super::Deinitialize();
}

bool UDemoDedicatedServerSubsystem::Connect(const FString& InHost, int32 InPort, FString* OutError)
{
	if (IsConnected() && Host == InHost && Port == InPort)
	{
		return true;
	}

	Disconnect(TEXT("Reconnect"));

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		if (OutError)
		{
			*OutError = TEXT("无法获取 Socket 子系统");
		}
		return false;
	}

	TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
	bool bIsValidAddress = false;
	Address->SetIp(*InHost, bIsValidAddress);
	Address->SetPort(InPort);
	if (!bIsValidAddress)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("无效的专用服务器地址：%s"), *InHost);
		}
		return false;
	}

	FSocket* NewSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("DemoDedicatedServerSocket"), false);
	if (!NewSocket)
	{
		if (OutError)
		{
			*OutError = TEXT("创建专用服务器 Socket 失败");
		}
		return false;
	}

	NewSocket->SetNoDelay(true);
	NewSocket->SetNonBlocking(false);

	if (!NewSocket->Connect(*Address))
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("连接专用服务器失败：%s:%d"), *InHost, InPort);
		}
		NewSocket->Close();
		SocketSubsystem->DestroySocket(NewSocket);
		return false;
	}

	NewSocket->SetNonBlocking(true);
	Socket = NewSocket;
	Host = InHost;
	Port = InPort;
	SetStatusMessage(FString::Printf(TEXT("已连接专用服务器 %s:%d"), *Host, Port));
	return true;
}

void UDemoDedicatedServerSubsystem::Disconnect(const FString& Reason)
{
	if (Socket)
	{
		if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			Socket->Close();
			SocketSubsystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}

	ResetSessionState();

	if (!Reason.IsEmpty())
	{
		SetStatusMessage(FString::Printf(TEXT("专用服务器连接已断开：%s"), *Reason));
	}
}

bool UDemoDedicatedServerSubsystem::Login(const FString& DisplayName, FString* OutError)
{
	const FString SanitizedName = DisplayName.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
	return SendCommand(FString::Printf(TEXT("AUTH %s"), *SanitizedName), OutError);
}

bool UDemoDedicatedServerSubsystem::JoinMatch(const FString& PreferredMatchId, FString* OutError)
{
	if (!PreferredMatchId.IsEmpty())
	{
		return SendCommand(FString::Printf(TEXT("JOIN %s"), *PreferredMatchId), OutError);
	}
	return SendCommand(TEXT("JOIN"), OutError);
}

bool UDemoDedicatedServerSubsystem::LeaveMatch(FString* OutError)
{
	if (!CurrentMatchId.IsEmpty())
	{
		return SendCommand(TEXT("LEAVE"), OutError);
	}
	return true;
}

bool UDemoDedicatedServerSubsystem::InteractContainer(const FString& ContainerId, FString* OutError)
{
	return SendCommand(FString::Printf(TEXT("INTERACT_CONTAINER %s"), *ContainerId), OutError);
}

bool UDemoDedicatedServerSubsystem::TakeContainerItem(const FString& ContainerId, int32 SlotIndex, FString* OutError)
{
	return SendCommand(FString::Printf(TEXT("TAKE_CONTAINER_ITEM %s %d"), *ContainerId, SlotIndex), OutError);
}

bool UDemoDedicatedServerSubsystem::PickupLoot(const FString& LootId, FString* OutError)
{
	return SendCommand(FString::Printf(TEXT("PICKUP_LOOT %s"), *LootId), OutError);
}

bool UDemoDedicatedServerSubsystem::DropItem(int32 SlotIndex, FString* OutError)
{
	return SendCommand(FString::Printf(TEXT("DROP_ITEM %d"), SlotIndex), OutError);
}

bool UDemoDedicatedServerSubsystem::UseItem(int32 SlotIndex, FString* OutError)
{
	return SendCommand(FString::Printf(TEXT("USE_ITEM %d"), SlotIndex), OutError);
}

bool UDemoDedicatedServerSubsystem::SendInput(
	int32 Sequence,
	const FVector2D& MoveInput,
	const FRotator& ControlRotation,
	bool bSprinting,
	bool bCrouching,
	FString* OutError)
{
	return SendCommand(
		FString::Printf(
			TEXT("INPUT %d %.3f %.3f %.3f %.3f %d %d"),
			Sequence,
			MoveInput.X,
			MoveInput.Y,
			ControlRotation.Yaw,
			ControlRotation.Pitch,
			bSprinting ? 1 : 0,
			bCrouching ? 1 : 0),
		OutError);
}

bool UDemoDedicatedServerSubsystem::SendFire(FString* OutError)
{
	return SendCommand(TEXT("FIRE"), OutError);
}

bool UDemoDedicatedServerSubsystem::SendReload(FString* OutError)
{
	return SendCommand(TEXT("RELOAD"), OutError);
}

bool UDemoDedicatedServerSubsystem::IsConnected() const
{
	return Socket != nullptr && Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}

EDemoDedicatedServerConnectionState UDemoDedicatedServerSubsystem::GetConnectionState() const
{
	if (!IsConnected())
	{
		return EDemoDedicatedServerConnectionState::Disconnected;
	}
	if (!CurrentMatchId.IsEmpty())
	{
		return EDemoDedicatedServerConnectionState::InMatch;
	}
	if (bAuthenticated)
	{
		return EDemoDedicatedServerConnectionState::Authenticated;
	}
	return EDemoDedicatedServerConnectionState::Connected;
}

const FDemoDedicatedServerContainerSnapshot* UDemoDedicatedServerSubsystem::FindContainerSnapshot(
	const FString& ContainerId) const
{
	return ContainerSnapshots.Find(ContainerId);
}

const FDemoDedicatedServerPlayerSnapshot* UDemoDedicatedServerSubsystem::FindPlayerSnapshot(const FString& InPlayerId) const
{
	for (const FDemoDedicatedServerPlayerSnapshot& SnapshotPlayer : LastSnapshot.Players)
	{
		if (SnapshotPlayer.PlayerId == InPlayerId)
		{
			return &SnapshotPlayer;
		}
	}
	return nullptr;
}

const FDemoDedicatedServerPlayerSnapshot* UDemoDedicatedServerSubsystem::GetLocalPlayerSnapshot() const
{
	return FindPlayerSnapshot(PlayerId);
}

bool UDemoDedicatedServerSubsystem::Tick(float DeltaTime)
{
	(void)DeltaTime;
	PumpSocket();
	return true;
}

void UDemoDedicatedServerSubsystem::PumpSocket()
{
	if (!Socket)
	{
		return;
	}

	if (Socket->GetConnectionState() == ESocketConnectionState::SCS_ConnectionError)
	{
		BroadcastError(TEXT("connection_error"), TEXT("专用服务器连接发生错误"));
		Disconnect(TEXT("ConnectionError"));
		return;
	}

	uint32 PendingData = 0;
	while (Socket && Socket->HasPendingData(PendingData))
	{
		const int32 BytesToRead = FMath::Min<int32>(static_cast<int32>(PendingData), 65536);
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(BytesToRead + 1);

		int32 BytesRead = 0;
		if (!Socket->Recv(Buffer.GetData(), BytesToRead, BytesRead) || BytesRead <= 0)
		{
			BroadcastError(TEXT("recv_failed"), TEXT("读取专用服务器数据失败"));
			Disconnect(TEXT("RecvFailed"));
			return;
		}

		Buffer[BytesRead] = 0;
		ReceiveBuffer += UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData()));

		int32 NewlineIndex = INDEX_NONE;
		while (ReceiveBuffer.FindChar(TEXT('\n'), NewlineIndex))
		{
			const FString Line = ReceiveBuffer.Left(NewlineIndex).TrimStartAndEnd();
			ReceiveBuffer.RightChopInline(NewlineIndex + 1, EAllowShrinking::No);
			if (!Line.IsEmpty())
			{
				ProcessLine(Line);
			}
		}
	}
}

bool UDemoDedicatedServerSubsystem::SendCommand(const FString& Command, FString* OutError)
{
	if (!Socket)
	{
		if (OutError)
		{
			*OutError = TEXT("尚未连接专用服务器");
		}
		return false;
	}

	const FString Payload = Command + TEXT("\n");
	FTCHARToUTF8 Converter(*Payload);
	int32 TotalBytesSent = 0;

	while (TotalBytesSent < Converter.Length())
	{
		int32 BytesSent = 0;
		if (!Socket->Send(
				reinterpret_cast<const uint8*>(Converter.Get()) + TotalBytesSent,
				Converter.Length() - TotalBytesSent,
				BytesSent))
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("发送专用服务器命令失败：%s"), *Command);
			}
			return false;
		}

		TotalBytesSent += BytesSent;
	}

	return true;
}

void UDemoDedicatedServerSubsystem::ProcessLine(const FString& Line)
{
	UE_LOG(LogDemoClient, Log, TEXT("[DedicatedServer] %s"), *Line);

	if (Line.StartsWith(WelcomePrefix))
	{
		TArray<FString> Parts;
		Line.ParseIntoArrayWS(Parts);
		if (Parts.Num() >= 2)
		{
			ConnectionId = Parts[1];
			SetStatusMessage(FString::Printf(TEXT("专用服务器握手完成，连接 ID：%s"), *ConnectionId));
		}
		return;
	}

	if (Line.StartsWith(AuthOkPrefix))
	{
		const FString Payload = Line.RightChop(AuthOkPrefix.Len());
		FString ParsedPlayerId;
		FString DisplayName;
		if (Payload.Split(TEXT(" "), &ParsedPlayerId, &DisplayName))
		{
			PlayerId = ParsedPlayerId;
			bAuthenticated = true;
			SetStatusMessage(FString::Printf(TEXT("专用服务器登录成功：%s"), *DisplayName));
			OnAuthSucceeded.Broadcast(PlayerId, DisplayName);
		}
		return;
	}

	if (Line.StartsWith(MatchJoinedPrefix))
	{
		TArray<FString> Parts;
		Line.ParseIntoArrayWS(Parts);
		if (Parts.Num() >= 2)
		{
			CurrentMatchId = Parts[1];
			if (Parts.Num() >= 5)
			{
				CurrentMapName = Parts[4];
			}
			SetStatusMessage(FString::Printf(TEXT("已加入专用服务器对局：%s"), *CurrentMatchId));
			OnMatchJoined.Broadcast(CurrentMatchId);
		}
		return;
	}

	if (Line.StartsWith(MatchLeftPrefix))
	{
		CurrentMatchId.Reset();
		CurrentMapName.Reset();
		SetStatusMessage(TEXT("已离开专用服务器对局"));
		return;
	}

	if (Line.StartsWith(SnapshotPrefix))
	{
		FDemoDedicatedServerSnapshot ParsedSnapshot;
		if (ParseSnapshotJson(Line.RightChop(SnapshotPrefix.Len()), ParsedSnapshot))
		{
			TSet<FString> SnapshotContainerIds;
			for (FDemoDedicatedServerContainerSnapshot& ContainerSnapshot : ParsedSnapshot.Containers)
			{
				SnapshotContainerIds.Add(ContainerSnapshot.ContainerId);

				if (const FDemoDedicatedServerContainerSnapshot* Existing = ContainerSnapshots.Find(ContainerSnapshot.ContainerId))
				{
					if (ContainerSnapshot.Contents.Num() == 0 && Existing->Contents.Num() > 0)
					{
						ContainerSnapshot.Contents = Existing->Contents;
					}
				}

				ContainerSnapshots.Add(ContainerSnapshot.ContainerId, ContainerSnapshot);
			}

			TArray<FString> ContainersToRemove;
			for (const TPair<FString, FDemoDedicatedServerContainerSnapshot>& Pair : ContainerSnapshots)
			{
				if (!SnapshotContainerIds.Contains(Pair.Key))
				{
					ContainersToRemove.Add(Pair.Key);
				}
			}
			for (const FString& ContainerId : ContainersToRemove)
			{
				ContainerSnapshots.Remove(ContainerId);
			}

			LastSnapshot = MoveTemp(ParsedSnapshot);
			if (!LastSnapshot.MapName.IsEmpty())
			{
				CurrentMapName = LastSnapshot.MapName;
			}
			OnSnapshotReceived.Broadcast(LastSnapshot);
		}
		return;
	}

	if (Line.StartsWith(InventoryPrefix))
	{
		TArray<FDemoInventorySlot> ParsedItems;
		if (ParseInventoryJson(Line.RightChop(InventoryPrefix.Len()), ParsedItems))
		{
			InventoryItems = MoveTemp(ParsedItems);
			OnInventoryUpdated.Broadcast(InventoryItems);
		}
		return;
	}

	if (Line.StartsWith(ContainerStatePrefix))
	{
		FDemoDedicatedServerContainerSnapshot ParsedContainer;
		if (ParseContainerJson(Line.RightChop(ContainerStatePrefix.Len()), ParsedContainer))
		{
			ContainerSnapshots.Add(ParsedContainer.ContainerId, ParsedContainer);
			OnContainerUpdated.Broadcast(ParsedContainer);
		}
		return;
	}

	if (Line.StartsWith(ErrorPrefix))
	{
		const FString Payload = Line.RightChop(ErrorPrefix.Len());
		FString Code;
		FString Message;
		if (Payload.Split(TEXT(" "), &Code, &Message))
		{
			BroadcastError(Code, Message);
		}
		else
		{
			BroadcastError(TEXT("server_error"), Payload);
		}
		return;
	}

	if (Line.StartsWith(StatusPrefix))
	{
		SetStatusMessage(Line.RightChop(StatusPrefix.Len()));
		return;
	}

	if (Line.StartsWith(PongPrefix))
	{
		SetStatusMessage(TEXT("专用服务器延迟检测完成"));
	}
}

void UDemoDedicatedServerSubsystem::SetStatusMessage(const FString& Message)
{
	LastStatusMessage = Message;
	OnStatusMessage.Broadcast(LastStatusMessage);
}

void UDemoDedicatedServerSubsystem::BroadcastError(const FString& Code, const FString& Message)
{
	LastStatusMessage = Message;
	OnServerError.Broadcast(Code, Message);
}

void UDemoDedicatedServerSubsystem::ResetSessionState()
{
	ReceiveBuffer.Reset();
	ConnectionId.Reset();
	PlayerId.Reset();
	CurrentMatchId.Reset();
	CurrentMapName.Reset();
	bAuthenticated = false;
	LastSnapshot = FDemoDedicatedServerSnapshot();
	InventoryItems.Reset();
	ContainerSnapshots.Reset();
}

bool UDemoDedicatedServerSubsystem::ParseSnapshotJson(
	const FString& JsonPayload,
	FDemoDedicatedServerSnapshot& OutSnapshot) const
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogDemoClient, Warning, TEXT("Failed to parse dedicated server snapshot json."));
		return false;
	}

	OutSnapshot.MatchId = RootObject->GetStringField(TEXT("matchId"));
	OutSnapshot.MapName = RootObject->GetStringField(TEXT("mapName"));
	OutSnapshot.Tick = RootObject->GetIntegerField(TEXT("tick"));
	OutSnapshot.ServerTime = static_cast<int64>(RootObject->GetNumberField(TEXT("serverTime")));
	RootObject->TryGetNumberField(TEXT("matchRemainingSeconds"), OutSnapshot.MatchRemainingSeconds);
	RootObject->TryGetBoolField(TEXT("matchExpired"), OutSnapshot.bMatchExpired);

	const TArray<TSharedPtr<FJsonValue>>* PlayerValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("players"), PlayerValues))
	{
		for (const TSharedPtr<FJsonValue>& PlayerValue : *PlayerValues)
		{
			const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
			if (!PlayerObject.IsValid())
			{
				continue;
			}

			FDemoDedicatedServerPlayerSnapshot SnapshotPlayer;
			SnapshotPlayer.PlayerId = PlayerObject->GetStringField(TEXT("playerId"));
			SnapshotPlayer.DisplayName = PlayerObject->GetStringField(TEXT("displayName"));
			TryReadVector(PlayerObject, TEXT("position"), SnapshotPlayer.Position);
			TryReadRotator(PlayerObject, TEXT("rotation"), SnapshotPlayer.Rotation);
			TryReadVector(PlayerObject, TEXT("velocity"), SnapshotPlayer.Velocity);
			SnapshotPlayer.Health = PlayerObject->GetNumberField(TEXT("health"));
			SnapshotPlayer.Armor = PlayerObject->GetNumberField(TEXT("armor"));
			SnapshotPlayer.Stamina = PlayerObject->GetNumberField(TEXT("stamina"));
			SnapshotPlayer.CurrentAmmo = PlayerObject->GetIntegerField(TEXT("currentAmmo"));
			SnapshotPlayer.ReserveAmmo = PlayerObject->GetIntegerField(TEXT("reserveAmmo"));
			PlayerObject->TryGetBoolField(TEXT("reloading"), SnapshotPlayer.bReloading);
			SnapshotPlayer.bAlive = PlayerObject->GetBoolField(TEXT("alive"));
			PlayerObject->TryGetBoolField(TEXT("isBot"), SnapshotPlayer.bIsBot);
			SnapshotPlayer.Kills = PlayerObject->GetIntegerField(TEXT("kills"));
			SnapshotPlayer.Deaths = PlayerObject->GetIntegerField(TEXT("deaths"));
			PlayerObject->TryGetBoolField(TEXT("extracted"), SnapshotPlayer.bExtracted);
			SnapshotPlayer.ExtractedValue = PlayerObject->GetIntegerField(TEXT("extractedValue"));
			SnapshotPlayer.LastProcessedInputSequence =
				PlayerObject->GetIntegerField(TEXT("lastProcessedInputSequence"));
			OutSnapshot.Players.Add(MoveTemp(SnapshotPlayer));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* EventValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("events"), EventValues))
	{
		for (const TSharedPtr<FJsonValue>& EventValue : *EventValues)
		{
			const TSharedPtr<FJsonObject> EventObject = EventValue.IsValid() ? EventValue->AsObject() : nullptr;
			if (!EventObject.IsValid())
			{
				continue;
			}

			FDemoDedicatedServerSnapshotEvent SnapshotEvent;
			EventObject->TryGetStringField(TEXT("type"), SnapshotEvent.Type);
			EventObject->TryGetStringField(TEXT("playerId"), SnapshotEvent.PlayerId);
			EventObject->TryGetStringField(TEXT("sourcePlayerId"), SnapshotEvent.SourcePlayerId);
			EventObject->TryGetStringField(TEXT("targetPlayerId"), SnapshotEvent.TargetPlayerId);
			EventObject->TryGetNumberField(TEXT("health"), SnapshotEvent.Health);
			EventObject->TryGetNumberField(TEXT("armor"), SnapshotEvent.Armor);
			EventObject->TryGetNumberField(TEXT("ammo"), SnapshotEvent.Ammo);
			EventObject->TryGetStringField(TEXT("zoneId"), SnapshotEvent.ZoneId);
			EventObject->TryGetNumberField(TEXT("totalValue"), SnapshotEvent.TotalValue);
			EventObject->TryGetNumberField(TEXT("itemCount"), SnapshotEvent.ItemCount);
			OutSnapshot.Events.Add(MoveTemp(SnapshotEvent));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ContainerValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("containers"), ContainerValues))
	{
		for (const TSharedPtr<FJsonValue>& ContainerValue : *ContainerValues)
		{
			const TSharedPtr<FJsonObject> ContainerObject = ContainerValue.IsValid() ? ContainerValue->AsObject() : nullptr;
			if (!ContainerObject.IsValid())
			{
				continue;
			}

			FDemoDedicatedServerContainerSnapshot ContainerSnapshot;
			ContainerObject->TryGetStringField(TEXT("containerId"), ContainerSnapshot.ContainerId);
			TryReadVector(ContainerObject, TEXT("position"), ContainerSnapshot.Position);
			ContainerObject->TryGetBoolField(TEXT("opened"), ContainerSnapshot.bOpened);
			ContainerSnapshot.RemainingItemCount = ContainerObject->GetIntegerField(TEXT("remainingItemCount"));

			const TArray<TSharedPtr<FJsonValue>>* ContainerItemValues = nullptr;
			if (ContainerObject->TryGetArrayField(TEXT("contents"), ContainerItemValues))
			{
				ParseInventorySlotArray(*ContainerItemValues, ContainerSnapshot.Contents);
			}

			OutSnapshot.Containers.Add(MoveTemp(ContainerSnapshot));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* WorldLootValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("worldLoot"), WorldLootValues))
	{
		for (const TSharedPtr<FJsonValue>& LootValue : *WorldLootValues)
		{
			const TSharedPtr<FJsonObject> LootObject = LootValue.IsValid() ? LootValue->AsObject() : nullptr;
			if (!LootObject.IsValid())
			{
				continue;
			}

			FDemoDedicatedServerWorldLootSnapshot LootSnapshot;
			FString ItemIdString;
			LootObject->TryGetStringField(TEXT("lootId"), LootSnapshot.LootId);
			TryReadVector(LootObject, TEXT("position"), LootSnapshot.Position);
			LootObject->TryGetStringField(TEXT("itemId"), ItemIdString);
			LootSnapshot.ItemId = FName(*ItemIdString);
			LootSnapshot.Quantity = LootObject->GetIntegerField(TEXT("quantity"));
			if (!LootSnapshot.LootId.IsEmpty() && !LootSnapshot.ItemId.IsNone() && LootSnapshot.Quantity > 0)
			{
				OutSnapshot.WorldLoot.Add(MoveTemp(LootSnapshot));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ExtractionZoneValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("extractionZones"), ExtractionZoneValues))
	{
		for (const TSharedPtr<FJsonValue>& ZoneValue : *ExtractionZoneValues)
		{
			const TSharedPtr<FJsonObject> ZoneObject = ZoneValue.IsValid() ? ZoneValue->AsObject() : nullptr;
			if (!ZoneObject.IsValid())
			{
				continue;
			}

			FDemoDedicatedServerExtractionZoneSnapshot ZoneSnapshot;
			FString RawStatus;
			ZoneObject->TryGetStringField(TEXT("zoneId"), ZoneSnapshot.ZoneId);
			ZoneObject->TryGetStringField(TEXT("displayName"), ZoneSnapshot.DisplayName);
			TryReadVector(ZoneObject, TEXT("position"), ZoneSnapshot.Position);
			ZoneObject->TryGetNumberField(TEXT("radius"), ZoneSnapshot.Radius);
			ZoneObject->TryGetStringField(TEXT("status"), RawStatus);
			ZoneSnapshot.State = ParseExtractionState(RawStatus);
			ZoneObject->TryGetNumberField(TEXT("progress"), ZoneSnapshot.Progress);
			ZoneObject->TryGetStringField(TEXT("extractingPlayerId"), ZoneSnapshot.ExtractingPlayerId);
			OutSnapshot.ExtractionZones.Add(MoveTemp(ZoneSnapshot));
		}
	}

	return true;
}

bool UDemoDedicatedServerSubsystem::ParseInventoryJson(
	const FString& JsonPayload,
	TArray<FDemoInventorySlot>& OutItems) const
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ItemValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("items"), ItemValues))
	{
		return true;
	}

	ParseInventorySlotArray(*ItemValues, OutItems);
	return true;
}

bool UDemoDedicatedServerSubsystem::ParseContainerJson(
	const FString& JsonPayload,
	FDemoDedicatedServerContainerSnapshot& OutContainer) const
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	RootObject->TryGetStringField(TEXT("containerId"), OutContainer.ContainerId);
	TryReadVector(RootObject, TEXT("position"), OutContainer.Position);
	RootObject->TryGetBoolField(TEXT("opened"), OutContainer.bOpened);
	OutContainer.RemainingItemCount = RootObject->GetIntegerField(TEXT("remainingItemCount"));

	const TArray<TSharedPtr<FJsonValue>>* ItemValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("contents"), ItemValues))
	{
		ParseInventorySlotArray(*ItemValues, OutContainer.Contents);
	}

	return !OutContainer.ContainerId.IsEmpty();
}

void UDemoDedicatedServerSubsystem::ParseInventorySlotArray(
	const TArray<TSharedPtr<FJsonValue>>& JsonValues,
	TArray<FDemoInventorySlot>& OutItems)
{
	for (const TSharedPtr<FJsonValue>& ItemValue : JsonValues)
	{
		const TSharedPtr<FJsonObject> ItemObject = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
		if (!ItemObject.IsValid())
		{
			continue;
		}

		FDemoInventorySlot Slot;
		FString ItemId;
		if (!ItemObject->TryGetStringField(TEXT("itemId"), ItemId) || ItemId.IsEmpty())
		{
			continue;
		}

		Slot.ItemID = FName(*ItemId);
		Slot.Quantity = ItemObject->GetIntegerField(TEXT("quantity"));
		if (!Slot.IsEmpty())
		{
			OutItems.Add(MoveTemp(Slot));
		}
	}
}

bool UDemoDedicatedServerSubsystem::TryReadVector(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& FieldName,
	FVector& OutVector)
{
	const TSharedPtr<FJsonObject>* VectorObject = nullptr;
	if (!JsonObject.IsValid() || !JsonObject->TryGetObjectField(FieldName, VectorObject) || !VectorObject || !VectorObject->IsValid())
	{
		return false;
	}

	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	(*VectorObject)->TryGetNumberField(TEXT("x"), X);
	(*VectorObject)->TryGetNumberField(TEXT("y"), Y);
	(*VectorObject)->TryGetNumberField(TEXT("z"), Z);
	OutVector = FVector(X, Y, Z);
	return true;
}

bool UDemoDedicatedServerSubsystem::TryReadRotator(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& FieldName,
	FRotator& OutRotator)
{
	const TSharedPtr<FJsonObject>* RotatorObject = nullptr;
	if (!JsonObject.IsValid() || !JsonObject->TryGetObjectField(FieldName, RotatorObject) || !RotatorObject || !RotatorObject->IsValid())
	{
		return false;
	}

	double Yaw = 0.0;
	double Pitch = 0.0;
	(*RotatorObject)->TryGetNumberField(TEXT("yaw"), Yaw);
	(*RotatorObject)->TryGetNumberField(TEXT("pitch"), Pitch);
	OutRotator = FRotator(Pitch, Yaw, 0.0f);
	return true;
}

EDemoExtractionState UDemoDedicatedServerSubsystem::ParseExtractionState(const FString& RawState)
{
	if (RawState.Equals(TEXT("Available"), ESearchCase::IgnoreCase))
	{
		return EDemoExtractionState::Available;
	}

	if (RawState.Equals(TEXT("Extracting"), ESearchCase::IgnoreCase))
	{
		return EDemoExtractionState::Extracting;
	}

	if (RawState.Equals(TEXT("Completed"), ESearchCase::IgnoreCase))
	{
		return EDemoExtractionState::Completed;
	}

	return EDemoExtractionState::Inactive;
}
