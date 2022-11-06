﻿#include "GeometryProcessor.h"
#include "Data.h"
#include "Geometry.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RenderingThread.h"
#include "Engine/StaticMeshActor.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"
#include "Debug.h"
#include "UNavMesh.h"
#include "Containers/ArrayView.h"
#include "string.h"

// TODO: currently just using LOD0, and it would be nice to parameterize this, but I wouldn't do it until...
// TODO: ... there is a good system in place to take that input from the user
// TODO: batch processing, at least per group

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
GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::PopulateNavMeshes(
	TArray<TriMesh>& InMeshes,
	TArray<UNavMesh>& OutMeshes
) {
	OutMeshes.Reserve(InMeshes.Num());
	TArray<TArray<TriMesh*>> IntersectGroups;

	GetIntersectGroups(IntersectGroups, InMeshes);
	UNavDbg::PrintTriMeshIntersectGroups(IntersectGroups);
	TArray<TArray<TArray<Polygon>>> Polygons;
	
	FlagOutsideTris(IntersectGroups);
	BuildPolygonsAtMeshIntersections(IntersectGroups, Polygons);
	FormMeshesFromGroups(IntersectGroups, Polygons, OutMeshes);
	
	return GEOPROC_SUCCESS;
}

GeometryProcessor::GEOPROC_RESPONSE GeometryProcessor::SimplifyNavMeshes(TArray<UNavMesh>& NMeshes) {
	// TODO: parameterize the values that determine the outcome here
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
	FVector* Vertices = TMesh.Vertices;
	TArray<TempTri> Tris;
	for (uint16* IndexPtr = Indices; (IndexPtr - Indices) <= VEnd; ) {
		FVector* A = &Vertices[*IndexPtr++];
		FVector* B = &Vertices[*IndexPtr++];
		FVector* C = &Vertices[*IndexPtr++];
		TempTri T(A, B, C);
		Tris.Add(T);
	}
	TMesh.VertexCt = VertexCt;
	TMesh.Grid.Init(TMesh, Tris);
	
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

void GeometryProcessor::FlagOutsideTris(TArray<TArray<TriMesh*>>& Groups) {
	for (int i = 0; i < Groups.Num(); i++) {
		TArray<TriMesh*>& Group = Groups[i];
		for (int j = 0; j < Group.Num(); j++) {
			TriMesh& TMesh = *Group[j];
			if (!Geometry::IsBoxAInBoxB(TMesh.Box, Data::BoundsVolumeTMesh.Box)) {
				Geometry::FlagTrisOutsideBox(Data::BoundsVolumeTMesh.Box, TMesh);
			}
		}
	}
}

void GeometryProcessor::BuildPolygonsAtMeshIntersections(
	TArray<TArray<TriMesh*>>& Groups,
	TArray<TArray<TArray<Polygon>>>& AllPolygons
) {
	auto& Grid = Data::BoundsVolumeTMesh.Grid;
	printf("bv tmesh num: %d\n", Grid.Num());
	for (int i = 0; i < Grid.Num(); i++) {
		Grid[i].Normal = -Grid[i].Normal;
	}
	for (int i = 0; i < Groups.Num(); i++) {
		TArray<TriMesh*>& Group = Groups[i];
		TArray<TArray<UnstructuredPolygon>> UPolys;
		AllPolygons.Add(TArray<TArray<Polygon>>());
		TArray<TArray<Polygon>>& GroupPolygons = AllPolygons.Last();

		// slipping the bounds volume tmesh into the group so it creates intersections with other meshes
		Group.Add(&Data::BoundsVolumeTMesh);
		const int GroupCt = Group.Num();
		for (int j = 0; j < GroupCt; j++) {
			UPolys.Add(TArray<UnstructuredPolygon>());
			TArray<UnstructuredPolygon>& MeshUPolys = UPolys[j];
			// creating an unstructured polygon per tri
			MeshUPolys.Init(UnstructuredPolygon(), Group[j]->Grid.Num());
		}

		// get mesh intersections between meshes, including Bounds Volume
		Geometry::GetUPolygonsFromIntersections(Group, UPolys);

		// removing the bounds volume tmesh since we don't care what intersections landed on it
		Group.RemoveAt(GroupCt - 1);
		UPolys.RemoveAt(GroupCt - 1);
		
		for (int j = 0; j < UPolys.Num(); j++) {
			TArray<UnstructuredPolygon>& MeshUPolys = UPolys[j];
			TriMesh& TMesh = *Group[j];
			GroupPolygons.Add(TArray<Polygon>());
			TArray<Polygon>& TMeshPolygons = GroupPolygons.Last();
			
			for (int k = 0; k < MeshUPolys.Num(); k++) {
				Tri& T = TMesh.Grid[k];
				if (T.IsTriCull()) {
					// Tris are marked for cull early if they fall outside of the bounds box
					continue;
				}
				const UnstructuredPolygon& UPoly = MeshUPolys[k];
				if (UPoly.Edges.Num() == 0) {
					// if there are no intersections...
					if (T.AllObscured()) {
						// ...and all vertices are obscured, tri is enclosed -> cull
						T.MarkForCull();
						Data::CulledTris.Add(T);
					}
					else if (T.AnyObscured()) {
						// ... and 0 < n < 3 of the vertices are obscured, something has gone wrong
						T.MarkProblemCase();
						T.MarkForCull();
						Data::FailureCaseTris.Add(T);
					}	
					continue;
				}

				TArray<UPolyNode> PolygonNodes;
				
				// creating graphs where edges (intersections and tri edges) connect, and where they're visible
				PopulateNodes(T, UPoly, PolygonNodes);
				
				BuildPolygonsFromTri(T, PolygonNodes, TMeshPolygons, k);
				
				// TODO: each tri might come out with more than one polygon; for example, a set of intersections in the center...
				// TODO: ... of the tri that do not touch tri edges - one outside, one inside; but, we start with one...
				// TODO: ... add if ever touches edge, else subtract unless enclosed by subtract polygon
			}
		}
	}
	for (auto& GroupPolygons : AllPolygons) {
		for (auto& MeshPolygons : GroupPolygons) {
			for (auto& Polygon : MeshPolygons) {
				LinkPolygonEdges(Polygon);
			}
		}	
	}
}

void GeometryProcessor::FormMeshesFromGroups(
	TArray<TArray<TriMesh*>>& Groups,
	TArray<TArray<TArray<Polygon>>>& Polygons,
	TArray<UNavMesh>& NavMeshes
) {
	for (int i = 0; i < Groups.Num(); i++) {
		const auto& Group = Groups[i];

		// reserving space for temp tris and verts
		int TriCt = 0;
		int VertexCt = 0;
		for (auto& TMesh : Group) {
			TriCt += TMesh->Grid.Num();
			VertexCt += TMesh->VertexCt;
		}

		// populating these data with Tris that are not changing
		TArray<FIntVector> UnchangedTriVertexIndices;
		UnchangedTriVertexIndices.Reserve(TriCt);
		TArray<FVector*> UnchangedVertices;
		UnchangedVertices.Reserve(VertexCt);
		PopulateUnmarkedTriData(Group, UnchangedVertices, UnchangedTriVertexIndices);

		// populating these data with Tris formed from new polygons
		TArray<FIntVector> NewTriVertexIndices;
		NewTriVertexIndices.Reserve((int)(TriCt * 0.2f)); // just guessing how much space to reserve here
		TArray<FVector> NewVertices;
		NewVertices.Reserve((int)(VertexCt * 0.2f));
		TArray<TArray<Polygon>>& GroupPolygons = Polygons[i];
		TArray<FVector*> Normals;
		for (int j = 0; j < GroupPolygons.Num(); j++) {
			auto& TMesh = Group[j];
			if (strcmp(TCHAR_TO_ANSI(*TMesh->MeshActor->GetName()), "DebugMarkerMesh2_2") == 0) {
				printf("hello\n");
			}
			auto& MeshPolygons = GroupPolygons[j];
			CreateNewTriData(MeshPolygons, NewVertices, NewTriVertexIndices, Normals);
		}

		// making new vertex buffer
		const int UnchangedVertexCt = UnchangedVertices.Num();
		const int NewVertexCt = NewVertices.Num();
		const int NewVertCt = UnchangedVertexCt + NewVertexCt;
		FVector* NewVertexBuffer = new FVector[NewVertCt];
		if (NewVertexBuffer == nullptr) {
			continue;
		}
		for (int j = 0; j < UnchangedVertexCt; j++) {
			NewVertexBuffer[j] = *UnchangedVertices[j];
		}
		if (NewVertexCt > 0) {
			memcpy(NewVertexBuffer + UnchangedVertexCt, NewVertices.GetData(), NewVertexCt * sizeof(FVector));
		}

		// making temp tri buffer
		TArray<TempTri> NewTris;
		for (int j = 0; j < UnchangedTriVertexIndices.Num(); j++) {
			const FIntVector& VertexIndices = UnchangedTriVertexIndices[j];
			NewTris.Add(TempTri(
				&NewVertexBuffer[VertexIndices.X],
				&NewVertexBuffer[VertexIndices.Y],
				&NewVertexBuffer[VertexIndices.Z]
			));
		}
		for (int j = 0; j < NewTriVertexIndices.Num(); j++) {
			const FIntVector& VertexIndices = NewTriVertexIndices[j];
			NewTris.Add(TempTri(
				&NewVertexBuffer[VertexIndices.X + UnchangedVertexCt],
				&NewVertexBuffer[VertexIndices.Y + UnchangedVertexCt],
				&NewVertexBuffer[VertexIndices.Z + UnchangedVertexCt],
				Normals[j]
			));
		}

		// init navmesh
		NavMeshes.Add(UNavMesh());
		UNavMesh& NavMesh = NavMeshes.Last();
		NavMesh.Vertices = NewVertexBuffer;
		NavMesh.VertexCt = NewVertCt;
		Geometry::SetBoundingBox(NavMesh, Group);
		NavMesh.Grid.Init(NavMesh, NewTris);
		NavMesh.Grid.SetVertices(NewVertexBuffer);
		for (auto& TMesh : Group) {
			NavMesh.MeshActors.Add(TMesh->MeshActor);	
		}
	}	
}

void GeometryProcessor::PopulateNodes(const Tri& T, const UnstructuredPolygon& UPoly, TArray<UPolyNode>& PolygonNodes) {
	const TArray<PolyEdge>& Edges = UPoly.Edges;
	for (int i = 0; i < Edges.Num(); i++) {
		const PolyEdge& PEdge = Edges[i];
		const TArray<float> PtDistances = PEdge.TrDropDistances;
		if (PtDistances.Num() == 0) {
			// A is not enclosed here, so is B, so create 2 nodes. else, none
			if (!PEdge.IsAEnclosed()) {
				AddUPolyNodes(PolygonNodes, PEdge.A, PEdge.B);
			}
			continue;
		}

		// NOTE: would be faster but much more data loaded at once to just store positions rather than distances
		FVector StartLoc;
		int start_j;
		const FVector Dir = (PEdge.B - PEdge.A).GetUnsafeNormal();
		const int PtDistCt = PtDistances.Num();
		
		if (PEdge.IsAEnclosed()) {
			StartLoc = PEdge.A + Dir * PtDistances[0];
			if (PtDistCt == 1) {
				AddUPolyNodes(PolygonNodes, StartLoc, PEdge.B);
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
			AddUPolyNodes(PolygonNodes, StartLoc, EndLoc);
			if (++j == PtDistCt) {
				break;		
			}
			StartLoc = PEdge.A + Dir * PtDistances[j];
			if (++j == PtDistCt) {
				AddUPolyNodes(PolygonNodes, StartLoc, PEdge.B);
				break;
			}
		}
	}
	if (UPoly.Edges.Num() > 0 && PolygonNodes.Num() == 0) {
		Data::FailureCaseTris.Add(T);
	}
}

void GeometryProcessor::AddUPolyNodes(TArray<UPolyNode>& Nodes, const FVector& A, const FVector& B) {
	constexpr float EPSILON = 1e-2f;
	int i0 = -1;
	int i1 = -1;
	for (int i = 0; i < Nodes.Num(); i++) {
		UPolyNode& PNode = Nodes[i];
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
		i0 = Nodes.Add(UPolyNode(A));
	}
	if (i1 == -1) {
		i1 = Nodes.Add(UPolyNode(B));
	}
	ADDPOLY_LINK:
	Nodes[i0].Edges.Add(i1);
	Nodes[i1].Edges.Add(i0);
}

void GeometryProcessor::BuildPolygonsFromTri(
	Tri& T,
	TArray<UPolyNode>& PolygonNodes,
	TArray<Polygon>& TMeshPolygons,
	int TriIndex
) {
	// if no nodes were made or the links aren't made properly, this tri could be a problem child, or it could
	// simply have intersections but still be fully enclosed in another mesh
	const int NodeCt = PolygonNodes.Num();
	if (NodeCt < 3) {
		T.MarkForCull();
		return;
	}
	for (int m = 0; m < NodeCt; m++) {
		if (PolygonNodes[m].Edges.Num() % 2 == 1) {
			
			Data::FailureCaseTris.Add(T);
			T.MarkForCull();
			return;
		}
	}
	// if the tri makes it here, its PolygonNodes array is fairly likely composed of one or more closed
	// loops that can be used to form polygon(s)
	for (int StartIndex = 0; StartIndex < NodeCt; ) {
		UPolyNode& StartNode = PolygonNodes[StartIndex];
		TArray<int>* EdgeIndices = &StartNode.Edges;
		
		// check to make sure the node isn't exhausted
		if (EdgeIndices->Num() == 0) {
			StartIndex++;
			continue;
		}
		
		Polygon BuildingPolygon(TriIndex);
		BuildingPolygon.Vertices.Add(PolyNode(StartNode.Location));
		int PrevIndex = StartIndex;
		
		while (true) {
			const int EdgeIndex = (*EdgeIndices)[0];
			if (EdgeIndex == StartIndex) {
				// end of polygon
				EdgeIndices->RemoveAt(0, 1, false);
				const int SNIndexOfEdge = StartNode.Edges.Find(PrevIndex);
				if (SNIndexOfEdge != -1) {
					StartNode.Edges.RemoveAt(SNIndexOfEdge, 1, false);
				}
				BuildingPolygon.Normal = &T.Normal;
				if (BuildingPolygon.Vertices.Num() >= 3) {
					TMeshPolygons.Add(BuildingPolygon);
				}
				break;
			}
			// connect to another node
			UPolyNode& EdgeNode = PolygonNodes[EdgeIndex];
			BuildingPolygon.Vertices.Add(EdgeNode.Location);

			// remove the connection both ways
			EdgeIndices->RemoveAt(0, 1, false);
			if (EdgeNode.Edges.Num() <= 1 || EdgeNode.Edges.Find(PrevIndex) == -1) {
				T.MarkForCull();
				return;
			}
			EdgeNode.Edges.RemoveSingle(PrevIndex);

			// move on to next node
			EdgeIndices = &EdgeNode.Edges;
			PrevIndex = EdgeIndex;
		}
	}
	T.MarkForPolygon();	
}

void GeometryProcessor::PopulateUnmarkedTriData(
	const TArray<TriMesh*>& Group,
	TArray<FVector*>& TempVertices,
	TArray<FIntVector>& TempTriVertexIndices
) {
	int SliceStart = 0;
	for (int j = 0; j < Group.Num(); j++) {
		const auto& TMesh = Group[j];
		const auto& Grid = TMesh->Grid;

		// finding first fully exposed tri on mesh to add, because Slicing the Vertex Array (in next step) requires
		// at least one item in the slice
		int VerticesAdded = 0;
		int StartK = 0;
		const int GridCt = Grid.Num();
		for ( ; StartK < GridCt; StartK++) {
			const auto& Tri = Grid[StartK];
			if (!Tri.IsTriCull() && !Tri.IsTriMarkedForPolygon()) {
				const int AIndex = TempVertices.Add(&Tri.A);
				const int BIndex = TempVertices.Add(&Tri.B);
				const int CIndex = TempVertices.Add(&Tri.C);
				TempTriVertexIndices.Add(FIntVector(AIndex, BIndex, CIndex));
				VerticesAdded += 3;
				StartK++;
				break;
			}
		}
		if (StartK == Grid.Num()) {
			SliceStart += VerticesAdded;
			continue;	
		}

		// Adding vertices and temp tri placeholders. Slicing out only this mesh's vertices to search for existing
		// ones because meshes don't share vertices. Using indices of vertices as placeholders for tris (instead of
		// TempTri w/ FVector*) because the vertex array may reallocate when new vertices are added from the
		// polygon->tri process.
		for (int k = StartK; k < GridCt; k++) {
			const auto& Tri = Grid[k];
			if (!Tri.IsTriCull() && !Tri.IsTriMarkedForPolygon()) {
				TArrayView<FVector*> TempVerticesSlice =
					TArrayView<FVector*>(TempVertices).Slice(SliceStart, VerticesAdded);
				int AIndex = TempVerticesSlice.Find(&Tri.A);
				if (AIndex == -1) {
					AIndex = TempVertices.Add(&Tri.A);
					VerticesAdded++;
				}
				else {
					AIndex += SliceStart;
				}
				int BIndex = TempVerticesSlice.Find(&Tri.B);
				if (BIndex == -1) {
					BIndex = TempVertices.Add(&Tri.B);
					VerticesAdded++;
				}
				else {
					BIndex += SliceStart;
				}
				int CIndex = TempVerticesSlice.Find(&Tri.C);
				if (CIndex == -1) {
					CIndex = TempVertices.Add(&Tri.C);
					VerticesAdded++;
				}
				else {
					CIndex += SliceStart;
				}
				TempTriVertexIndices.Add(FIntVector(AIndex, BIndex, CIndex));
			}
		}
		SliceStart += VerticesAdded;
	}
}

// https://www.geometrictools.com/Documentation/TriangulationByEarClipping.pdf
// Ear clipping is a bit slow compared to other methods, but it's the easiest to implement
void GeometryProcessor::CreateNewTriData(
	TArray<Polygon>& Polygons,
	TArray<FVector>& Vertices,
	TArray<FIntVector>& TriVertexIndices,
	TArray<FVector*>& Normals
) {
	TArray<Geometry::VERTEX_T> VertTypes;
	for (auto& Polygon : Polygons) {
		TArray<PolyNode>& PolyVerts = Polygon.Vertices;
		const FVector& PolyNormal = *Polygon.Normal;
		const int PolyVertCt = PolyVerts.Num();
		const int AddVerticesOffset = Vertices.Num();

		if (PolyVertCt == 3) {
			for (int i = 0; i < PolyVerts.Num(); i++) {
				Vertices.Add(PolyVerts[i].Location);
			}
			TriVertexIndices.Add(FIntVector(
				AddVerticesOffset,
				AddVerticesOffset + 1,
				AddVerticesOffset + 2
			));
			Normals.Add(Polygon.Normal);
			continue;
		}

		double LongestDistSq = DBL_MIN;
		int LongestDistIndex = -1;
		
		TArray<bool> IsEar;
		TArray<FVector> NextVertVec;
		IsEar.Init(false, PolyVertCt);
		NextVertVec.Reserve(PolyVertCt);
		
		for (int i = 0; i < PolyVertCt; i++) {
			PolyNode& Vertex = PolyVerts[i];
			// testing whether or i-1,i,i+1 makes an ear - a triangle with no polygon points inside of it
			IsEar[i] = Geometry::IsEar(Vertex);
			// the farthest vertex from any point R^3 (here, origin) must be the vertex of an interior angle
			const double DistSq = Geometry::DoubleVector::SizeSquared(Vertex.Location);	
			if (DistSq > LongestDistSq) {
				LongestDistSq = DistSq;
				LongestDistIndex = i;
			}
			// getting the vector from each vertex to the next
			NextVertVec.Add(Vertex.Next->Location - Vertex.Location);
		}
		
		// starting on a known interior angled vertex allows us to walk along the polygon and determine the types
		// of the rest.
		{
			VertTypes.Init(Geometry::VERTEX_INTERIOR, PolyVertCt);
			const PolyNode* KnownInterior = &PolyVerts[LongestDistIndex];
			for (const PolyNode* WalkNode = KnownInterior->Next; WalkNode != KnownInterior; WalkNode = WalkNode->Next) {
				const int CurIndex = WalkNode->PolygonIndex;
				const PolyNode& Prev = *WalkNode->Prev;
				const int Prev1Index = Prev.PolygonIndex;
				const int Prev2Index = Prev.Prev->PolygonIndex;
				// WalkNode being checked for interior/exterior - whether the acute angle made by the 3 vertices falls
				// inside the polygon or outside it
				VertTypes[CurIndex] = Geometry::GetPolyVertexType(
					PolyNormal,
					-NextVertVec[Prev2Index],
					NextVertVec[Prev1Index],
					NextVertVec[CurIndex],
					VertTypes[Prev1Index]
				);
			}
		}

		int AvailableCt = PolyVertCt;
		bool FillSuccess = false;
		TArray<FIntVector> TempTriVertexIndices;

		const PolyNode* WalkNode = &PolyVerts[0];
		for (int FailCt = 0; FailCt < AvailableCt; ) {
			const int CurIndex = WalkNode->PolygonIndex;
			if (IsEar[CurIndex] && VertTypes[CurIndex] == Geometry::VERTEX_INTERIOR) {
				FailCt = 0;
				PolyNode* Prev = WalkNode->Prev;
				PolyNode* Next = WalkNode->Next;
				const int PrevIndex = Prev->PolygonIndex;
				const int NextIndex = Next->PolygonIndex;
				
				TempTriVertexIndices.Add(FIntVector(
					PrevIndex + AddVerticesOffset,
					CurIndex + AddVerticesOffset,
					NextIndex + AddVerticesOffset
				));
				Prev->Next = Next;
				Next->Prev = Prev;
				
				if (--AvailableCt == 3) {
					TempTriVertexIndices.Add(FIntVector(
						 PrevIndex + AddVerticesOffset,
						 NextIndex + AddVerticesOffset,
						 Next->Next->PolygonIndex + AddVerticesOffset
					));
					FillSuccess = true;
					break;
				}
				
				IsEar[PrevIndex] = Geometry::IsEar(*Prev);
				IsEar[NextIndex] = Geometry::IsEar(*Next);

				const PolyNode& BackOnce = *Prev->Prev;
				const int BackOnceIndex = BackOnce.PolygonIndex;
				const int BackTwiceIndex = BackOnce.Prev->PolygonIndex;
				
				VertTypes[PrevIndex] = Geometry::GetPolyVertexType(
					PolyNormal,
					-NextVertVec[BackTwiceIndex],
					NextVertVec[BackOnceIndex],
					NextVertVec[PrevIndex],
					VertTypes[BackOnceIndex]
				);
				VertTypes[NextIndex] = Geometry::GetPolyVertexType(
					PolyNormal,
					-NextVertVec[BackOnceIndex],
					NextVertVec[PrevIndex],
					NextVertVec[NextIndex],
					VertTypes[PrevIndex]
				);
			}
			else {
				FailCt++;
			}
			WalkNode = WalkNode->Next;
		}
		if (FillSuccess) {
			for (int i = 0; i < PolyVerts.Num(); i++) {
				Vertices.Add(PolyVerts[i].Location);
			}
			TriVertexIndices.Append(TempTriVertexIndices);
			TArray<FVector*> TriNormals;
			TriNormals.Init(Polygon.Normal, TempTriVertexIndices.Num());
			Normals.Append(TriNormals);
		}
		else {
			Data::FailureCasePolygons.Add(Polygon);
		}
	}
}

void GeometryProcessor::LinkPolygonEdges(Polygon& P) {
	auto& PolyVerts = P.Vertices;
	const int VertCtM1 = PolyVerts.Num() - 1;
	for (int i = 1; i < VertCtM1; i++) {
		PolyNode& Vertex = PolyVerts[i];
		Vertex.PolygonIndex = i;
		Vertex.Prev = &PolyVerts[i-1];
		Vertex.Next = &PolyVerts[i+1];
	}
	PolyNode& V0 = PolyVerts[0];
	PolyNode& VL = PolyVerts[VertCtM1];
	V0.Prev = &VL;
	V0.Next = &PolyVerts[1];
	V0.PolygonIndex = 0;
	VL.Prev = &PolyVerts[VertCtM1-1];
	VL.Next = &V0;
	VL.PolygonIndex = VertCtM1;
}
