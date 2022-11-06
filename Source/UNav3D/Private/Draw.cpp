#include "Draw.h"

#include "Data.h"
#include "UNavMesh.h"
#include "SceneManagement.h"

namespace {
	
	UMaterial* LoadMaterial(const FName& Path) {
		if (Path == NAME_None) {
			return nullptr;
		}
		return Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *Path.ToString()));
	}
	
}

ADraw::ADraw() {
	if (WITH_EDITOR) {
		PrimaryActorTick.bCanEverTick = false;
		PrimaryActorTick.bStartWithTickEnabled = false;
	}
	// Mesh->bUseAsyncCooking = true;

	NavMeshes = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("NavMeshes"));
	NavMeshes->SetupAttachment(GetRootComponent());
	
	static const TCHAR* TriMatPath = TEXT("/UNav3D/Internal/M_Triangle.M_Triangle");
	TriMaterial = LoadMaterial(TriMatPath);
	
	// TODO: Investigate - crashed on this when adding unav3dmovementcomponent to src from editor
	SetFolderPath("/UNav3D");
}

void ADraw::Init() {
	static const TCHAR* TriMatPath = TEXT("/UNav3D/Internal/M_Triangle.M_Triangle");
	TriMaterial = LoadMaterial(TriMatPath);
}

void ADraw::HideAndShowAllNavMeshes() {
	if (NavMeshes->GetNumSections() == 0) {
		for (auto& NMesh : Data::NMeshes) {
			AddNavMesh(NMesh);	
		}
	}
	else {
		NavMeshes->ClearAllMeshSections();
	}
}

void ADraw::BeginPlay() {
	Super::BeginPlay();
}

void ADraw::AddNavMesh(UNavMesh& NMesh) {
	
	TArray<FVector> Vertices(NMesh.Vertices, NMesh.VertexCt);
	TArray<FVector> Normals;
	Normals.Init(FVector::ZeroVector, Vertices.Num());
	TArray<int32> Triangles;
	TArray<int32> Triangle;
	TArray<FColor> VertexColors;
	VertexColors.Init(FColor(10, 128, 96, 100), Vertices.Num());
	
	FVector CalculatedNormal;
	const auto& Grid = NMesh.Grid;
	for (int j = 0; j < Grid.Num(); j++) {
		Grid.GetIndices(j, Triangle);
		Tri& T = Grid[j];
		
		// TODO: do this in GeometryProcessor
		T.CalculateNormal(CalculatedNormal);
		if (FVector::DotProduct(T.Normal, CalculatedNormal) <= 0.0f) {
			const uint32 TempC = Triangle[0];
			Triangle[0] = Triangle[2];
			Triangle[2] = TempC;
		}
		
		Triangles.Append(Triangle);
		for (int k = 0; k < Triangle.Num(); k++) {
			Normals[Triangle[k]] += T.Normal;
		}
		Triangle.Empty(3);
	}
	for (int j = 0; j < Normals.Num(); j++) {
		Normals[j] = Normals[j].GetUnsafeNormal();	
	}
	
	const int NewIndex = NavMeshes->GetNumSections();
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;
	NavMeshes->CreateMeshSection(
		NewIndex,
		Vertices,
		Triangles,
		Normals,
		UV0,
		VertexColors,
		Tangents,
		false
	);
	NavMeshes->SetMeshSectionVisible(NewIndex, true);
	if (TriMaterial != nullptr) {
		NavMeshes->SetMaterial(NewIndex, TriMaterial);
	}
	ActorMemory.Add(NMesh.MeshActors);
}
