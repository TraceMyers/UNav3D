#include "UNav3D.h"
#include "UNav3DStyle.h"
#include "UNav3DCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "UNav3DBoundsVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopedSlowTask.h"
#include "DebugSwitches.h"
#include "Engine/StaticMeshActor.h"
#include "UNav3DBoundsVolume.h"

// using the default windows package define; would be better to determine this
#define _WIN32_WINNT_WIN10_TH2 0
#define _WIN32_WINNT_WIN10_RS1 0
#define _WIN32_WINNT_WIN10_RS2 0
#define _WIN32_WINNT_WIN10_RS3 0
#define _WIN32_WINNT_WIN10_RS4 0
#define _WIN32_WINNT_WIN10_RS5 0
#include <windows.h>
#include <stdio.h>

#include "GeometryProcessor.h"

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
	printf("hello sailor\n");
#endif
}

void FUNav3DModule::ShutdownModule() {
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FUNav3DStyle::Shutdown();
	FUNav3DCommands::Unregister();
}

void FUNav3DModule::PluginButtonClicked(){
	
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		UNAV_GENERR("GEditor or World Unavailable")
		return;
	}

	// If we find a single UNav3DBoundsVolume in the editor world, we're good
	if (!SetBoundsVolume()) {
		return;
	}

	// starting up progress bar
	constexpr int TotalCalls = 2;
	FScopedSlowTask ProgressTask(TotalCalls, FText::FromString("Generating UNav3D Data"));
	ProgressTask.MakeDialog(true);

	// find overlapping static mesh actors
	TArray<Geometry::TriMesh> TMeshes;
	BoundsVolume->GetOverlappingMeshes(TMeshes);
	if (TMeshes.Num() == 0) {
		UNAV_GENERR("No static mesh actors found inside the bounds volume.")
		return;
	}
	ProgressTask.EnterProgressFrame();

#ifdef UNAV_DBG
	for (int i = 0; i < TMeshes.Num(); i++) {
		printf("found mesh: %s\n", TCHAR_TO_ANSI(*TMeshes[i].MeshActor->GetName()));
	}
#endif

	// getting geometry data and populating the TriMeshes with it
	for (int i = 0; i < TMeshes.Num(); i++) {
		GeomProcessor.PopulateTriMesh(TMeshes[i]);
	}
	ProgressTask.EnterProgressFrame();
	
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

bool FUNav3DModule::SetBoundsVolume() {
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(
		GEditor->GetEditorWorldContext().World(),
		AUNav3DBoundsVolume::StaticClass(),
		FoundActors
	);
	const int BoundsVolumeCt = FoundActors.Num();
	if (BoundsVolumeCt == 0) {
		UNAV_GENERR("No bounds volumes found.")
		return false;
	}
	if (BoundsVolumeCt > 1) {
		UNAV_GENERR("UNav3D Currently only supports one Navigation Volume.")
		return false;
	}
	BoundsVolume = Cast<AUNav3DBoundsVolume>(FoundActors[0]);
	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUNav3DModule, UNav3D)