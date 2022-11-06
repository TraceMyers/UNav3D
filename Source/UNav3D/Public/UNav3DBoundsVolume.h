#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Geometry.h"
#include "TriMesh.h"
#include "Engine/StaticMeshActor.h"
#include "UNav3DBoundsVolume.generated.h"

class GeometryProcessor;
class UBoxComponent;
struct TriMesh;

UCLASS()
class UNAV3D_API AUNav3DBoundsVolume : public AStaticMeshActor {
	GENERATED_BODY()
	
public:	

	AUNav3DBoundsVolume();
	virtual void Tick(float DeltaTime) override;
	virtual void PostLoad() override;
	
	// Returns any Static Mesh whose *bounding box extents* overlap BoundsBox
	void GetOverlappingMeshes(TArray<TriMesh>& Meshes);

	// fills OutVertices with any tri indices in TMesh that are also inside the bounds volume
	void GetInnerTris(const TriMesh& TMesh, TArray<int>& OutIndices) const;

	const BoundingBox& GetBBox() const;

	TriMesh* GetTriMesh() const;

#ifdef UNAV_BNDVOL_DBG
	const FVector* GetVertices() const;
#endif

	UPROPERTY()
	UStaticMeshComponent* BoundsMesh;
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UBoxComponent* BoundsBox;
	

protected:

	virtual void BeginPlay() override;

private:
	
	// To check for overlaps with static meshes
	BoundingBox OverlapBBox;
	TriMesh* TMesh;

};
