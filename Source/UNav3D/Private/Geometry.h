#pragma once
#include "TriMesh.h"

struct PolyNode;
class AStaticMeshActor;
class UBoxComponent;


struct BoundingBox;
struct TriMesh;
struct UnstructuredPolygon;

// Geometry's job is to provide information about geometrical objects
namespace Geometry {

	struct DoubleVector {
		
		static double SizeSquared(const FVector& V);
		
	};

	enum VERTEX_T {VERTEX_INTERIOR, VERTEX_EXTERIOR};
	
	// Populates a BoundingBox from a AStaticMeshActor
	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor);

	// Populates a BoundingBox from a UBoxComponent
	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp);

	// Populates a BoundingBox from a UNavMesh
	void SetBoundingBox(UNavMesh& NMesh, const TArray<TriMesh*> Group);

	// Checks whether or not the two bounding boxes overlap. Seemingly necessary to do this on our own for editor plugin.
	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB);

	// checks whether B fully envelops A
	bool IsBoxAInBoxB(const BoundingBox& BBoxA, const BoundingBox& BBoxB);

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

	void FlagTrisOutsideBox(const BoundingBox& BBox, const TriMesh& TMesh);

	// find all intersections between tris and create a picture of where each tri is inside and where it's outside
	// other meshes; if 'inside' edges connect, they form polygons
	void GetUPolygonsFromIntersections(
		TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& GroupUPolys,
		bool LastIsBoundsVolume=true
	);

	// Does P (roughly) lie on/inside Triangle ABC?
	bool DoesPointTouchTri(const FVector& A, const FVector& B, const FVector& C, const FVector& P);

	// Tests if P.Prev, P and P.Next make a triangle that contains no other points in the polygon
	// polygon must have >= 4 vertices
	bool IsEar(const PolyNode& P);

	// Given an ordered set of vertices a, b, c, d of a simple polygon, regardless of polygon orientation (cw or ccw),
	// and given the type of vertex b is known, the type of vertex c can be determined. if b,c,d make an interior
	// angle of the polygon, this returns VERTEX_INTERIOR, else VERTEX_EXTERIOR.
	// The normal can be up or down facing - it doesn't matter. W = -(b-a), V = c-b, U = d-c, PrevType = type of b.
	// To start, you can find an interior vertex by picking any point q in R^3 and getting the farthest vertex from q.
	// That vertex will be interior.
	VERTEX_T GetPolyVertexType(
		const FVector& Normal, const FVector& W, const FVector& V, const FVector& U, VERTEX_T PrevType
	);
}
