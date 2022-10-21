#pragma once

struct Tri {
	Tri(const FVector& _A, const FVector& _B, const FVector& _C) :
		A(_A), B(_B), C(_C)
	{}
	const FVector& A;
	const FVector& B;
	const FVector& C;
	TArray<Tri*> Edges;
};

/*
 * Edges are between adjacent vertices and Num() - 1 connects to 0
 */
struct Polygon {
	TArray<FVector> Points;
};

/*
 * Mesh consisting of vertices and edges as pulled from the GPU 
 */
struct TriMesh {
	int VertexCt;
	FVector* Vertices;
	TArray<Tri> Tris;
};
