#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessor.h"

// TODO: UNav3D should have no private includes
// NOTE: vertices that are too close together will likely break this plugin
// NOTE: will require editor utility scripting plugin

#define LOCTEXT_NAMESPACE "UNav3D"
#define UNAV_GENMSG(x) \
	FMessageDialog::Open( \
		EAppMsgType::Ok, FText::Format(LOCTEXT("UNav3D", "UNAV3D Message: {0}", FText::FromString(x)) \
	);
#define UNAV_GENERR(x) \
	FMessageDialog::Open( \
		EAppMsgType::Ok, FText::Format(LOCTEXT("UNav3D", "UNAV3D ERROR: {0}"), FText::FromString(x)) \
	);

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

	// Finds bounds volume in editor viewport; returns false if number of bounds volumes is not 1
	bool SetBoundsVolume(const UWorld* World) const;

	// Takes meshes found inside bounds volume and populates TriMeshes with their data
	bool PopulateTriMeshes(TArray<TriMesh>& TriMeshes) const;

	void EnterProgressFrame(FScopedSlowTask& Task, const char* msg) const;
	
	TSharedPtr<FUICommandList> PluginCommands;
	GeometryProcessor GeomProcessor;
	
	bool Initialized = false;
	const int TotalTasks = 2;
	
};


#undef LOCTEXT_NAMESPACE
