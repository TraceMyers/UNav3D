#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "UNAV3D"
#define UNAV_DBG
#define UNAV_GENMSG(x) \
	FMessageDialog::Open( \
		EAppMsgType::Ok, FText::Format(LOCTEXT("UNAV3D", "UNAV3D Message: {0}", FText::FromString(x)) \
	);
#define UNAV_GENERR(x) \
	FMessageDialog::Open( \
		EAppMsgType::Ok, FText::Format(LOCTEXT("UNAV3D", "UNAV3D ERROR: {0}"), FText::FromString(x)) \
	);

class FToolBarBuilder;
class FMenuBuilder;
class FUICommandList;

class FUNav3DModule : public IModuleInterface {
	
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();
	
private:

	void RegisterMenus();
	
	TSharedPtr<FUICommandList> PluginCommands;
	
};
