// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRiCCGameMode.h"
#include "VRiCCCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVRiCCGameMode::AVRiCCGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
