// Copyright Epic Games, Inc. All Rights Reserved.

#include "room_vizGameMode.h"
#include "room_vizCharacter.h"
#include "UObject/ConstructorHelpers.h"

Aroom_vizGameMode::Aroom_vizGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
