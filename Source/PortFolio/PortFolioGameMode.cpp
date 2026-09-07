// Copyright Epic Games, Inc. All Rights Reserved.

#include "PortFolioGameMode.h"
#include "PortFolioCharacter.h"

#include "UObject/ConstructorHelpers.h"

APortFolioGameMode::APortFolioGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
