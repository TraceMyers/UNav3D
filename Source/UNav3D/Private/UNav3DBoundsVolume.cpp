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

TArray<AStaticMeshActor*>& AUNav3DBoundsVolume::GetOverlappingStaticMeshActors() {
	OverlappingMeshActors.Empty(32);
	
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		return OverlappingMeshActors;
	}
	
	SetBoundsCheckValues();
	
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
		
		const FVector Extents = Mesh->GetBoundingBox().GetExtent();
		const FTransform TForm = FoundMesh->GetTransform();
		for (int j = 0; j < RECT_PRISM_PTS; j++) {
			FVector Vertex = TForm.TransformPosition(BaseExtents[j] * Extents);
			if (IsPointInside(Vertex)) {
				OverlappingMeshActors.Add(FoundMesh);
				break;
			}
		}	
	}
	return OverlappingMeshActors;
}


void AUNav3DBoundsVolume::SetBoundsCheckValues() {
	// we only need 4 vertices for overlap checking
	FVector BoxVertices[4];
	const FVector Extents = BoundsBox->GetScaledBoxExtent();
	const FTransform TForm = BoundsBox->GetComponentTransform();
	for (int i = 0; i < 4; i++) {
		BoxVertices[i] = TForm.TransformPositionNoScale(BaseExtents[i] * Extents);
	}
	RefPoint = BoxVertices[0];
	for (int i = 0; i < 3; i ++) {
		IntersectionVectors[i] = BoxVertices[i + 1] - RefPoint;
		IntersectionSqMags[i] = FVector::DotProduct(IntersectionVectors[i], IntersectionVectors[i]);	
	}
}

// Checks in a way that reduces comparisons by subtracting the point from a BoundsBox point
// Ref: https://math.stackexchange.com/questions/1472049/check-if-a-point-is-inside-a-rectangular-shaped-area-3d
bool AUNav3DBoundsVolume::IsPointInside(const FVector& Point) const {
	const FVector V = Point - RefPoint;
	float SideCheckA = FVector::DotProduct(V, IntersectionVectors[0]);
	if (SideCheckA <= 0.0f) {
		return false;
	}
	if (SideCheckA >= IntersectionSqMags[0]) {
		return false;
	}
	SideCheckA = FVector::DotProduct(V, IntersectionVectors[1]);
	if (SideCheckA <= 0.0f) {
		return false;
	}
	if (SideCheckA >= IntersectionSqMags[1]) {
		return false;
	}
	SideCheckA = FVector::DotProduct(V, IntersectionVectors[2]);
	if (SideCheckA <= 0.0f) {
		return false;
	}
	if (SideCheckA >= IntersectionSqMags[2]) {
		return false;
	}
	return true;
}

// TODO: test whether or not bounding box edges of static meshes pass through BoundsBox
bool AUNav3DBoundsVolume::DoesLineSegmentIntersect(const FVector& A, const FVector& B) {
	return false;	
}
