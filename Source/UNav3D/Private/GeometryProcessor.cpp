#include "GeometryProcessor.h"
#include "Geometry.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RenderingThread.h"
#include "Engine/StaticMeshActor.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"
#include "Debug.h"

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
	UNavDbg::PrintTriMeshIntersectGroups(IntersectGroups);
	TArray<TArray<TArray<Polygon>>> Polygons;
	BuildPolygonsAtMeshIntersections(World, IntersectGroups, Polygons);

	UNavDbg::DrawAllPolygons(World, Polygons);
	
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

// this one's a bit long
void GeometryProcessor::BuildPolygonsAtMeshIntersections(
	const UWorld* World,
	TArray<TArray<TriMesh*>>& Groups,
	TArray<TArray<TArray<Polygon>>>& AllPolygons
) {

	for (int i = 0; i < Groups.Num(); i++) {
		TArray<TriMesh*>& Group = Groups[i];
		TArray<TArray<UnstructuredPolygon>> UPolys; // one upoly per tri

		FVector GroupBBoxMin;
		FVector GroupBBoxMax;
		Geometry::GetGroupExtrema(Group, GroupBBoxMin, GroupBBoxMax, true);
		const float BBoxDiagDist = FVector::Dist(GroupBBoxMin, GroupBBoxMax);
		Geometry::PopulateUnstructuredPolygons(World, Group, UPolys, BBoxDiagDist);
		
		// should be integrated to PopulateUnstructuredPolygons to help avoid cache misses OR put in inner loop below
		// per tri after check num == 3 (changed to num == 0) to reduce calculations
		if (i == 1) {
			printf("hello\n");
		}
		Geometry::PopulatePolyEdgesFromTriEdges(World, Group, UPolys, BBoxDiagDist);

		AllPolygons.Add(TArray<TArray<Polygon>>());
		TArray<TArray<Polygon>>& GroupPolygons = AllPolygons.Last();
		
		for (int j = 0; j < UPolys.Num(); j++) {
			TArray<UnstructuredPolygon>& MeshUPolys = UPolys[j];
			TriMesh& TMesh = *Group[j];

			GroupPolygons.Add(TArray<Polygon>());
			TArray<Polygon>& TMeshPolygons = GroupPolygons.Last();
			
			for (int k = 0; k < MeshUPolys.Num(); k++) {
				const UnstructuredPolygon& UPoly = MeshUPolys[k];
				Tri& T = TMesh.Tris[k];
				
				if (UPoly.Edges.Num() == 3) {
					// if there are no intersections...
					if (T.AllObscured()) {
						// ...and all vertices are obscured, tri is enclosed -> cull
						T.MarkForCull();
					}
					else if (T.AnyObscured()) {
						// ... and 0 < n < 3 of the vertices are obscured, something has gone wrong
						T.MarkProblemCase();
					}	
					continue;
				}

				TArray<PolyNode> PolygonNodes;
				PopulateNodes(T, UPoly, PolygonNodes);
				
				// each tri might come out with more than one polygon; for example, a set of intersections in the center
				// of the tri that do not touch tri edges - one outside, one inside; but, we start with one
				// add if ever touches edge, else subtract unless enclosed by subtract polygon
				const int NodeCt = PolygonNodes.Num();
				if (NodeCt == 0) {
					T.MarkForCull();
					continue;
				}

				for (int m = 0; m < NodeCt; m++) {
					if (PolygonNodes[m].Edges.Num() % 2 == 1) {
						T.MarkForCull();
						goto BUILDPOLY_NEXT;
					}
				}
				
				for (int StartIndex = 0; StartIndex < NodeCt; ) {
					PolyNode& StartNode = PolygonNodes[StartIndex];
					TArray<int>* EdgeIndices = &StartNode.Edges;
					// check to make sure the node isn't exhausted
					if (EdgeIndices->Num() == 0) {
						StartIndex++;
						continue;
					}
					
					Polygon BuildingPolygon(k);
					BuildingPolygon.Vertices.Add(StartNode.Location);
					int PrevIndex = StartIndex;
					while (true) {
						const int EdgeIndex = (*EdgeIndices)[0];
						if (EdgeIndex == StartIndex) {
							EdgeIndices->RemoveAt(0, 1, false);
							const int SNIndexOfEdge = StartNode.Edges.Find(PrevIndex);
							if (SNIndexOfEdge != -1) {
								StartNode.Edges.RemoveAt(SNIndexOfEdge, 1, false);
							}
							else {
								printf("polygon build error: can't find start index at end\n");
							}
							TMeshPolygons.Add(BuildingPolygon);
							break;
						}
						// connect to another node
						PolyNode& EdgeNode = PolygonNodes[EdgeIndex];
						BuildingPolygon.Vertices.Add(EdgeNode.Location);

						// remove the connection both ways
						EdgeIndices->RemoveAt(0, 1, false);
						if (EdgeNode.Edges.Num() <= 1 || EdgeNode.Edges.Find(PrevIndex) == -1) {
							printf("polygon build error: num() <= 1 or can't find previndex\n");
							T.MarkForCull();
							goto BUILDPOLY_NEXT;
						}
						EdgeNode.Edges.RemoveSingle(PrevIndex);

						// move on to next node
						EdgeIndices = &EdgeNode.Edges;
						PrevIndex = EdgeIndex;
					}
				}
				T.MarkForPolygon();
				BUILDPOLY_NEXT: ;
			}
		}
	}
}

