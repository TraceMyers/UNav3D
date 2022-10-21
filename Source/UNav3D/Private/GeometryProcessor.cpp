#include "GeometryProcessor.h"
#include "Geometry.h"

GeometryProcessor::GeometryProcessor() {}

GeometryProcessor::~GeometryProcessor() {}

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::CreateTriMesh(
	TriMesh& TMesh,
	UStaticMesh* StaticMesh,
	const FTransform* TForm
) const {
	StaticMesh->bAllowCPUAccess = true;
	const FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
	
	TMesh.VertexCt = 0;
	TMesh.Vertices = nullptr;

	// Getting Static Mesh Vertices and Indices
	// Vertices[Indices[0]] = Tri0.A, Vertices[Indices[1]] = Tri0.B, Vertices[Indices[2]] = Tri0.C,
	// Vertices[Indices[3]] = Tri1.A ... and so on
	uint32 IndexCt;
	uint32 VertexCt;
	uint16* Indices = GetIndices(LOD, IndexCt);
	FVector* Vertices = GetVertices(LOD, VertexCt);

	if (TForm != nullptr) {
		for (int i = 0; i < VertexCt; i++) {
			Vertices[i] = TForm->TransformPosition(Vertices[i]);
		}
	}

	const GEOPROC_RESPONSE Response = PopulateTriMesh(TMesh, Indices, Vertices, IndexCt, VertexCt);
	if (Response != GEOPROC_SUCCESS) {
		delete Vertices;
	}

	delete Indices;
	return Response;
}

uint16* GeometryProcessor::GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const {
	IndexCt = LOD.IndexBuffer.IndexBufferRHI->GetSize() / sizeof(uint16);
	uint16* TriIndices = new uint16[IndexCt];
	const FRawStaticIndexBuffer* IndBuf = &LOD.IndexBuffer;

	// Asking the GPU to kindly relinquish the goods
	ENQUEUE_RENDER_COMMAND(GetIndexBuffer) (
		[TriIndices, IndBuf] (FRHICommandList& RHICmd) {
			const uint16* Indices = (uint16*)RHILockIndexBuffer(
				IndBuf->IndexBufferRHI,
				0,
				IndBuf->IndexBufferRHI->GetSize(),
				RLM_ReadOnly
			);
			memcpy(TriIndices, Indices, IndBuf->IndexBufferRHI->GetSize());
			RHIUnlockIndexBuffer(IndBuf->IndexBufferRHI);
		}
	);
	FlushRenderingCommands();
	
	return TriIndices;
}

FVector* GeometryProcessor::GetVertices(const FStaticMeshLODResources& LOD, uint32& VertexCt) const {
	VertexCt = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	FVector* Vertices = new FVector[VertexCt];
	const FPositionVertexBuffer* PosBuf = &LOD.VertexBuffers.PositionVertexBuffer;
	
	// Asking the GPU to kindly relinquish the goods
	ENQUEUE_RENDER_COMMAND(GetPositionVertexBuffer) (
		[Vertices, PosBuf] (FRHICommandList& rhi_cmd) {
			const FVector* Verts = (FVector*)RHILockVertexBuffer(
				PosBuf->VertexBufferRHI,
				0,
				PosBuf->VertexBufferRHI->GetSize(),
				RLM_ReadOnly
			);
			memcpy(Vertices, Verts, PosBuf->VertexBufferRHI->GetSize());
			RHIUnlockVertexBuffer(PosBuf->VertexBufferRHI);
		}
	);
	FlushRenderingCommands();

	return Vertices;
}

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::PopulateTriMesh(
	TriMesh& TMesh,
	uint16* Indices,
	FVector* Vertices,
	uint32 IndexCt,
	uint32 VertexCt
) {
	uint16 IndexMax = 0;
	for (int i = 0; i < IndexCt; i++) {
		const uint16& Index = Indices[i];
		if (Index > IndexMax) {
			IndexMax = Index;
		}
	}
	if (IndexMax > VertexCt) {
		return GEOPROC_ERR_BAD_INDEX_MAX;	
	}
	
	const uint32 VEnd = VertexCt - 3;
	for (uint16* IndexPtr = Indices; (Indices - IndexPtr) < VEnd; ) {
		const uint16& AIndex = *IndexPtr++;
		const uint16& BIndex = *IndexPtr++;
		const uint16& CIndex = *IndexPtr++;

		if (
			AIndex < 0 || AIndex >= IndexMax
			|| BIndex < 0 || BIndex >= IndexMax
			|| CIndex < 0 || CIndex >= IndexMax
		) {
#ifdef UNAV_GEO_DBG
			return GEOPROC_ERR_BAD_INDEX;
#endif
			continue;
		}

		const FVector& A = Vertices[AIndex];
		const FVector& B = Vertices[BIndex];
		const FVector& C = Vertices[CIndex];
		Tri T(A, B, C);
		TMesh.Tris.Add(T);
	}
	TMesh.Vertices = Vertices;
	TMesh.VertexCt = VertexCt;
	
	return GEOPROC_SUCCESS;
}
