#include "UNav3DBoundsVolume.h"

#include "UObject/ConstructorHelpers.h"

AUNav3DBoundsVolume::AUNav3DBoundsVolume() {
	PrimaryActorTick.bCanEverTick = false;

	BoundsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bounds Mesh"));
	SetRootComponent(BoundsMesh);
}

void AUNav3DBoundsVolume::BeginPlay() {
	Super::BeginPlay();
}

void AUNav3DBoundsVolume::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

}

FVector AUNav3DBoundsVolume::GetMaximum() {
	return FVector::ZeroVector;	
}

FVector AUNav3DBoundsVolume::GetMinimum() {
	return FVector::ZeroVector;	
}

