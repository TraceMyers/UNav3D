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

		// are any vertices obscured?
		inline bool AnyObscured() const;

		// are all vertices obscured?
		inline bool AllObscured() const;

		void MarkForCull();

		void MarkProblemCase();

		// 48 bytes
		const FVector& A;
		const FVector& B;
		const FVector& C;
		const FVector Normal;
		uint32 Flags; // only using 2 of the 4 bytes
		// for faster intersection checking
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

	// edges are between adjacent vertices; index Num() - 1 connects to 0; Every polygon is a stepping stone
	// between a triangle that was intersected and a new set of triangles with no intersections.
	struct Polygon {
		Polygon(int _TMeshTriIndex)
			: TMeshTriIndex(_TMeshTriIndex), Mode(ADD)
		{}

		// 16 bytes
		TArray<FVector> Vertices;
		int TMeshTriIndex;
		enum POLYGON_MODE {ADD, SUBTRACT} Mode;
	};

	// used to denote intersections between triangles, for building polygons
	struct PolyEdge {

		enum POLYEDGE_FLAGS {
			A_OK =	0x0001,
			B_OK =	0x0002,
		};
		
		PolyEdge(const FVector& _A, const FVector& _B) :
			A{_A}, B{_B}, Flags{0xffff}
		{}

		bool GetAOk() const {
			return Flags & A_OK;
		}

		bool GetBOk() const {
			return Flags & B_OK;
		}

		void SetAOk() {
			Flags |= A_OK;
		}

		void SetBOk() {
			Flags |= B_OK;
		}
		
		FVector A;
		FVector B;
		uint32 Flags; // only using 2 of the 4 bytes
	};
	
	// a collection of tri-on-tri intersections, per tri
	struct UnstructuredPolygon {
		TArray<PolyEdge> Edges;	
	};

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
		const TArray<TriMesh*>& OtherTMeshes,
		const FVector& GroupExtMin // group bounding box extrema min
	);

	// 
	void PopulateUnstructuredPolygons(
		TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& UPolys
	);
	
}
