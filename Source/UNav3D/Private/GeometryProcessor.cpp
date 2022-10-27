#include "GeometryProcessor.h"
#include "Geometry.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RenderingThread.h"
#include "Engine/StaticMeshActor.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"

// TODO: currently just using LOD0, and it would be nice to parameterize this, but I wouldn't do it until...
// TODO: ... there is a good system in place to take that input from the user

GeometryProcessor::GeometryProcessor() {}

GeometryProcessor::~GeometryProcessor() {}

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::PopulateTriMesh(TriMesh& TMesh, bool DoTransform) const {
	// forcing CPU access to the mesh seems like the most user-friendly option
	UStaticMesh* StaticMesh = TMesh.MeshActor->GetStaticMeshComponent()->GetStaticMesh();
	StaticMesh->bAllowCPUAccess = true;
	
	// just using LOD0 for now
	const FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
	
	TMesh.ResetVertexData();
	uint32 IndexCt;
	uint32 VertexCt;

	// get indices and vertices from gpu buffers
	uint16* Indices = GetIndices(LOD, IndexCt);
	if (Indices == nullptr) {
		return GEOPROC_ALLOC_FAIL;
	}
	GEOPROC_RESPONSE Response = GetVertices(LOD, TMesh, VertexCt);
	
	if (Response == GEOPROC_SUCCESS) {
		// optionally use passed-in transform
		if (DoTransform) {
			FVector* Vertices = TMesh.Vertices;
			const FTransform TForm = TMesh.MeshActor->GetTransform();
			for (uint32 i = 0; i < VertexCt; i++) {
				Vertices[i] = TForm.TransformPosition(Vertices[i]);
			}
		}
		// populate mesh; fails if any indices outside of reasonable bounds
		Response = Populate(TMesh, Indices, IndexCt, VertexCt);
	}

	if (Response != GEOPROC_SUCCESS) {
		TMesh.ResetVertexData();
	}
	delete Indices;
	return Response;
}

