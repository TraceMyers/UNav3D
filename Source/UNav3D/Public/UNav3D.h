#pragma once

#include "CoreMinimal.h"

class FToolBarBuilder;
class FMenuBuilder;
class FUICommandList;
struct TriMesh;

class FUNav3DModule : public IModuleInterface {
	
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Runs whenever the button at the top of the editor is clicked; current entry point for all plugin functionality
	void PluginButtonClicked();
	
private:

	void RegisterMenus();

	TSharedPtr<FUICommandList> PluginCommands;
	
};
