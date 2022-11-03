#pragma once

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