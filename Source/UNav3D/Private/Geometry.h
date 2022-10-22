#pragma once

namespace Geometry {

	// Triangle; saves space by storing references to vertices
	struct Tri {
		Tri(const FVector& _A, const FVector& _B, const FVector& _C) :
			A(_A), B(_B), C(_C), Edges{nullptr}
		{}
		const FVector& A;
		const FVector& B;
		const FVector& C;
		Tri* Edges[3]; // adjacent tris touching AB, BC, and CA in that order
	};

	// edges are between adjacent vertices and Num() - 1 connects to 0
	struct Polygon {
		TArray<FVector> Points;
	};

	// mesh consisting of vertices and edges as pulled from the GPU 
	struct TriMesh {
		int VertexCt;
		TUniquePtr<FVector[]> Vertices;
		TArray<Tri> Tris;
	};

	static FVector& GetZMinimum(const TriMesh& TMesh);
}