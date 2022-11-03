#include "UNavMesh.h"

UNavMesh::UNavMesh() :
	Box(),
	VertexCt(0),
	Vertices(nullptr)
{}

UNavMesh::UNavMesh(const UNavMesh& OtherMesh) {
	memcpy(this, &OtherMesh, sizeof(UNavMesh));
}

UNavMesh::~UNavMesh() {
	ResetVertexData();
}

bool UNavMesh::operator == (const UNavMesh& OtherMesh) const {
	return this == &OtherMesh;	
}

void UNavMesh::ResetVertexData() {
	VertexCt = 0;
	if (Vertices != nullptr) {
		delete Vertices;
		Vertices = nullptr;
	}
	Grid.Reset();
}
