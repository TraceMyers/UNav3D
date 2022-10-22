#include "UNav3DBoundsVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"

AUNav3DBoundsVolume::AUNav3DBoundsVolume() {
	PrimaryActorTick.bCanEverTick = false;

	BoundsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bounds Mesh"));
	SetRootComponent(BoundsMesh);
	BoundsMesh->SetWorldScale3D(FVector(10.0f, 10.0f, 10.0f));
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds Box"));
	BoundsBox->SetupAttachment(BoundsMesh);
	BoundsBox->SetBoxExtent(FVector(5.0f, 5.0f, 5.0f));
}

void AUNav3DBoundsVolume::BeginPlay() {
	Super::BeginPlay();
}

void AUNav3DBoundsVolume::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void AUNav3DBoundsVolume::GetOverlappingMeshes(TArray<Geometry::TriMesh>& Meshes) {
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

		Geometry::TriMesh TMesh;
		TMesh.MeshActor = FoundMesh;
		Geometry::SetBoundingBox(TMesh.Box, FoundMesh);
		// DoBoundingBoxesOverlap() checks if B pts are in A first, so this order is helpful for resolving positive
		// results faster
		if (Geometry::DoBoundingBoxesOverlap(OverlapBBox, TMesh.Box)) {
			Meshes.Add(TMesh);
		}
	}
}

void AUNav3DBoundsVolume::GetInnerTris(const Geometry::TriMesh& TMesh, TArray<int>& OutIndices) const {
	
}
