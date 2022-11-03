#pragma once

#include "TriGrid.h"
#include "BoundingBox.h"

struct Tri;

// Mesh made of Tris, vertices populated from GPU vertex buffer. Used temporarily before UNavMesh creation
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