// Does not group Mesh A and Mesh B if Mesh A is entirely inside MeshB, unless Mesh C intersects both
GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::ReformTriMeshes(
	const UWorld* World,
	TArray<TriMesh>& InMeshes,
	TArray<TriMesh>& OutMeshes
) {
	OutMeshes.Reserve(InMeshes.Num());
	TArray<TArray<TriMesh*>> IntersectGroups;
	GetIntersectGroups(IntersectGroups, InMeshes);

	for (int i = 0; i < IntersectGroups.Num(); i++) {
		printf("group {%d}:\n", i + 1);
		auto& IntersectGroup = IntersectGroups[i];
		for (int j = 0; j < IntersectGroup.Num(); j++) {
			const auto TMesh = IntersectGroup[j];
			printf("TMesh: %s\n", TCHAR_TO_ANSI(*TMesh->MeshActor->GetName()));
		}
		putchar('\n');
	}

	FlagObscuredTris(World, IntersectGroups);
	
	// InMeshes.Empty();
	return GEOPROC_SUCCESS;
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
	TriMesh& TMesh,
	uint32& VertexCt
) const {
	VertexCt = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	TMesh.Vertices = new FVector[VertexCt];
	if (TMesh.Vertices == nullptr) {
		return GEOPROC_ALLOC_FAIL;
	}
	FVector* Vertices = TMesh.Vertices;
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

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::Populate(
	TriMesh& TMesh,
	uint16* Indices,
	uint32 IndexCt,
	uint32 VertexCt
) {
	for (uint32 i = 0; i < IndexCt; i++) {
		const uint16& Index = Indices[i];
		if (Index > VertexCt) {
			return GEOPROC_HIGH_INDEX;	
		}
	}

	// Vertices[Indices[0]] = Tri0.A, Vertices[Indices[1]] = Tri0.B, Vertices[Indices[2]] = Tri0.C,
	// Vertices[Indices[3]] = Tri1.A ... and so on
	const uint32 VEnd = IndexCt - 3;
	const FVector* Vertices = TMesh.Vertices;
	for (uint16* IndexPtr = Indices; (IndexPtr - Indices) <= VEnd; ) {
		const FVector& A = Vertices[*IndexPtr++];
		const FVector& B = Vertices[*IndexPtr++];
		const FVector& C = Vertices[*IndexPtr++];
		Tri T(A, B, C);
		TMesh.Tris.Add(T);
	}
	TMesh.VertexCt = VertexCt;
	
	return GEOPROC_SUCCESS;
}

void GeometryProcessor::GetIntersectGroups(
	TArray<TArray<TriMesh*>>& Groups,
	TArray<TriMesh>& InMeshes
) {
	const int InMeshCt = InMeshes.Num();
	TArray<int> GroupIndices;
	GroupIndices.Init(-1, InMeshCt);
	
	TArray<int> PotentialIntersectIndices;
	TArray<int> IntersectIndices;
	for (int i = 0; i < InMeshCt - 1; i++) {
		TriMesh& TMeshA = InMeshes[i];
		
		// if the TMesh already belongs to a group, get the group, otherwise make a group
		int GrpIndex = GroupIndices[i];
		if (GrpIndex == -1) {
			GrpIndex = Groups.Add(TArray<TriMesh*>());
			GroupIndices[i] = GrpIndex;
		}
		
		// check forward for bbox overlaps
		for (int j = i + 1; j < InMeshCt; j++) {
			const int& OtherGrpIndex = GroupIndices[j];
			// if TMeshA and TMeshB are already grouped, no need to check overlap
			if (OtherGrpIndex == GrpIndex) {
				continue;
			}
			TriMesh& TMeshB = InMeshes[j];
			if (Geometry::DoBoundingBoxesOverlap(TMeshA.Box, TMeshB.Box)) {
				PotentialIntersectIndices.Add(j);
			}
		}

		// populate group indices array with indices of mesh intersections
		if(Geometry::GetTriMeshIntersectGroups(IntersectIndices, PotentialIntersectIndices, TMeshA, InMeshes)) {
			for (int j = 0; j < IntersectIndices.Num(); j++) {
				const int& IndexOfOverlap = IntersectIndices[j];
				const int OverlapGrpIndex = GroupIndices[IndexOfOverlap];
				// merge groups if overlapping TMesh belongs to a different group (already checked if in same group)
				if (OverlapGrpIndex == -1) {
					GroupIndices[IndexOfOverlap] = GrpIndex;
				}
				else {
					for (int m = 0; m < InMeshCt; m++) {
						if (GroupIndices[m] == OverlapGrpIndex) {
							GroupIndices[m] = GrpIndex;
						}	
					}
				}
			}	
		}

		PotentialIntersectIndices.Empty(InMeshCt);
		IntersectIndices.Empty(InMeshCt);
	}
	// the very last mesh may be an island (no overlaps)
	const int EndIndex = InMeshCt - 1;
	if (GroupIndices[EndIndex] == -1) {
		GroupIndices[EndIndex] = Groups.Add(TArray<TriMesh*>());
	}

	// populate groups with known group indices
	for (int i = 0; i < InMeshCt; i++) {
		const int& GrpIndex = GroupIndices[i];
		Groups[GrpIndex].Add(&InMeshes[i]);
	}
}

void GeometryProcessor::FlagObscuredTris(const UWorld* World, TArray<TArray<TriMesh*>>& Groups) {
	for (int i = 0; i < Groups.Num(); i++) {
		TArray<TriMesh*>& Group = Groups[i];
		FVector GroupBBoxMin;
		FVector GroupBBoxMax;
		Geometry::GetGroupExtrema(Group, GroupBBoxMin, GroupBBoxMax, true);
		for (int j = 0; j < Group.Num(); j++) {
			TriMesh* CurTMesh = Group[j];
			TArray<TriMesh*> OtherTMeshes = Group;
			OtherTMeshes.Remove(CurTMesh);
			Geometry::FlagObscuredTris(World, *CurTMesh, OtherTMeshes, GroupBBoxMin);
		}
	}
}

void GeometryProcessor::BuildPolygonsAtMeshIntersections(TArray<TArray<TriMesh*>>& Groups) {

	for (int i = 0; i < Groups.Num(); i++) {
		
		TArray<TriMesh*>& Group = Groups[i];
		TArray<TArray<UnstructuredPolygon>> UPolys; // one poly per tri

		Geometry::PopulateUnstructuredPolygons(Group, UPolys);
		
		for (int j = 0; j < UPolys.Num(); j++) {
			TArray<UnstructuredPolygon>& MeshUPolys = UPolys[j];
			TriMesh& TMesh = *Group[j];
			TArray<Polygon> TMeshPolygons;
			
			for (int k = 0; k < MeshUPolys.Num(); k++) {
				const UnstructuredPolygon& UPoly = MeshUPolys[k];
				Tri& T = TMesh.Tris[k];
				
				if (UPoly.Edges.Num() == 0) {
					// if there are no intersections...
					if (T.AllObscured()) {
						// ...and all vertices are obscured, tri is enclosed -> cull
						T.MarkForCull();
					}
					else if (T.AnyObscured()) {
						// ... 0 < n < 3 of the vertices are obscured, something has gone wrong
						T.MarkProblemCase();
					}	
					continue;	
				}

				// each tri might come out with more than one polygon; for example, a set of intersections in the center
				// of the tri that do not touch tri edges - one outside, one inside
				TArray<Polygon> BuildingPolygons;
				BuildingPolygons.Add(Polygon(k));
				
			}
		}
	}
}
