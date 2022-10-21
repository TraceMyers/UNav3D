// Copyright Epic Games, Inc. All Rights Reserved.

#include "UNav3DCommands.h"

#define LOCTEXT_NAMESPACE "FUNav3DModule"

void FUNav3DCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "UNav3D", "Execute UNav3D action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
