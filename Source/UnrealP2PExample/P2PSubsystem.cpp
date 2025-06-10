#include "P2PSubsystem.h"

#include "IpNetDriver.h"
#include "P2PFunctionLibrary.h"
#include "Common/UdpSocketBuilder.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/KismetSystemLibrary.h"

DECLARE_STATS_GROUP(TEXT("P2PSubsystem"), STATGROUP_P2PSubsystem, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("P2PSubsystem Tick"), STAT_P2PSubsystemTick, STATGROUP_P2PSubsystem);

#define TICK_SERVER_MAX_SLEEP_TIME 1.0f
#define TICK_PING_MAX_SLEEP_TIME 1.0f

void UP2PSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	IsInit = true;
}

void UP2PSubsystem::Tick(float DeltaTime)
{
	TickRecv(DeltaTime);
	TickPing(DeltaTime);
	if (UKismetSystemLibrary::IsServer(GetWorld()) &&
		!UKismetSystemLibrary::IsStandalone(GetWorld()))
	{
		TickServer(DeltaTime);
	}
}

bool UP2PSubsystem::IsTickable() const
{
	return IsInit && GetWorld() != nullptr;
}

TStatId UP2PSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UP2PSubsystem, STATGROUP_P2PSubsystem);
}

void UP2PSubsystem::InitMessageSocket()
{
	if (MessageSocket)
	{
		return;
	}
	MessageSocket = FUdpSocketBuilder(TEXT("MessageSocket"))
	                .AsReusable()
	                .WithBroadcast()
	                .WithSendBufferSize(1024);

	TSharedRef<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	LocalAddr->SetAnyAddress();
	LocalAddr->SetPort(0);
	MessageSocket->Bind(*LocalAddr);
}

void UP2PSubsystem::InitListenSocket()
{
	TArray<FNamedNetDriver> drivers = GEngine->GetWorldContextFromWorldChecked(GetWorld()).ActiveNetDrivers;
	for (const FNamedNetDriver& DriverEntry : drivers)
	{
		UIpNetDriver* ip_net_driver = Cast<UIpNetDriver>(DriverEntry.NetDriver);
		if (ip_net_driver && ip_net_driver->GetSocket())
		{
			ListenSocket = ip_net_driver->GetSocket();
			return;
		}
	}
}

void UP2PSubsystem::TickServer(float DeltaTime)
{
	TickServerSleepTime += DeltaTime;
	if (TickServerSleepTime <= TICK_SERVER_MAX_SLEEP_TIME)
	{
		return;
	}
	TickServerSleepTime = 0.0f;

	InitMessageSocket();
	SendMessageBySocket(MessageSocket, "@message#register#\0", ServerIP, ServerPort);
	if (MySession.ID > 0)
	{
		InitListenSocket();

		const FString session_name = MySession.Name;
		const int session_id = MySession.ID;
		const FString session_level = GetWorld()->GetName();
		const int session_num = GetWorld()->GetAuthGameMode()->GetNumPlayers();
		const FString send_message = FString::Printf(TEXT("@listen#register#%d#%s#%s#%d#\0"), session_id, *session_name, *session_level, session_num);
		SendMessageBySocket(ListenSocket, send_message, ServerIP, ServerPort);
	}
}

void UP2PSubsystem::TickPing(float DeltaTime)
{
	TickPingSleepTime += DeltaTime;
	if (TickPingSleepTime <= TICK_PING_MAX_SLEEP_TIME)
	{
		return;
	}
	TickPingSleepTime = 0.0f;

	for (int i = 0; i < PingList.Num(); i++)
	{
		PingList[i].Times--;
		if (PingList[i].Times <= 0)
		{
			PingList.RemoveAt(i);
			continue;
		}
		InitListenSocket();
		if (ListenSocket)
		{
			FString send_message = FString::Printf(TEXT("@ping_me#%s#%d#\0"), *PingList[i].IP, PingList[i].Port);
			SendMessageBySocket(ListenSocket, send_message, ServerIP, ServerPort);
		}
	}
}

