#include "UNav3DBoundsVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"

AUNav3DBoundsVolume::AUNav3DBoundsVolume() {
	PrimaryActorTick.bCanEverTick = false;

	BoundsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bounds Mesh"));
	SetRootComponent(BoundsMesh);
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds Box"));
	BoundsBox->SetupAttachment(BoundsMesh);
}

void AUNav3DBoundsVolume::BeginPlay() {
	Super::BeginPlay();
	printf("beginplay called\n");
}

void AUNav3DBoundsVolume::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

TArray<AActor*> AUNav3DBoundsVolume::GetOverlappingStaticMeshes() {
	TArray<AActor*> FoundActors;
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		return FoundActors;
	}
	UGameplayStatics::GetAllActorsOfClass(
		GEditor->GetEditorWorldContext().World(),
		AStaticMeshActor::StaticClass(),
		FoundActors
	);
	// BoundsBox->UpdateBodySetup();
	// BoundsBox->UpdateOverlaps();
	// BoundsBox->GetOverlappingActors(OverlappingStaticMeshes);
	for (int i = 0; i < FoundActors.Num(); i++) {
		AActor* FoundActor = FoundActors[i];
		if (FoundActor != this && BoundsBox->IsOverlappingActor(FoundActor)) {
			OverlappingStaticMeshes.Add(FoundActor);
		}
		else {
			printf("%s not inside", TCHAR_TO_ANSI(*FoundActor->GetName()));
		}
	}
	// BoundsBox->IsOverlappingActor()
	return OverlappingStaticMeshes;
}

