#include "UNav3DBoundsVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Data.h"

AUNav3DBoundsVolume::AUNav3DBoundsVolume() {
	PrimaryActorTick.bCanEverTick = false;
    BoundsMesh = GetStaticMeshComponent();
	BoundsMesh->SetWorldScale3D(FVector(10.0f, 10.0f, 10.0f));
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds Box"));
	BoundsBox->SetupAttachment(BoundsMesh);
	BoundsBox->SetBoxExtent(FVector(5.0f, 5.0f, 5.0f));
}

void AUNav3DBoundsVolume::BeginPlay() {
	Super::BeginPlay();
	Destroy();
}

void AUNav3DBoundsVolume::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void AUNav3DBoundsVolume::PostLoad() {
	Super::PostLoad();
	SetFolderPath("/UNav3D");
}

void AUNav3DBoundsVolume::GetOverlappingMeshes(TArray<TriMesh>& Meshes) {
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		return ;
	}

	Geometry::SetBoundingBox(OverlapBBox, BoundsBox);
	
	// iterate over all static mesh actors in the level and populate Meshes with overlaps
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(
		GEditor->GetEditorWorldContext().World(),
		AStaticMeshActor::StaticClass(),
		FoundActors
	);
	
	if (Meshes.Num() > 0) {
		Meshes.Empty(FoundActors.Num());
	}
	else {
		Meshes.Reserve(FoundActors.Num());
	}
	for (int i = 0; i < FoundActors.Num(); i++) {
		AStaticMeshActor* FoundMesh = Cast<AStaticMeshActor>(FoundActors[i]);
		if (FoundMesh == nullptr) {
			continue;
		}
		const UStaticMeshComponent* MeshCmp = FoundMesh->GetStaticMeshComponent();
		if (MeshCmp == nullptr || MeshCmp == BoundsMesh) {
			continue;
		}
		const UStaticMesh* Mesh = MeshCmp->GetStaticMesh();
		if (Mesh == nullptr) {
			continue;	
		}

		TriMesh TrMesh;
		TrMesh.MeshActor = FoundMesh;
		Geometry::SetBoundingBox(TrMesh.Box, FoundMesh);
		// DoBoundingBoxesOverlap() checks if B pts are in A first, so this order is helpful for resolving positive
		// results faster
		if (Geometry::DoBoundingBoxesOverlap(OverlapBBox, TrMesh.Box)) {
			Meshes.Add(TrMesh);
		}
	}
	if (Meshes.Num() > 0) {
		Data::BoundsVolumeTMesh.ResetVertexData();
		Data::BoundsVolumeTMesh.MeshActor = this;
		Geometry::SetBoundingBox(Data::BoundsVolumeTMesh.Box, this);
	}
}

void AUNav3DBoundsVolume::GetInnerTris(const TriMesh& TrMesh, TArray<int>& OutIndices) const {
	
}

const BoundingBox& AUNav3DBoundsVolume::GetBBox() const {
	return OverlapBBox;
}

TriMesh* AUNav3DBoundsVolume::GetTriMesh() const {
	return TMesh;
}

#ifdef UNAV_BNDVOL_DBG
const FVector* AUNav3DBoundsVolume::GetVertices() const {
	return OverlapBBox.Vertices;
}
#endif
