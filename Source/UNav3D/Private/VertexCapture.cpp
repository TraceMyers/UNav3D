#include "VertexCapture.h"

AVertexCapture::AVertexCapture() {
	PrimaryActorTick.bCanEverTick = false;
}

void AVertexCapture::BeginPlay() {
	Super::BeginPlay();
	
}

void AVertexCapture::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

BoundingBox& AVertexCapture::GetBBox() {
	return Box;
}