void UP2PSubsystem::TickRecv(float DeltaTime)
{
	InitMessageSocket();
	uint8 Data[1024];
	int32 BytesReceived = 0;
	if (MessageSocket->Recv(Data, sizeof(Data), BytesReceived))
	{
		FString ReceivedString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data)));
		HandleMessage(ReceivedString);
	}
}

void UP2PSubsystem::HandleMessage(FString RecvString)
{
	TArray<FString> param_list = UP2PFunctionLibrary::BreakString(RecvString, "#");
	if (param_list.Num() < 1)
	{
		return;
	}
	if (param_list[0] == "@id")
	{
		CmdID(param_list, RecvString);
	}
	else if (param_list[0] == "@ping")
	{
		CmdPing(param_list, RecvString);
	}
	else if (param_list[0] == "@session")
	{
		CmdSession(param_list, RecvString);
	}
}

void UP2PSubsystem::CmdID(TArray<FString> ParamList, FString RecvString)
{
	if (ParamList.Num() < 2)
	{
		return;
	}
	MySession.ID = FCString::Atoi(*ParamList[1]);
}

void UP2PSubsystem::CmdPing(TArray<FString> ParamList, FString RecvString)
{
	if (ParamList.Num() < 3)
	{
		return;
	}
	Ping(ParamList[1], FCString::Atoi(*ParamList[2]));
}

void UP2PSubsystem::CmdSession(TArray<FString> ParamList, FString RecvString)
{
	SessionList.Empty();
	for (int i = 0; i < ParamList.Num(); i++)
	{
		TArray<FString> session_param_list = UP2PFunctionLibrary::BreakString(ParamList[i], "/");
		if (session_param_list.Num() < 6)
		{
			continue;
		}
		FSession new_session;
		new_session.IP = session_param_list[0];
		new_session.Port = FCString::Atoi(*session_param_list[1]);
		new_session.ID = FCString::Atoi(*session_param_list[2]);
		new_session.Name = session_param_list[3];
		new_session.Level = session_param_list[4];
		new_session.PlayerNum = FCString::Atoi(*session_param_list[5]);
		SessionList.Add(new_session);
	}
}

bool UP2PSubsystem::SendMessageBySocket(FSocket* InSocket, FString Message, FString TargetIP, int TargetPort)
{
	if (!InSocket || Message.IsEmpty())
	{
		return false;
	}

	FIPv4Address RemoteIP;
	FIPv4Address::Parse(TargetIP, RemoteIP);

	TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	RemoteAddr->SetIp(RemoteIP.Value);
	RemoteAddr->SetPort(TargetPort);

	FTCHARToUTF8 Converted(*Message);

	TArray<uint8> Data;
	Data.Append((uint8*)Converted.Get(), Converted.Length());

	int32 BytesSent = 0;
	bool bSendSuccess = InSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *RemoteAddr);
	return bSendSuccess;
}

void UP2PSubsystem::SetServer(FString IP, int Port)
{
	ServerIP = IP;
	ServerPort = Port;
}

void UP2PSubsystem::Ping(FString TargetIP, int TargetPort)
{
	InitListenSocket();
	SendMessageBySocket(ListenSocket, "@\0", TargetIP, TargetPort);
}

void UP2PSubsystem::SetMySessionProperty(FString Name, int PlayerNum)
{
	MySession.Name = Name;
	MySession.PlayerNum = PlayerNum;
}

FSession UP2PSubsystem::GetMySessionProperty()
{
	return MySession;
}

void UP2PSubsystem::Connect(FString IP, int Port)
{
	GetWorld()->GetFirstPlayerController()->ClientTravel(FString::Printf(TEXT("%s:%d"), *IP, Port), ETravelType::TRAVEL_Absolute);

	FPing new_ping;
	new_ping.IP = IP;
	new_ping.Port = Port;
	PingList.Add(new_ping);
}

TArray<FSession> UP2PSubsystem::GetSessionList()
{
	return SessionList;
}

void UP2PSubsystem::FlushSessionList()
{
	InitMessageSocket();
	SendMessageBySocket(MessageSocket, "@get#session#", ServerIP, ServerPort);
}
