#include "GeometryProcessor.h"
#include "Data.h"
#include "Geometry.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RenderingThread.h"
#include "Engine/StaticMeshActor.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"
#include "Debug.h"
#include "DoubleVector.h"
#include "SelectionSet.h"
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
void GeometryProcessor::GroupTriMeshes(
	TArray<TriMesh>& TMeshes,
	TArray<TArray<TriMesh*>>& Groups
) {
	Groups.Reserve(TMeshes.Num());
	const int InMeshCt = TMeshes.Num();
	TArray<int> GroupIndices;
	GroupIndices.Init(-1, InMeshCt);
	TArray<int> PotentialIntersectIndices;
	TArray<int> IntersectIndices;
	TArray<TriMesh*> Remove;
	
	for (int i = 0; i < InMeshCt - 1; i++) {
		TriMesh& TMeshA = TMeshes[i];
		
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
			TriMesh& TMeshB = TMeshes[j];
			if (Geometry::DoBoundingBoxesOverlap(TMeshA.Box, TMeshB.Box)) {
				PotentialIntersectIndices.Add(j);
			}
		}

		// populate group indices array with indices of mesh intersections
		if(Geometry::GetTriMeshIntersectGroups(IntersectIndices, PotentialIntersectIndices, TMeshA, TMeshes)) {
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
		Groups[GrpIndex].Add(&TMeshes[i]);
	}
	
	for (int i = 0; ; ) {
		if (i >= Groups.Num()) {
			break;
		}
		if (Groups[i].Num() == 0) {
			Groups.RemoveAtSwap(i);
		}
		else {
			i++;
		}
	}
	
	UNavDbg::PrintTriMeshIntersectGroups(Groups);
}

void GeometryProcessor::ReformTriMesh(
	TArray<TriMesh*>* Group, FCriticalSection* Mutex, const FThreadSafeBool* IsThreadRun, UNavMesh* NMesh
) {
	TArray<TArray<Polygon>> Polygons;
	auto& GroupRef = *Group;
	// SimplifyTriMesh()
	FlagTrisWithBV(GroupRef);
	if (!IsThreadRun) {
		return;
	}
	BuildPolygonsAtMeshIntersections(GroupRef, Polygons, Mutex);
	if (!IsThreadRun) {
		return;
	}
	FormMeshFromGroup(GroupRef, Polygons, NMesh, Mutex);
}

