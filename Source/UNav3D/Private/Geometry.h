#pragma once
#include "TriMesh.h"

class AStaticMeshActor;
class UBoxComponent;


struct BoundingBox;
struct TriMesh;
struct UnstructuredPolygon;

namespace Geometry {

	// Populates a BoundingBox from a AStaticMeshActor
	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor);

	// Populates a BoundingBox from a UBoxComponent
	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp);

	// Checks whether or not the two bounding boxes overlap. Seemingly necessary to do this on our own for editor plugin.
	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB);

	// Checks whether two meshes overlap. Only those meshes in TMeshes with an index in PotentialOverlapIndices are checked.
	bool GetTriMeshIntersectGroups(
		TArray<int>& OverlapIndices,
		const TArray<int>& PotentialOverlapIndices,
		const TriMesh& TMesh,
		const TArray<TriMesh>& TMeshes
	);

	// Gets the extrema of world axis-aligned Bounding Box of a group of TMeshes
	void GetGroupExtrema(TArray<TriMesh*> TMeshes, FVector& Min, FVector& Max, bool NudgeOutward=false);

	void GetAxisAlignedExtrema(const BoundingBox& BBox, FVector& Min, FVector& Max, float NudgeOutward=0.0f);

	// find all intersections between tris and create a picture of where each tri is inside and where it's outside
	// other meshes
	void PopulateUnstructuredPolygons(
		TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& GroupUPolys
	);
	
}
