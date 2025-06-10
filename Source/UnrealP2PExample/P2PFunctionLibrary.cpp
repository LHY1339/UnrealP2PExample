// Fill out your copyright notice in the Description page of Project Settings.


#include "P2PFunctionLibrary.h"

TArray<FString> UP2PFunctionLibrary::BreakString(FString BaseStr, FString Breaker)
{
	TArray<FString> rtn_value;
	FString left_string = BaseStr;
	while (true)
	{
		int index = left_string.Find(Breaker);
		if (index == INDEX_NONE)
		{
			break;
		}
		FString sub_str = left_string.Mid(0, index);
		rtn_value.Add(sub_str);
		left_string = left_string.Mid(index + 1);
	}
	return rtn_value;
}
