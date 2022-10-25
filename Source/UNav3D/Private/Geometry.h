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
		
		Tri(const FVector& _A, const FVector& _B, const FVector& _C) :
			A(_A), B(_B), C(_C),
			Normal(FVector::CrossProduct(_B - _A, _C - _A).GetUnsafeNormal()),
			Edges{nullptr},
			Center((_A + _B + _C) * ONE_THIRD),
			SqPerimeter(FMath::Square((_A - _B).Size() + (_B - _C).Size() + (_C - _A).Size())),
			Area(GetArea(_A, _B, _C))
		{}

		~Tri() {
			for (int i = 0; i < 3; i++) {
				if (Edges[i] != nullptr) {
					Tri** EdgesOfEdge = Edges[i]->Edges;
					for (int j = 0; j < 3; j++) {
						if (EdgesOfEdge[j] == this) {
							EdgesOfEdge[j] = nullptr;
							break;
						}
					}
				}
			}		
		}

		static float GetArea(const FVector& A, const FVector& B, const FVector& C) {
			return FVector::CrossProduct(A - B, A - C).Size() * 0.5f;
		}
		
		static float GetArea(const Tri& T) {
			return FVector::CrossProduct(T.A - T.B, T.A - T.C).Size() * 0.5f;
		}

		const FVector& A;
		const FVector& B;
		const FVector& C;
		const FVector Normal;
		Tri* Edges[3]; // adjacent tris touching AB, BC, and CA in that order
		// for faster intersection checking
		const FVector Center;
		const float SqPerimeter;
		const float Area;
	};

	// For storing world bounding box vertices of UBoxComponents and AStaticMeshActors' Meshes.
	// When these overlap, we consider the possibility that the static meshes inside the boxes overlap.
	// This implementation of a bounding box includes rotations, which makes it a little more computationally
	// expensive to check if a point lies inside or if a line intersects a face. However, this saves processor time
	// in the long run since *far* more work is done checking where meshes overlap, if they potentially overlap. And,
	// bounding boxes that rotate with meshes are much smaller, thus giving less false positives.
	struct BoundingBox {
		FVector Vertices[RECT_PRISM_PTS];
		// -- for overlap checking --
		FVector FaceNormals[RECT_PRISM_FACES];
		FVector OverlapCheckVectors[3];
		float OverlapCheckSqMags[3];
	};
	
	// Mesh made of Tris, vertices populated from GPU vertex buffer. Currently only supports nonmoving actors, but with
	// the MeshActor reference, movement could be supported in the future; BoundingBox is used for initial tests -
	// if two BB's overlap, the TriMeshes *might* overlap.
	struct TriMesh {
		
		TriMesh() :
			MeshActor(nullptr),
			Box(),
			VertexCt(0),
			Vertices(nullptr)
		{}
	
		TriMesh(const TriMesh& OtherMesh) {
			memcpy(this, &OtherMesh, sizeof(TriMesh));
		}
		
		~TriMesh() {
			if (Vertices != nullptr) {
				delete Vertices;
				Vertices = nullptr;
			}
		}

		// checking of the mesh actors are the same provides a tiny bit of debugging value
		// must be changed if UNav3D changes s.t. we expect more than one TriMesh per mesh actor
		bool operator == (const TriMesh& OtherMesh) const {
			return this->MeshActor == OtherMesh.MeshActor;	
		}

		void ResetVertexData() {
			VertexCt = 0;
			if (Vertices != nullptr) {
				delete Vertices;
				Vertices = nullptr;
			}
			if (Tris.Num() > 0) {
				Tris.Empty();
			}
		}

		AStaticMeshActor* MeshActor;
		BoundingBox Box;
		int VertexCt;
		// It might be a good idea to make this a TUniquePtr, but MoveTemp() doesn't work inside
		// TriMesh(const TriMesh&), which is necessary for TArray
		FVector* Vertices;
		TArray<Tri> Tris;
	};

	// edges are between adjacent vertices; index Num() - 1 connects to 0
	struct Polygon {
		TArray<FVector> Vertices;
	};

	// Populates a BoundingBox from a AStaticMeshActor
	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor);

	// Populates a BoundingBox from a UBoxComponent
	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp);

	// Checks whether or not the two bounding boxes overlap. Doing our own overlap checking in editor appears to be
	// necessary, as UBoxComponents will not report overlaps with AStaticMeshActor until play starts
	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB);

	// Checks whether two meshes overlap. May give false negatives due to imprecision of line trace checking; true if
	// any overlaps
	bool GetTriMeshIntersectGroups (
		TArray<int>& OverlapIndices,
		const TArray<int>& PotentialOverlapIndices,
		const TriMesh& TMesh,
		const TArray<TriMesh>& Meshes
	);
	
}
