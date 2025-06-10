#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "P2PFunctionLibrary.generated.h"

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

UCLASS()
class UNREALP2PEXAMPLE_API UP2PFunctionLibrary : public UObject
{
	GENERATED_BODY()

public:
	static TArray<FString> BreakString(FString BaseStr, FString Breaker);
};
