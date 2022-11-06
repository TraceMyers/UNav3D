#pragma once
#include "UNavMesh.h"
#include "TriMesh.h"
#include "Polygon.h"
#include "UNav3DBoundsVolume.h"

namespace Data {
	TArray<UNavMesh> NMeshes;
	TArray<TriMesh> TMeshes;
	TArray<Tri> FailureCaseTris;
	TArray<Polygon> FailureCasePolygons;
	TArray<Tri> CulledTris;
	AUNav3DBoundsVolume* BoundsVolume;
	TriMesh BoundsVolumeTMesh;
}