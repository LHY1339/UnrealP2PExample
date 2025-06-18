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

bool FSession::operator==(const FSession& Other)
{
	return IP == Other.IP &&
		Port == Other.Port &&
		ID == Other.ID &&
		Property == Other.Property;
}

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
	                .WithSendBufferSize(512);

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
	SendMessageBySocket(MessageSocket, "@mr#\0", ServerIP, ServerPort);
	if (MySession.ID > 0)
	{
		InitListenSocket();

		const int session_id = MySession.ID;
		const FString session_password = MySession.Password.IsEmpty() ? "f" : "t";
		const FString session_property = MySession.Property;
		const FString send_message = FString::Printf(TEXT("@lr#%d#%s#%s#\0"), session_id, *session_password, *session_property);
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
			FString send_message = FString::Printf(TEXT("@pm#%s#%d#%s#\0"), *PingList[i].IP, PingList[i].Port, *PingList[i].Password);
			SendMessageBySocket(ListenSocket, send_message, ServerIP, ServerPort);
		}
	}
}

void UP2PSubsystem::TickRecv(float DeltaTime)
{
	InitMessageSocket();
	uint8 Data[4096];
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
	else if (param_list[0] == "@p")
	{
		CmdPing(param_list, RecvString);
	}
	else if (param_list[0] == "@s")
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
	if (ParamList.Num() < 4)
	{
		return;
	}
	if (MySession.Password.IsEmpty() || MySession.Password == ParamList[3])
	{
		Ping(ParamList[1], FCString::Atoi(*ParamList[2]));
	}
}

void UP2PSubsystem::CmdSession(TArray<FString> ParamList, FString RecvString)
{
	SessionList.Empty();
	for (int i = 0; i < ParamList.Num(); i++)
	{
		//UKismetSystemLibrary::PrintString(GetWorld(), ParamList[i]);
		TArray<FString> session_param_list = UP2PFunctionLibrary::BreakString(ParamList[i], "/");
		if (session_param_list.Num() < 5)
		{
			continue;
		}
		FSession new_session;
		new_session.IP = session_param_list[0];
		new_session.Port = FCString::Atoi(*session_param_list[1]);
		new_session.ID = FCString::Atoi(*session_param_list[2]);
		new_session.Password = session_param_list[3];
		new_session.Property = session_param_list[4];
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

void UP2PSubsystem::SetMySessionProperty(FString Property)
{
	MySession.Property = Property;
}

void UP2PSubsystem::SetMySessionPassword(FString Password)
{
	MySession.Password = Password;
}

FSession UP2PSubsystem::GetMySessionProperty()
{
	return MySession;
}

void UP2PSubsystem::Connect(FString IP, int Port, FString Password)
{
	GetWorld()->GetFirstPlayerController()->ClientTravel(FString::Printf(TEXT("%s:%d"), *IP, Port), ETravelType::TRAVEL_Absolute);

	FPing new_ping;
	new_ping.IP = IP;
	new_ping.Port = Port;
	new_ping.Password = Password;
	PingList.Add(new_ping);
}

TArray<FSession> UP2PSubsystem::GetSessionList()
{
	return SessionList;
}

void UP2PSubsystem::FlushSessionList()
{
	InitMessageSocket();
	SendMessageBySocket(MessageSocket, "@gs#", ServerIP, ServerPort);
}

FString UP2PSubsystem::GetLegalString(FString InStr)
{
	InStr.ReplaceInline(TEXT("\\"), TEXT(""));
	InStr.ReplaceInline(TEXT("/"), TEXT(""));
	InStr.ReplaceInline(TEXT("#"), TEXT(""));
	return InStr;
}
