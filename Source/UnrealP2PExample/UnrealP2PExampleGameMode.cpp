// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealP2PExampleGameMode.h"
#include "UnrealP2PExampleCharacter.h"
#include "UObject/ConstructorHelpers.h"

AUnrealP2PExampleGameMode::AUnrealP2PExampleGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
