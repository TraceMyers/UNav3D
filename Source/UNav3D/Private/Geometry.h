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
 * A Vertex is a point with edges
 */
struct Vertex {
	Vertex(const FVector& _Point) :
		Point(_Point)
	{}
	const FVector& Point;
	TArray<FVector> Edges;
};

/*
 * Mesh consisting of vertices and edges as pulled from the GPU 
 * Useful when only the vertices are needed, since reconstructing tris from the vertices takes time.
 */
struct VertexMesh {
	TArray<Vertex> Vertices;	
};

/*
 * Mesh consisting of vertices and edges as pulled from the GPU 
 * Slower to build than VertexMesh
 */
struct TriMesh {
	TArray<FVector> Points;
	TArray<Tri> Tris;
};
