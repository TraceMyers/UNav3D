#pragma once

class AStaticMeshActor;
class UBoxComponent;

// TODO: consider creating a vertex buffer and tri buffer, rather than individual allocations per TriMesh

namespace Geometry {

	static constexpr int RECT_PRISM_PTS = 8;
	static constexpr int RECT_PRISM_FACES = 6;
	static constexpr int RECT_PRISM_EDGES = 12;
	static constexpr float ONE_THIRD = 1.0f / 3.0f;
	static constexpr float NEAR_EPSILON = 1e-4f;
	
	// Triangle; saves space by storing references to vertices on the TriMesh; stores references to adjacent Tris
	// Edges must be re-pointed if Tri's container reallocates
	struct Tri {
		
		Tri(const FVector& _A, const FVector& _B, const FVector& _C);
		~Tri();

		// meant for use in constructor
		static float GetLongestTriSidelen(const FVector& A, const FVector& B, const FVector& C);

		// get the area of a triangle with vertices A, B, C
		static float GetArea(const FVector& A, const FVector& B, const FVector& C);

		// get the area of tri T
		static float GetArea(const Tri& T);

		// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
		inline bool IsAObscured() const;
		
		// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
		inline bool IsBObscured() const;
		
		// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
		inline bool IsCObscured() const;

		const FVector& A;
		const FVector& B;
		const FVector& C;
		const FVector Normal;
		Tri* Edges[3]; // adjacent tris touching AB, BC, and CA in that order
		uint32 Flags;
		// for faster intersection checking
		const FVector Center;
		const float LongestSidelen;
		const float Area;
	};

	// For storing world bounding box vertices of UBoxComponents and AStaticMeshActors' Meshes.
	// When these overlap, we consider the possibility that the static meshes inside the boxes overlap.
	struct BoundingBox {
		FVector Vertices[RECT_PRISM_PTS];
		FVector FaceNormals[RECT_PRISM_FACES];
		FVector OverlapCheckVectors[3];
		float OverlapCheckSqMags[3];
	};
	
	// Mesh made of Tris, vertices populated from GPU vertex buffer.
	struct TriMesh {
		
		TriMesh();
		TriMesh(const TriMesh& OtherMesh);
		~TriMesh();

		// checks if the TMeshes point to the same static mesh
		bool operator == (const TriMesh& OtherMesh) const;

		// Clears Vertices and Tris, since they are inextricably linked
		void ResetVertexData();

		AStaticMeshActor* MeshActor;
		BoundingBox Box;
		int VertexCt;
		FVector* Vertices;
		TArray<Tri> Tris;
	};

	// edges are between adjacent vertices; index Num() - 1 connects to 0
	struct Polygon {
		TArray<FVector> Vertices;
	};

	struct PolyEdge {
		
		PolyEdge(const FVector& _A, const FVector& _B) :
			A{_A}, B{_B}
		{}
		
		FVector A;
		FVector B;
	};
	
	//
	struct UnstructuredPolygon {
		TArray<PolyEdge> Edges;	
	};

	// Populates a BoundingBox from a AStaticMeshActor
	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor);

	// Populates a BoundingBox from a UBoxComponent
	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp);

	// Checks whether or not the two bounding boxes overlap. Seemingly necessary to do this on our own for editor plugin.
	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB);

	// Checks whether two meshes overlap. May give false negatives due to imprecision of line trace checking
	bool GetTriMeshIntersectGroups(
		TArray<int>& OverlapIndices,
		const TArray<int>& PotentialOverlapIndices,
		const TriMesh& TMesh,
		const TArray<TriMesh>& TMeshes
	);

	// Gets the extrema of world axis-aligned Bounding Box of a group of TMeshes
	void GetGroupExtrema(TArray<TriMesh*> TMeshes, FVector& Min, FVector& Max, bool NudgeOutward=false);

	// Flags most obscured tris in TMesh that are fully or partly inside any other meshes in OtherTMeshes.
	// if any vertices are inside other meshes, that tri is definitely 'obscured'. Some tris will be obscured
	// even if they aren't caught here, since all of their vertices may be visible but they're intersected in the middle.
	void FlagObscuredTris(
		const UWorld* World,
		TriMesh& TMesh,
		const TArray<TriMesh*>& OtherTMeshes,
		const FVector& GroupExtMin // group bounding box extrema min
	);

	void PopulateUnstructuredPolygons(
		TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& UPolys
	);
	
	// NOTE: breaking this up with these ideas:
	// - enclosed: tri is fully enclosed by one mesh or a combination of meshes in its group
	// - obscured: tri is partially hidden by one mesh or a combination of meshes in its group
	// - unobscured: tri is fully visible in the context of its group
}
