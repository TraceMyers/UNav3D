#include "UNavUI.h"
#include "Data.h"
#include "Engine/StaticMeshActor.h"
#include "Draw.h"

UNavUI::UNavUI() {
	
}

UNavUI::~UNavUI() {}

void UNavUI::PostLoad() {
	Super::PostLoad();
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		printf("UNavUI::PostLoad() world unavailable\n");
		return;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	Draw = Cast<ADraw>(World->SpawnActor(ADraw::StaticClass()));
}

void UNavUI::HideAndShowAllStaticMeshes() {
	if (Data::NMeshes.Num() > 0) {
		// doing it this way both because meshes will be individually selectable for visibility
		// later and it's the easiest way to get at least one mesh to flip visibility on the first click.
		const bool Mesh0VisibilityFlip = !Data::NMeshes[0].MeshActors[0]->GetStaticMeshComponent()->GetVisibleFlag();
		for (auto& NMesh : Data::NMeshes) {
			for (const auto& MeshActor : NMesh.MeshActors) {
				MeshActor->GetStaticMeshComponent()->SetVisibility(Mesh0VisibilityFlip);
			}
		}
	}
}

void UNavUI::HideAndShowAllNavMeshes() {
	if (Draw != nullptr) {
		Draw->HideAndShowAllNavMeshes();
	}
}

