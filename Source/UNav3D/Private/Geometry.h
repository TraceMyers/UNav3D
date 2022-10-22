#pragma once

class AStaticMeshActor;
class UBoxComponent;

// TODO: consider creating a vertex buffer and tri buffer, grouping them together in memory

namespace Geometry {

	static constexpr int RECT_PRISM_PTS = 8;
	
	// Triangle; saves space by storing references to vertices on the TriMesh; stores references to adjacent Tris
	struct Tri {
		
		Tri(const FVector& _A, const FVector& _B, const FVector& _C) :
			A(_A), B(_B), C(_C), Edges{nullptr}
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
		
		const FVector& A;
		const FVector& B;
		const FVector& C;
		Tri* Edges[3]; // adjacent tris touching AB, BC, and CA in that order
	};

	// for storing world bounding box vertices of UBoxComponents and AStaticMeshActors' Meshes
	struct BoundingBox {
		FVector Vertices[RECT_PRISM_PTS];
		// precomputed data using Vertices
		FVector OverlapCheckVectors[3];
		float OverlapCheckSqMags[3];
	};
	
	// Mesh made of Tris, vertices populated from GPU vertex buffer. currently static but with the MeshActor
	// reference, movement could be supported in the future; BoundingBox is used for initial tests - if two BB's overlap,
	// the TriMeshes *might* overlap.
	struct TriMesh {
		
		TriMesh() :
			MeshActor(nullptr),
			Box(),
			VertexCt(0)
		{}

		~TriMesh() {
			if (Vertices.IsValid()) {
				Vertices.Release();
			}
		}
		
		AStaticMeshActor* MeshActor;
		BoundingBox Box;
		int VertexCt;
		TUniquePtr<FVector[]> Vertices;
		TArray<Tri> Tris;
	};

	// edges are between adjacent vertices and Num() - 1 connects to 0
	struct Polygon {
		TArray<FVector> Points;
	};

	// Sets a BoundingBox from a AStaticMeshActor
	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor, bool DoTransform=true);

	// Sets a BoundingBox from a UBoxComponent
	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp, bool DoTransform=true);

	// Checks whether or not the two bounding boxes overlap. Doing our own overlap checking in editor appears to be
	// necessary, as UBoxComponents will not report overlaps with AStaticMeshActor until play starts
	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB);
	
}
