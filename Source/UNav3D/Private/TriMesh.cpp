#include "TriMesh.h"

TriMesh::TriMesh() :
	Box(),
	VertexCt(0),
	Vertices(nullptr),
	MeshActor(nullptr)
{}

TriMesh::TriMesh(const TriMesh& OtherMesh) {
	memcpy(this, &OtherMesh, sizeof(TriMesh));
}

TriMesh::~TriMesh() {
	ResetVertexData();
}

// checking of the mesh actors are the same provides a tiny bit of debugging value
bool TriMesh::operator == (const TriMesh& OtherMesh) const {
	return this->MeshActor == OtherMesh.MeshActor;	
}

void TriMesh::ResetVertexData() {
	VertexCt = 0;
	if (Vertices != nullptr) {
		delete Vertices;
		Vertices = nullptr;
	}
	Grid.Reset();
}