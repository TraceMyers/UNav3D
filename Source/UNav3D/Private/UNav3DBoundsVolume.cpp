#include "UNav3DBoundsVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"

// TODO: NavGraph will just reference TriMesh memory for node placement; a "Node" can be a reference to a tri
// TODO: have different kinds of nodes: Intersections and Throughways; Intersections require logic to decide.. 
// TODO: ... where to go; Throughways are bi-directional connectors to Intersections. Instead of 'node net normalization' ...
// TODO: ... just try to create a network of Intersections that are fairly evenly spaced except at critical rotation points

AUNav3DBoundsVolume::AUNav3DBoundsVolume() {
	PrimaryActorTick.bCanEverTick = false;

	BoundsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bounds Mesh"));
	SetRootComponent(BoundsMesh);
	BoundsMesh->SetWorldScale3D(FVector(10.0f, 10.0f, 10.0f));
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds Box"));
	BoundsBox->SetupAttachment(BoundsMesh);
	BoundsBox->SetBoxExtent(FVector(5.0f, 5.0f, 5.0f));
	
	SetFolderPath("/UNav3D");
}

void AUNav3DBoundsVolume::BeginPlay() {
	Super::BeginPlay();
	Destroy();
}

void AUNav3DBoundsVolume::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void AUNav3DBoundsVolume::GetOverlappingMeshes(TArray<TriMesh>& Meshes) {
	Meshes.Empty(32);
	
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

		// TODO: could instead initialize Meshes in chunks and set Meshes[i].MeshActor = FoundMesh, reducing expands...
		// TODO: ... and no need to copy
		TriMesh TMesh;
		TMesh.MeshActor = FoundMesh;
		Geometry::SetBoundingBox(TMesh.Box, FoundMesh);
		// DoBoundingBoxesOverlap() checks if B pts are in A first, so this order is helpful for resolving positive
		// results faster
		if (Geometry::DoBoundingBoxesOverlap(OverlapBBox, TMesh.Box)) {
			Meshes.Add(TMesh);
		}
	}
}

void AUNav3DBoundsVolume::GetInnerTris(const TriMesh& TMesh, TArray<int>& OutIndices) const {
	
}

#ifdef UNAV_BNDVOL_DBG
const FVector* AUNav3DBoundsVolume::GetVertices() const {
	return OverlapBBox.Vertices;
}
#endif
