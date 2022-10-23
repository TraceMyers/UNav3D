#include "DebugMarker.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ADebugMarker::ADebugMarker() {
	if (WITH_EDITOR) {
		PrimaryActorTick.bCanEverTick = true;
		PrimaryActorTick.bStartWithTickEnabled = true;
	}
	InitialLifeSpan = 1.0f;
	SetActorEnableCollision(false);
	
	MeshCmp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshCmp);
	static ConstructorHelpers::FObjectFinder<UStaticMesh>DbgMarkerAsset(TEXT("/UNav3D/Internal/DebugMarkerMesh.DebugMarkerMesh"));
	MeshCmp->SetStaticMesh(DbgMarkerAsset.Object);
}

bool ADebugMarker::ShouldTickIfViewportsOnly() const {
	return true;
}

void ADebugMarker::BeginPlay() {
	Super::BeginPlay();
}

void ADebugMarker::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	Ctr += DeltaTime;
	if (Ctr >= InitialLifeSpan) {
		Destroy();
	}
}

void ADebugMarker::Spawn(UWorld* World, const FVector& Location, float LifeSpan, float Scale) {
	ADebugMarker* Marker = Cast<ADebugMarker>(World->SpawnActor(StaticClass()));
	Marker->SetActorLocation(Location);
	Marker->SetActorScale3D(Scale * FVector::OneVector);
	Marker->InitialLifeSpan = LifeSpan;
}

