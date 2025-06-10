#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P2PSubsystem.generated.h"

/*
	  ／＞　 フ
	 | 　_　_| 
   ／` ミ＿xノ 
  /　　　　 |
 /　 ヽ　　 ﾉ
│　　|　|　|
／￣|　　 |　|　|
(￣ヽ＿_ヽ_)__)
＼二)
 
因为傻逼虚幻在这几行写东西容易莫名其妙的报错
所以就先画只猫猫
*/

USTRUCT(BlueprintType)
struct FSession
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString IP = "";

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Port = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int ID = -1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Name = "NULL";

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Level = "NULL";

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int PlayerNum = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Password = "";

	bool operator==(const FSession& Other);
};

USTRUCT()
struct FPing
{
	GENERATED_BODY()

	UPROPERTY()
	FString IP;

	UPROPERTY()
	int Port;

	UPROPERTY()
	FString Password = "";

	UPROPERTY()
	int Times = 2;
};

UCLASS()
class UNREALP2PEXAMPLE_API UP2PSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	//override UGameInstanceSubsystem functions
	//重写UGameInstanceSubsystem的函数
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	//override FTickableGameObject functions
	//重写FTickableGameObject的函数
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	//custom functions
	//自定义函数
	void InitMessageSocket();
	void InitListenSocket();

	void TickServer(float DeltaTime);
	void TickPing(float DeltaTime);
	void TickRecv(float DeltaTime);

	void HandleMessage(FString RecvString);
	void CmdID(TArray<FString> ParamList, FString RecvString);
	void CmdPing(TArray<FString> ParamList, FString RecvString);
	void CmdSession(TArray<FString> ParamList, FString RecvString);

	bool SendMessageBySocket(FSocket* InSocket, FString Message, FString TargetIP, int TargetPort);

	//blueprint functions
	//公开给蓝图的函数
	UFUNCTION(BlueprintCallable, Category="P2P")
	void SetServer(FString IP, int Port);

	UFUNCTION(BlueprintCallable, Category="P2P")
	void Ping(FString TargetIP, int TargetPort);

	UFUNCTION(BlueprintCallable, Category="P2P")
	void SetMySessionProperty(FString Name, FString Password);

	UFUNCTION(BlueprintCallable, Category="P2P")
	FSession GetMySessionProperty();

	UFUNCTION(BlueprintCallable, Category="P2P")
	void Connect(FString IP, int Port, FString Password);

	UFUNCTION(BlueprintCallable, Category="P2P")
	TArray<FSession> GetSessionList();

	UFUNCTION(BlueprintCallable, Category="P2P")
	void FlushSessionList();

private:
	bool IsInit = false;

	FSocket* MessageSocket = nullptr;
	FSocket* ListenSocket = nullptr;

	FString ServerIP = "";
	int ServerPort = 0;

	float TickServerSleepTime = 0.0f;
	float TickPingSleepTime = 0.0f;

	FSession MySession;

	TArray<FPing> PingList;
	TArray<FSession> SessionList;
};
