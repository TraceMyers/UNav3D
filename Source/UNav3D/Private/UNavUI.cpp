#include "UNavUI.h"
#include "Data.h"
#include "Engine/StaticMeshActor.h"
#include "Draw.h"
#include "Debug.h"
#include "Kismet/GameplayStatics.h"

// TODO: find ADraw at open and delete

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

	// if the editor is exited incorrectly, the previous ADraw object is left in the world
	// PostLoad() might (always?) be called twice
	if (Draw == nullptr) {
		TArray<AActor*> DrawActors;
		UGameplayStatics::GetAllActorsOfClass(World, ADraw::StaticClass(), DrawActors);
		for (int i = 0; i < DrawActors.Num(); i++) {
			ADraw* DrawActor = Cast<ADraw>(DrawActors[i]);
			if (DrawActor != nullptr) {
				DrawActor->Destroy();
			}
		}
		
		Draw = Cast<ADraw>(World->SpawnActor(ADraw::StaticClass()));
		Draw->SetFolderPath("/UNav3D");
	}
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

// TODO: move this to Draw w/ procedural mesh
void UNavUI::DrawFailureCases() {
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		printf("UNavUI::DrawFailureCases() world unavailable\n");
		return;
	}
	const UWorld* World = GEditor->GetEditorWorldContext().World();
	UNavDbg::DrawTris(World, Data::FailureCaseTris);
	UNavDbg::DrawPolygons(World, Data::FailureCasePolygons);
}

void UNavUI::DrawCulledTris() {
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		printf("UNavUI::DrawFailureCases() world unavailable\n");
		return;
	}
	const UWorld* World = GEditor->GetEditorWorldContext().World();
	UNavDbg::DrawTris(World, Data::CulledTris, FColor::Blue);
}

