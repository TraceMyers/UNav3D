#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Geometry.h"
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
	void GetOverlappingMeshes(TArray<Geometry::TriMesh>& Meshes);

	// fills OutVertices with any tri indices in TMesh that are also inside the bounds volume
	void GetInnerTris(const Geometry::TriMesh& TMesh, TArray<int>& OutIndices) const;

#ifdef UNAV_BNDVOL_DBG
	const FVector* GetVertices() const;
#endif
	
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UStaticMeshComponent* BoundsMesh;
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UBoxComponent* BoundsBox;
	

protected:

	virtual void BeginPlay() override;

private:
	
	// To check for overlaps with static meshes
	Geometry::BoundingBox OverlapBBox;

};
