#include "GeometryProcessor.h"
#include "Geometry.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RenderingThread.h"
#include "DebugSwitches.h"
#include "Engine/StaticMeshActor.h"

GeometryProcessor::GeometryProcessor() {}

GeometryProcessor::~GeometryProcessor() {}

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::PopulateTriMesh(Geometry::TriMesh& TMesh, bool DoTransform) const {
	// forcing CPU access to the mesh seems like the most user-friendly option
	UStaticMesh* StaticMesh = TMesh.MeshActor->GetStaticMeshComponent()->GetStaticMesh();
	StaticMesh->bAllowCPUAccess = true;
	
	// TODO: currently just using LOD0, and it would be nice to parameterize this, but I wouldn't do it until
	// there is a good system in place to take that input from the user
	const FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
	
	TMesh.VertexCt = 0;
	if (TMesh.Vertices.IsValid()) {
		TMesh.Vertices.Reset();
	}
	uint32 IndexCt;
	uint32 VertexCt;
	
	uint16* Indices = GetIndices(LOD, IndexCt);
	if (Indices == nullptr) {
		return GEOPROC_ALLOC_FAIL;
	}
	
	GEOPROC_RESPONSE Response = GetVertices(LOD, TMesh, VertexCt);
	if (Response == GEOPROC_SUCCESS) {
		// optionally use passed-in transform
		if (DoTransform) {
			FVector* Vertices = TMesh.Vertices.Get();
			FTransform TForm = TMesh.MeshActor->GetTransform();
			for (uint32 i = 0; i < VertexCt; i++) {
				Vertices[i] = TForm.TransformPosition(Vertices[i]);
			}
		}
		// populate mesh; fails if indices outside of reasonable bounds
		Response = PopulateTriMesh(TMesh, Indices, IndexCt, VertexCt);
		if (Response == GEOPROC_SUCCESS) {
			return Response;
		}
	}
	
	if (TMesh.Vertices.IsValid()) {
		TMesh.Vertices.Reset();
	}
	delete Indices;
	return Response;
}

uint16* GeometryProcessor::GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const {
	IndexCt = LOD.IndexBuffer.IndexBufferRHI->GetSize() / sizeof(uint16);
	uint16* TriIndices = new uint16[IndexCt];
	if (TriIndices == nullptr) {
		return nullptr;
	}
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

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::GetVertices(
	const FStaticMeshLODResources& LOD,
	Geometry::TriMesh& TMesh,
	uint32& VertexCt
) const {
	VertexCt = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	TMesh.Vertices = MakeUnique<FVector[]>(VertexCt);
	if (!TMesh.Vertices.IsValid()) {
		return GEOPROC_ALLOC_FAIL;
	}
	FVector* Vertices = TMesh.Vertices.Get();
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

	return GEOPROC_SUCCESS;
}

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::PopulateTriMesh(
	Geometry::TriMesh& TMesh,
	uint16* Indices,
	uint32 IndexCt,
	uint32 VertexCt
) {
	uint16 IndexMax = 0;
	for (uint32 i = 0; i < IndexCt; i++) {
		const uint16& Index = Indices[i];
		if (Index > IndexMax) {
			IndexMax = Index;
		}
	}
	if (IndexMax > VertexCt) {
		return GEOPROC_ERR_BAD_INDEX_MAX;	
	}

	// Vertices[Indices[0]] = Tri0.A, Vertices[Indices[1]] = Tri0.B, Vertices[Indices[2]] = Tri0.C,
	// Vertices[Indices[3]] = Tri1.A ... and so on
	const uint32 VEnd = VertexCt - 3;
	const FVector* Vertices = TMesh.Vertices.Get();
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
		Geometry::Tri T(A, B, C);
		TMesh.Tris.Add(T);
	}
	TMesh.VertexCt = VertexCt;
	
	return GEOPROC_SUCCESS;
}
