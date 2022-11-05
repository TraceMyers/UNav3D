#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GeometryProcessor.h"
#include "Draw.h"

// TODO: UNav3D should have no private includes

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
class AUNav3DBoundsVolume;
struct TriMesh;
class ADraw;

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
	bool SetBoundsVolume();

	// Takes meshes found inside bounds volume and populates TriMeshes with their data
	bool PopulateTriMeshes(UWorld* World, TArray<TriMesh>& TriMeshes, FScopedSlowTask& ProgressTask) const;
	
	TSharedPtr<FUICommandList> PluginCommands;
	AUNav3DBoundsVolume* BoundsVolume;
	GeometryProcessor GeomProcessor;
	ADraw* Draw;
	
	bool Initialized = false;
	
};

#undef LOCTEXT_NAMESPACE