void GeometryProcessor::PopulateNodes(const Tri& T, const UnstructuredPolygon& UPoly, TArray<PolyNode>& PolygonNodes) {
	const TArray<PolyEdge>& Edges = UPoly.Edges;
	for (int i = 0; i < Edges.Num(); i++) {
		const PolyEdge& PEdge = Edges[i];
		const TArray<float> PtDistances = PEdge.TrDropDistances;
		if (PtDistances.Num() == 0) {
			// A is not enclosed here, so is B, so create 2 nodes. else, none
			if (!PEdge.IsAEnclosed()) {
				AddPolyNodes(PolygonNodes, PEdge.A, PEdge.B);
			}
			continue;
		}

		// TODO: probably better to save the position than the distance
		FVector StartLoc;
		int start_j;
		const FVector Dir = (PEdge.B - PEdge.A).GetUnsafeNormal();
		const int PtDistCt = PtDistances.Num();
		
		if (PEdge.IsAEnclosed()) {
			StartLoc = PEdge.A + Dir * PtDistances[0];
			if (PtDistCt == 1) {
				AddPolyNodes(PolygonNodes, StartLoc, PEdge.B);
				continue;	
			}
			start_j = 1;
		}
		else {
			StartLoc = PEdge.A;
			start_j = 0;
		}

		for (int j = start_j; ; ) {
			FVector EndLoc = PEdge.A + Dir * PtDistances[j];
			AddPolyNodes(PolygonNodes, StartLoc, EndLoc);
			if (++j == PtDistCt) {
				break;		
			}
			StartLoc = PEdge.A + Dir * PtDistances[j];
			if (++j == PtDistCt) {
				AddPolyNodes(PolygonNodes, StartLoc, PEdge.B);
				break;
			}
		}
	}
}

void GeometryProcessor::AddPolyNodes(TArray<PolyNode>& Nodes, const FVector& A, const FVector& B) {
	constexpr float EPSILON = 1e-2f;
	int i0 = -1;
	int i1 = -1;
	for (int i = 0; i < Nodes.Num(); i++) {
		PolyNode& PNode = Nodes[i];
		if (i0 == -1 && FVector::Dist(A, PNode.Location) < EPSILON) {
			i0 = i;
			if (i1 != -1) {
				goto ADDPOLY_LINK;
			}
		}
		else if (i1 == -1 && FVector::Dist(B, PNode.Location) < EPSILON) {
			i1 = i;
			if (i0 != -1) {
				goto ADDPOLY_LINK;
			}
		}
	}
	if (i0 == -1) {
		i0 = Nodes.Add(PolyNode(A));
	}
	if (i1 == -1) {
		i1 = Nodes.Add(PolyNode(B));
	}
	ADDPOLY_LINK:
	Nodes[i0].Edges.Add(i1);
	Nodes[i1].Edges.Add(i0);
}
