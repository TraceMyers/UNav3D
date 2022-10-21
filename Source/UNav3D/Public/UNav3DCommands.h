// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "UNav3DStyle.h"

class FUNav3DCommands : public TCommands<FUNav3DCommands>
{
public:

	FUNav3DCommands()
		: TCommands<FUNav3DCommands>(TEXT("UNav3D"), NSLOCTEXT("Contexts", "UNav3D", "UNav3D Plugin"), NAME_None, FUNav3DStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
