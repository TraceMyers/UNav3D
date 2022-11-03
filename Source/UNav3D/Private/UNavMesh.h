#pragma once

#include "BoundingBox.h"
#include "TriGrid.h"

struct UNavMesh {
	UNavMesh();
	UNavMesh(const UNavMesh& OtherMesh);
	~UNavMesh();

	// checks if the TMeshes point to the same static mesh
	bool operator == (const UNavMesh& OtherMesh) const;

	// Clears Vertices and Tris, since they are inextricably linked
	void ResetVertexData();

	TArray<AStaticMeshActor*> MeshActors;
	TArray<BoundingBox> ActorBoxes;
	BoundingBox Box;
	int VertexCt;
	FVector* Vertices;
	TriGrid Grid;
};
