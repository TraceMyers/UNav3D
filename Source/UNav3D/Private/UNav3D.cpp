#include "UNav3D.h"
#include "UNav3DStyle.h"
#include "UNav3DCommands.h"
#include "ToolMenus.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopedSlowTask.h"
#include "Debug.h"
#include "Misc/FeedbackContext.h"
#include "DataProcessing.h"

// using the default windows package define; would be better to determine this
#define _WIN32_WINNT_WIN10_TH2 0
#define _WIN32_WINNT_WIN10_RS1 0
#define _WIN32_WINNT_WIN10_RS2 0
#define _WIN32_WINNT_WIN10_RS3 0
#define _WIN32_WINNT_WIN10_RS4 0
#define _WIN32_WINNT_WIN10_RS5 0
#include <windows.h>
#include <stdio.h>

#define LOCTEXT_NAMESPACE "UNav3D"

static const FName UNav3DTabName("UNav3D");

void FUNav3DModule::StartupModule() {
	FUNav3DStyle::Initialize();
	FUNav3DStyle::ReloadTextures();
	FUNav3DCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FUNav3DCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FUNav3DModule::PluginButtonClicked),
		FCanExecuteAction());
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUNav3DModule::RegisterMenus)
	);

#ifdef UNAV_DBG
	FILE *pFile = nullptr;
	AllocConsole();
	freopen_s(&pFile, "CONOUT$", "w", stdout);
	printf("Hello sailor!\n");
#endif

}

void FUNav3DModule::ShutdownModule() {
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FUNav3DStyle::Shutdown();
	FUNav3DCommands::Unregister();
}

void FUNav3DModule::PluginButtonClicked(){
	// For now, built-in button is full reset; in future, there will be a button for full reset and a button
	// for adding/changing existing data. Will also be moving over to using only editor utility widget instead
	// of built-in button.
	// Also, in release, TMeshes will probably get emptied after NMeshes are populated.
	if (!DataProcessing::Init()) {
		UNAV_GENERR("Failed to instantiate Data Processing threads.");
	}
	DataProcessing::TotalReload();
	DataProcessing::Cleanup();
}

void FUNav3DModule::RegisterMenus() {
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FUNav3DCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FUNav3DCommands::Get().PluginAction)
				);
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}



#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUNav3DModule, UNav3D)