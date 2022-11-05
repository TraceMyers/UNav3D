#include "UNav3D.h"
#include "UNav3DStyle.h"
#include "UNav3DCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "UNav3DBoundsVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopedSlowTask.h"
#include "Debug.h"
#include "GeometryProcessor.h"
#include "TriMesh.h"
#include "Data.h"
#include "UNavMesh.h"

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
	for (int i = 0; i < Data::TMeshes.Num(); i++) {
		Data::TMeshes[i].ResetVertexData();
	}
	for (int i = 0; i < Data::NMeshes.Num(); i++) {
		Data::NMeshes[i].ResetVertexData();
	}
	Data::TMeshes.Empty();
	Data::NMeshes.Empty();
	
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		UNAV_GENERR("GEditor or World Unavailable")
		return;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();

	// If we find a single UNav3DBoundsVolume in the editor world, we're good
	if (!SetBoundsVolume()) {
		return;
	}

	// starting up progress bar
	constexpr int TotalCalls = 2;
	FScopedSlowTask ProgressTask(TotalCalls, FText::FromString("Generating UNav3D Data"));
	ProgressTask.MakeDialog(true);

	// populate TriMeshes with vertices, tris
	if (!PopulateTriMeshes(World, Data::TMeshes, ProgressTask)) {
		return;
	}

	GeomProcessor.PopulateNavMeshes(World, Data::TMeshes, Data::NMeshes);
	
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

bool FUNav3DModule::PopulateTriMeshes(
	UWorld* World,
	TArray<TriMesh>& TMeshes,
	FScopedSlowTask& ProgressTask
) const {
	// find overlapping static mesh actors
	BoundsVolume->GetOverlappingMeshes(TMeshes);
	if (TMeshes.Num() == 0) {
		UNAV_GENERR("No static mesh actors found inside the bounds volume.")
		return false;
	}
	ProgressTask.EnterProgressFrame();

	// getting geometry data and populating the TriMeshes with it
	for (int i = 0; i < TMeshes.Num(); i++) {
		// setting the meshes to overlap in the trace channel used by this plugin
		TriMesh& TMesh = TMeshes[i];
		
		const GeometryProcessor::GEOPROC_RESPONSE Response = GeomProcessor.PopulateTriMesh(TMesh);
		if (Response == GeometryProcessor::GEOPROC_HIGH_INDEX) {
			UNAV_GENERR("One of the meshes was not processed correctly due to a bad index coming from the index buffer")
		}
		else if (Response == GeometryProcessor::GEOPROC_ALLOC_FAIL) {
			UNAV_GENERR("The Geometry Processor failed to allocate enough space for a mesh.")
		}

#ifdef UNAV_DBG
		UNavDbg::PrintTriMesh(TMesh);
		// UNavDbg::DrawTriMeshBoundingBox(World, TMesh);
#endif
		
	}
	ProgressTask.EnterProgressFrame();
	
	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUNav3DModule, UNav3D)