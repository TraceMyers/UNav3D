#pragma once

#include "UNavMesh.h"
#include "TriMesh.h"
#include "Polygon.h"
#include "UNav3DBoundsVolume.h"
#include "VertexCapture.h"

namespace Data {

	TArray<UNavMesh> NMeshes;
	TArray<TriMesh> TMeshes;
	TArray<Tri*> FailureCaseTris;
	TArray<Polygon> FailureCasePolygons;
	TArray<Tri*> CulledTris;
	AUNav3DBoundsVolume* BoundsVolume;
	TriMesh BoundsVolumeTMesh;
	TArray<AVertexCapture*> VertexCaptures;
	
	inline void Reset() {
		for (int i = 0; i < Data::TMeshes.Num(); i++) {
			TMeshes[i].ResetVertexData();
		}
		for (int i = 0; i < Data::NMeshes.Num(); i++) {
			NMeshes[i].ResetVertexData();
		}
		TMeshes.Empty();
		NMeshes.Empty();
		FailureCaseTris.Empty();
		FailureCasePolygons.Empty();
		CulledTris.Empty();
		BoundsVolume = nullptr;
		BoundsVolumeTMesh.ResetVertexData();
		VertexCaptures.Empty();
	}

}