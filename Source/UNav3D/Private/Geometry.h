#pragma once

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

	// Flags tris in TMesh that have vertices inside any other mesh in OtherTMeshes.
	void FlagObscuredTris(
		const UWorld* World,
		TriMesh& TMesh,
		TArray<TriMesh*>& OtherTMeshes,
		const FVector& GroupExtMin // group bounding box extrema min
	);

	// 
	void PopulateUnstructuredPolygons(
		const UWorld* World,
		TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& GroupUPolys,
		const float BBoxDiagDist 
	);
	
	void PopulatePolyEdgesFromTriEdges(
		const UWorld* World,
		const TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& GroupUPolys,
		float BBoxDiagDistance
	);
}
