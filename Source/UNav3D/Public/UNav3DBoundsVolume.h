#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UNav3DBoundsVolume.generated.h"

class GeometryProcessor;
class UBoxComponent;

UCLASS()
class UNAV3D_API AUNav3DBoundsVolume : public AActor {
	GENERATED_BODY()
	
public:	

	AUNav3DBoundsVolume();
	virtual void Tick(float DeltaTime) override;
	
	// Returns any Static Mesh whose *bounding box extents* overlap BoundsBox
	TArray<AStaticMeshActor*>& GetOverlappingStaticMeshActors();
	
	// Checks if Point is inside BoundsBox
	bool IsPointInside(const FVector& Point) const;
	
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UStaticMeshComponent* BoundsMesh;
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UBoxComponent* BoundsBox;
	UPROPERTY()
	TArray<AStaticMeshActor*> OverlappingMeshActors;

protected:

	virtual void BeginPlay() override;

private:
	
	// Gets BoundsBox scaled extents, rotates and translates them in world space
	void SetBoundsCheckValues();

	// TODO: Checks if a line segment intersects a side of BoundsBox
	bool DoesLineSegmentIntersect(const FVector& A, const FVector& B);

	// For use with Bounds Checking
	static constexpr int RECT_PRISM_PTS = 8;
	const FVector BaseExtents[RECT_PRISM_PTS] {
		FVector(-1.0f, -1.0f, -1.0f),
		FVector(1.0f, -1.0f, -1.0f),
		FVector(-1.0f, 1.0f, -1.0f),
		FVector(-1.0f, -1.0f, 1.0f),
		FVector(1.0f, 1.0f, -1.0f),
		FVector(1.0f, -1.0f, 1.0f),
		FVector(-1.0f, 1.0f, 1.0f),
		FVector(1.0f, 1.0f, 1.0f)
	};
	FVector RefPoint;
	FVector IntersectionVectors[3];
	float IntersectionSqMags[3];

};
