#pragma once
#include "TriGrid.h"

// TODO: consider creating a vertex buffer and tri buffer, rather than individual allocations per TriMesh

struct Tri;

// For storing world bounding box vertices of UBoxComponents and AStaticMeshActors' Meshes.
// When these overlap, we consider the possibility that the static meshes inside the boxes overlap.
struct BoundingBox {
	
	static constexpr int VERTEX_CT = 8;
	static constexpr int FACE_CT = 6;
	static constexpr int EDGE_CT = 12;
	
	FVector Vertices[VERTEX_CT];
	FVector FaceNormals[FACE_CT];
	FVector OverlapCheckVectors[3];
	float OverlapCheckSqMags[3];
	float DiagDistance; // distance diagonally from vertex 0 to vertex 7
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
	TriGrid Grid;
};