int GeometryProcessor::GetMeshBatch(
	TArray<TArray<Tri*>>& BatchTris,
	const TriMesh& TMesh,
	int& StartTriIndex,
	uint16 BatchSz,
	uint16 BatchNo
) {
	constexpr float DEVIATION_CUTOFF = 0.9f;
	constexpr int BFS_SEARCH_MAX = 1024;

	if (BatchSz == 0) {
		return GEOPROC_BATCH_SZ_0;
	}
	
	const TriGrid& Grid = TMesh.Grid;
	const int GridCt = Grid.Num();
	if (GridCt < StartTriIndex) {
		return GEOPROC_BAD_START_TRI;
	}
	if (GridCt < BatchSz) {
		BatchSz = GridCt;
	}

	TArray<uint32> BFSTrisA;
	TArray<uint32> BFSTrisB;
	BFSTrisA.Reserve(BFS_SEARCH_MAX);
	BFSTrisB.Reserve(BFS_SEARCH_MAX);
	BFSTrisA.Add(StartTriIndex);
	TArray<uint32>* BFSTrisPtr = &BFSTrisA;
	Tri* Neighbors[3] {nullptr};
	uint16 TriCt = 0;
	
	// populating batch & making sure we exit with m if logical failure or corrupt data
	for (int m = 0; m < BatchSz; m++) {
		const uint32 PrevStartTriIndex = StartTriIndex;
		Tri& StartTri = Grid[StartTriIndex];
		const FVector& GrpTriNormal = StartTri.Normal;
		const int GrpIndex = BatchTris.Add(TArray<Tri*>());
		TArray<Tri*> TempSearched;
		
		BatchTris[GrpIndex].Add(&StartTri);
		StartTri.SetBatch(BatchNo);
		StartTri.SetSearched();
		if (++TriCt >= BatchSz) {
			uint32 BFSTriVIndices[3];
			Grid.GetVIndices(StartTriIndex, BFSTriVIndices);
			const int NeighborCt = Geometry::GetNeighborTris(Grid, StartTriIndex, BFSTriVIndices, Neighbors);
			for (int j = 0; j < NeighborCt; j++) {
				Tri* Neighbor = Neighbors[j];
				StartTri.Neighbors.Add(Neighbor);
			}
			GetNewStartTriIndex(Grid, StartTriIndex);
			return TriCt;
		}
		
		BFSTrisPtr->Add(StartTriIndex);
		bool BatchFinished = false;
		bool OnlyAssignNeighbors = false;
		// populating group (within batch) with tris that have normals similar to first tri in group
		while (true) {
			TArray<uint32>& BFSTris = *BFSTrisPtr;
			TArray<uint32>& BFSTrisNext = (BFSTrisPtr == &BFSTrisA ? BFSTrisB : BFSTrisA);
			// iterating over this group's outer members (BFS search edge)
			for (int i = 0; i < BFSTris.Num(); i++) {
				// getting next tri whose neighbors we will be checking
				const uint32 BFSTriIndex = BFSTris[i];
				Tri& BFSTri = Grid[BFSTriIndex];

				// getting the neighbors
				uint32 BFSTriVIndices[3];
				Grid.GetVIndices(BFSTriIndex, BFSTriVIndices);
				const int NeighborCt = Geometry::GetNeighborTris(Grid, BFSTriIndex, BFSTriVIndices, Neighbors);

				// iterating over neighbors...
				for (int j = 0; j < NeighborCt; j++) {
					Tri* Neighbor = Neighbors[j];
					BFSTri.Neighbors.Add(Neighbor);
					if (OnlyAssignNeighbors) {
						continue;
					}
					// checking if they're already part of a group or have been searched by this group...
					if (!Neighbor->IsSearched()) {
						const int TIndex = Grid.GetIndex(Neighbor);
						Neighbor->SetSearched();
						// and checking if this neighbor's normal is similar enough to start tri's to be added to the group
						const float CosPhi = FVector::DotProduct(GrpTriNormal, Neighbor->Normal);
						if (
							!Neighbor->IsInBatch()
							&& CosPhi > DEVIATION_CUTOFF
						) {
							BFSTrisNext.Add(TIndex); // this neighbor will become an outer member (a search node)
							BatchTris[GrpIndex].Add(Neighbor); // add tri to this group...
							Neighbor->SetBatch(BatchNo); // and flag it for this batch
							
							if (++TriCt >= BatchSz) {
								OnlyAssignNeighbors = true;
								BatchFinished = true;
							}
							else if (BFSTrisNext.Num() >= BFS_SEARCH_MAX) {
								OnlyAssignNeighbors = true;
							}
						}
						else {
							TempSearched.Add(Neighbor);
							StartTriIndex = Grid.GetIndex(Neighbor);
						}
					}
				}
			}

			if (BatchFinished) {
				// setting start index for next
				if (StartTriIndex == PrevStartTriIndex) {
					GetNewStartTriIndex(Grid, StartTriIndex);
				}
				return TriCt;
			}
			// for tris that were not added to the group and are unbatched, unset them as searched
			for (const auto T : TempSearched) {
				T->UnsetSearched();
			}
			TempSearched.Empty();
			BFSTris.Empty(BFS_SEARCH_MAX);
			BFSTrisPtr = &BFSTrisNext;
			if (BFSTrisPtr->Num() == 0) {
				break;	
			}
		}

		// if all searched tris were added to the last group or were already searched, either the tmesh has been fully
		// searched, or there are islands on the mesh that still need to be searched
		if (PrevStartTriIndex == StartTriIndex && !GetNewStartTriIndex(Grid, StartTriIndex)) {
			break;
		}	
	}

	// TODO: parameterize the values that determine the outcome here
	// outside:
	// create new polygon vertex buffer VBuf
	// create all polygon array
	// create edge polygon array (indices in all array)
	// with threading:
	// until tris exhausted:
	//		create batch by connecting tris with similar vertices; batch tris are collected with BFS; all batches
	//		after first should start with a tri that was a neighbor of a tri in the previous batch
	//			while creating, form groups of similar normal'd tris; each group has a normal deviation budget
	//			that accrues per add; it's possible there's no need for normal delta preference
	//		create polygon group array
	//		per tri group:
	//			form a polygon with the edge of group
	//			over all polygon edges, check for neighboring edge polygons and connect if found
	//			remove the original tris from the pool
	//			add polygon to group array
	//		per group polygon i = 0 to end - 2:
	//			per group polygon j = i + 1 to end - 1:
	//				use tri connections to form neighbor connections (will be useful in graph creation)
	//				simplify edges between i and j by culling vertices that do not deviate beyond some delta from start search
	//				Add shared edge vertices to pool, and share references
	//				remove neighbor tris
	//		add polygons to all polygons
	//		add indices to edge index array if edge
	// per polygon i = 0 to end - 2 in edge polygons:
	//		per polygon j = i + 1 to end - 1 in edge polygons:
	//			use tri connections to form neighbor connections (will be useful in graph creation)
	//			simplify edges between i and j by culling vertices that do not deviate beyond some delta from start search
	//			Add shared edge vertices to pool, and share references
	//			remove neighbor tris
	//	triangulize polygons
	//	re-make mesh with new tris
	return TriCt;
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
	TMesh.Grid.SetVertices(TMesh.Vertices);
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

Tri* GeometryProcessor::GetUnbatchedTri(const TriGrid& Grid) {
	for (int i = 0; i < Grid.Num(); i++) {
		auto& T = Grid[i];
		if (!T.IsInBatch()) {
			return &T;
		}
	}
	return nullptr;
}

bool GeometryProcessor::GetNewStartTriIndex(const TriGrid& Grid, int& StartTriIndex) {
	const Tri* Unbatched = GetUnbatchedTri(Grid);
	if (Unbatched != nullptr) {
		StartTriIndex = Grid.GetIndex(Unbatched);
		return true;
	}
	// TMesh fully searched
	StartTriIndex = -1;
	return false;
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

void GeometryProcessor::FlagTrisWithBV(TArray<TriMesh*>& TMeshes) {
	for (int j = 0; j < TMeshes.Num(); j++) {
		TriMesh& TMesh = *TMeshes[j];
		if (!Geometry::IsBoxAInBoxB(TMesh.Box, Data::BoundsVolumeTMesh.Box)) {
			Geometry::FlagTriVerticesInsideBoundsVolume(TMesh);
		}
		else {
			auto& Grid = TMesh.Grid;
			for (int k = 0; k < Grid.Num(); k++) {
				Tri& T = Grid[k];
				T.SetInsideBV();
			}
		}
	}
}

void GeometryProcessor::BuildPolygonsAtMeshIntersections(
	TArray<TriMesh*>& Group,
	TArray<TArray<Polygon>>& GroupPolygons,
	FCriticalSection* Mutex
) {
	const auto& Grid = Data::BoundsVolumeTMesh.Grid;
	for (int i = 0; i < Grid.Num(); i++) {
		Grid[i].Normal = -Grid[i].Normal;
	}
	
	TArray<TArray<UnstructuredPolygon>> UPolys;

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
	Geometry::FindIntersections(Group, UPolys);

	// removing the bounds volume upolys since we don't care what intersections landed on it;
	Group.RemoveAt(GroupCt - 1);
	UPolys.RemoveAt(GroupCt - 1);
	
	for (int j = 0; j < UPolys.Num(); j++) {
		TArray<UnstructuredPolygon>& MeshUPolys = UPolys[j];
		TriMesh& TMesh = *Group[j];
		GroupPolygons.Add(TArray<Polygon>());
		TArray<Polygon>& TMeshPolygons = GroupPolygons.Last();
		
		for (int k = 0; k < MeshUPolys.Num(); k++) {
			Tri& T = TMesh.Grid[k];

			TArray<UPolyNode> PolygonNodes;
			if (T.IsCull()) {
				// Tris are marked for cull early if they fall outside of the bounds box
				continue;
			}
			const UnstructuredPolygon& UPoly = MeshUPolys[k];
			if (UPoly.Edges.Num() == 0) {
				// if there are no intersections...
				if (T.AllObscured()) {
					// ...and all vertices are obscured, tri is enclosed -> cull
					T.MarkForCull();
				}
				else if (T.AnyObscured()) {
					// ... and 0 < n < 3 of the vertices are obscured, something has gone wrong
					T.MarkProblemCase();
					T.MarkForCull();
					FScopeLock Lock(Mutex);
					Data::FailureCaseTris.Add(&T);
				}	
				continue;
			}

			// creating graphs where edges (intersections and tri edges) connect, and where they're visible
			PopulateNodes(T, UPoly, PolygonNodes);
			
			Polygonize(T, PolygonNodes, TMeshPolygons, k);
			
			// TODO: each tri might come out with more than one polygon; for example, a set of intersections in the center...
			// TODO: ... of the tri that do not touch tri edges - one outside, one inside; but, we start with one...
			// TODO: ... add if ever touches edge, else subtract unless enclosed by subtract polygon
			if (T.IsCull() || T.IsProblemCase()) {
				FScopeLock Lock(Mutex);
				Data::FailureCaseTris.Add(&T);
			}
		}
	}
	for (auto& MeshPolygons : GroupPolygons) {
		for (auto& Polygon : MeshPolygons) {
			LinkPolygonEdges(Polygon);
		}
	}	
}

void GeometryProcessor::FormMeshFromGroup(
	TArray<TriMesh*>& Group,
	TArray<TArray<Polygon>>& Polygons,
	UNavMesh* NMesh,
	FCriticalSection* Mutex
) {
	// reserving space for temp tris and verts
	int TriCt = 0;
	int VertexCt = 0;
	for (const auto& TMesh : Group) {
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
	TArray<FVector*> Normals;
	for (int j = 0; j < Polygons.Num(); j++) {
		auto& MeshPolygons = Polygons[j];
		Triangulize(MeshPolygons, NewVertices, NewTriVertexIndices, Normals);
	}

	// making new vertex buffer
	const int UnchangedVertexCt = UnchangedVertices.Num();
	const int NewVertexCt = NewVertices.Num();
	const int NewVertCt = UnchangedVertexCt + NewVertexCt;
	FVector* NewVertexBuffer = new FVector[NewVertCt];
	if (NewVertexBuffer == nullptr) {
		return;
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
	NMesh->Vertices = NewVertexBuffer;
	NMesh->VertexCt = NewVertCt;
	Geometry::SetBoundingBox(*NMesh, Group);
	NMesh->Grid.Init(*NMesh, NewTris);
	NMesh->Grid.SetVertices(NewVertexBuffer);
	for (const auto& TMesh : Group) {
		NMesh->MeshActors.Add(TMesh->MeshActor);	
	}
		
}

void GeometryProcessor::PopulateNodes(const Tri& T, const UnstructuredPolygon& UPoly, TArray<UPolyNode>& PolygonNodes) {
	const TArray<PolyEdge>& Edges = UPoly.Edges;
	TArray<const PolyEdge*> RecheckEdges;
	const int InitNodeCt = PolygonNodes.Num();
	int AddedNodes = 0;
	
	for (int i = 0; i < Edges.Num(); i++) {
		const PolyEdge& PEdge = Edges[i];
		const TArray<FVector>& ObscuredLocations = PEdge.ObscuredLocations;
		if (ObscuredLocations.Num() == 0) {
			// if A is not enclosed here, so is B, so create 2 nodes. if both enclosed, create none.
			if (!PEdge.IsAEnclosed()) {
				AddUPolyNodes(PolygonNodes, PEdge.A, PEdge.B, AddedNodes);
			}
			else {
				// however, it's possible that an edge incorrectly identified itself as being inside another mesh
				// due to a glancing hit on that mesh during a line test. We can later ask the other nodes if they mach
				// the nodes on this edge
				RecheckEdges.Add(&PEdge);
			}
			continue;
		}

		// NOTE: would be faster but much more data loaded at once to just store positions rather than distances
		FVector StartLoc;
		int start_j;
		const int PtDistCt = ObscuredLocations.Num();
		
		if (PEdge.IsAEnclosed()) {
			StartLoc = ObscuredLocations[0];
			if (PtDistCt == 1) {
				AddUPolyNodes(PolygonNodes, StartLoc, PEdge.B, AddedNodes);
				continue;	
			}
			start_j = 1;
		}
		else {
			StartLoc = PEdge.A;
			start_j = 0;
		}

		for (int j = start_j; ; ) {
			FVector EndLoc = ObscuredLocations[j];
			AddUPolyNodes(PolygonNodes, StartLoc, EndLoc, AddedNodes);
			if (++j == PtDistCt) {
				break;		
			}
			StartLoc = ObscuredLocations[j];
			if (++j == PtDistCt) {
				AddUPolyNodes(PolygonNodes, StartLoc, PEdge.B, AddedNodes);
				break;
			}
		}
	}
	if (AddedNodes > 0 && RecheckEdges.Num() != 0) {
		const TArrayView<UPolyNode> NodesSlice =
			TArrayView<UPolyNode>(PolygonNodes).Slice(InitNodeCt, AddedNodes);
		for (const auto EdgePtr : RecheckEdges) {
			auto& Edge = *EdgePtr;
			if (DoesEdgeConnect(NodesSlice, Edge)) {
				AddUPolyNodes(PolygonNodes, Edge.A,  Edge.B, AddedNodes);	
			}	
		}
	}
}

// Used by Populate Nodes in case an edge incorrectly identifies itself as being inside another mesh. In this case,
// due to the nature of the line test, a single edge forms a single sample on whether or not both of its nodes
// are enclosed. If there are two overlapping nodes that both disagree with this edge, we're statistically inclined
// to believe the majority of samples.
bool GeometryProcessor::DoesEdgeConnect(const TArrayView<UPolyNode>& Nodes, const PolyEdge& Edge) {
	bool AConnection = false;
	bool BConnection = false;
	for (const auto& Node : Nodes) {
		if (!AConnection && FVector::DistSquared(Edge.A, Node.Location) < EPSILON) {
			if (BConnection) {
				return true;
			}
			AConnection = true;
		}
		if (!BConnection && FVector::DistSquared(Edge.B, Node.Location) < EPSILON) {
			if (AConnection) {
				return true;
			}
			BConnection = true;
		}
	}
	return false;
}

void GeometryProcessor::AddUPolyNodes(TArray<UPolyNode>& Nodes, const FVector& A, const FVector& B, int& NodeCtr) {
	int i0 = -1;
	int i1 = -1;
	for (int i = 0; i < Nodes.Num(); i++) {
		UPolyNode& PNode = Nodes[i];
		if (i0 == -1 && FVector::DistSquared(A, PNode.Location) < EPSILON) {
			i0 = i;
			if (i1 != -1) {
				goto ADDPOLY_LINK;
			}
		}
		else if (i1 == -1 && FVector::DistSquared(B, PNode.Location) < EPSILON) {
			i1 = i;
			if (i0 != -1) {
				goto ADDPOLY_LINK;
			}
		}
	}
	if (i0 == -1) {
		i0 = Nodes.Add(UPolyNode(A));
		NodeCtr++;
	}
	if (i1 == -1) {
		i1 = Nodes.Add(UPolyNode(B));
		NodeCtr++;
	}
	ADDPOLY_LINK:
	Nodes[i0].Edges.Add(i1);
	Nodes[i1].Edges.Add(i0);
}

void GeometryProcessor::Polygonize(
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
		
		Polygon BuildingPolygon(TriIndex, T.Normal);
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
				if (BuildingPolygon.Vertices.Num() >= 3) {
					T.MarkForPolygon();	
					TMeshPolygons.Add(BuildingPolygon);
				}
				break;
			}
			// connect to another node
			UPolyNode& EdgeNode = PolygonNodes[EdgeIndex];
			BuildingPolygon.Vertices.Add(EdgeNode.Location);

			// remove the connection both ways
			EdgeIndices->RemoveAt(0, 1, false);
			if (EdgeNode.Edges.Num() <= 1) {
				T.MarkProblemCase();
				if (EdgeNode.Edges.Find(PrevIndex) != -1) {
					EdgeNode.Edges.RemoveSingle(PrevIndex);
				}
				break;
			}
			if (EdgeNode.Edges.Find(PrevIndex) == -1) {
				T.MarkProblemCase();
				break;
			}
			EdgeNode.Edges.RemoveSingle(PrevIndex);

			// move on to next node
			EdgeIndices = &EdgeNode.Edges;
			PrevIndex = EdgeIndex;
		}
	}
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
			const auto& T = Grid[StartK];
			if (!T.IsChanged()) {
				const int AIndex = TempVertices.Add(&T.A);
				const int BIndex = TempVertices.Add(&T.B);
				const int CIndex = TempVertices.Add(&T.C);
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
			if (!Tri.IsChanged()) {
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
void GeometryProcessor::Triangulize(
	TArray<Polygon>& Polygons,
	TArray<FVector>& Vertices,
	TArray<FIntVector>& TriVertexIndices,
	TArray<FVector*>& Normals
) {
	TArray<Geometry::VERTEX_T> VertTypes;
	for (auto& Polygon : Polygons) {
		TArray<PolyNode>& PolyVerts = Polygon.Vertices;
		const int PolyVertCt = PolyVerts.Num();
		const int AddVerticesOffset = Vertices.Num();

		if (PolyVertCt == 3) {
			for (int i = 0; i < PolyVerts.Num(); i++) {
				Vertices.Add(PolyVerts[i].Location);
			}
			AddTriData(
				TriVertexIndices, PolyVerts[0], PolyVerts[1], PolyVerts[2], Polygon.Normal, AddVerticesOffset
			);
			Normals.Add(&Polygon.Normal);
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
			const double DistSq = DoubleVector::SizeSquared(Vertex.Location);	
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
					Polygon.Normal,
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

				AddTriData(
					TempTriVertexIndices, *Prev, *WalkNode, *Next, Polygon.Normal, AddVerticesOffset
				);	
				Prev->Next = Next;
				Next->Prev = Prev;
				
				if (--AvailableCt == 3) {
					AddTriData(
						TempTriVertexIndices, *Prev, *Next, *Next->Next, Polygon.Normal, AddVerticesOffset
					);	
					FillSuccess = true;
					break;
				}
				
				const int PrevIndex = Prev->PolygonIndex;
				const int NextIndex = Next->PolygonIndex;
				IsEar[PrevIndex] = Geometry::IsEar(*Prev);
				IsEar[NextIndex] = Geometry::IsEar(*Next);

				const PolyNode& BackOnce = *Prev->Prev;
				const int BackOnceIndex = BackOnce.PolygonIndex;
				const int BackTwiceIndex = BackOnce.Prev->PolygonIndex;
				
				VertTypes[PrevIndex] = Geometry::GetPolyVertexType(
					Polygon.Normal,
					-NextVertVec[BackTwiceIndex],
					NextVertVec[BackOnceIndex],
					NextVertVec[PrevIndex],
					VertTypes[BackOnceIndex]
				);
				VertTypes[NextIndex] = Geometry::GetPolyVertexType(
					Polygon.Normal,
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
			TriNormals.Init(&Polygon.Normal, TempTriVertexIndices.Num());
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

void GeometryProcessor::AddTriData(
	TArray<FIntVector>& Indices,
	const PolyNode& A,
	const PolyNode& B,
	const PolyNode& C,
	const FVector& Normal,
	int IndexOffset
) {
	// Polygons often end up oriented differently than the original tris. Just fixing the indices here so
	// the procedural mesh representation of UNavMesh has the indices in the correct order
	if (FVector::DotProduct(Tri::CalculateNormal(A.Location, B.Location, C.Location), Normal) <= 0.0f) {
		Indices.Add(FIntVector(
			C.PolygonIndex + IndexOffset, B.PolygonIndex + IndexOffset, A.PolygonIndex + IndexOffset
		));
	}
	else {
		Indices.Add(FIntVector(
			A.PolygonIndex + IndexOffset, B.PolygonIndex + IndexOffset, C.PolygonIndex + IndexOffset
		));
	}
}
