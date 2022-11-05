#pragma once
#include "UNavMesh.h"
#include "TriMesh.h"
#include "Polygon.h"

namespace Data {
	TArray<UNavMesh> NMeshes;
	TArray<TriMesh> TMeshes;
	TArray<Tri> FailureCaseTris;
	TArray<Polygon> FailureCasePolygons;
	TArray<Tri> CulledTris;
}