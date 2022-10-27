#include "Geometry.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"

namespace Geometry {
	
	static constexpr float ONE_THIRD = 1.0f / 3.0f;
	static constexpr float NEAR_EPSILON = 1e-4f;

	namespace {

		// The unscaled/unrotated/untranslated vertices of a bounding box
		const FVector BaseExtent[BoundingBox::VERTEX_CT] {
			FVector(-1.0f, -1.0f, -1.0f), // neighbors 1, 2, 3
			FVector( 1.0f, -1.0f, -1.0f),
			FVector(-1.0f,  1.0f, -1.0f),
			FVector(-1.0f, -1.0f,  1.0f),
			FVector( 1.0f,  1.0f, -1.0f), // neighbors 1, 2, 7
			FVector( 1.0f, -1.0f,  1.0f), // neighbors 1, 3, 7
			FVector(-1.0f,  1.0f,  1.0f), // neighbors 2, 3, 7
			FVector( 1.0f,  1.0f,  1.0f)
		};
		
		constexpr int BBoxFaceIndices[BoundingBox::FACE_CT][3] {
			{0, 1, 2}, {0, 1, 3}, {0, 2, 3},
			{7, 4, 5}, {7, 5, 6}, {7, 4, 6}
		};

		constexpr int BBoxEdgeIndices[BoundingBox::EDGE_CT][2] {
			{0, 1}, {0, 2}, {1, 4}, {2, 4},
			{3, 5}, {3, 6}, {7, 5}, {7, 6},
			{1, 5}, {4, 7}, {2, 6}, {0, 3}
		};

		enum RAY_HIT {
			RAY_HIT_NONE=0,
			RAY_HIT_ATOB=1,
			RAY_HIT_BTOA=2
		};

		struct MeshHit {
			MeshHit(const AStaticMeshActor* _Mesh, const FVector& _Location) :
				Mesh{_Mesh}, Location(_Location)
			{}
			
			static bool XCmp(const MeshHit& A, const MeshHit& B) {
				return A.Location.X < B.Location.X;
			}
			
			static bool YCmp(const MeshHit& A, const MeshHit& B) {
				return A.Location.Y < B.Location.Y;
			}
			
			static bool ZCmp(const MeshHit& A, const MeshHit& B) {
				return A.Location.Z < B.Location.Z;
			}
			
			const AStaticMeshActor* Mesh;
			const FVector Location;
		};

		// used for counting mesh hits up to a point to check if a point lies between mesh hits.
		// mesh data only populated only at constructor
		struct MeshHitCounter {

			MeshHitCounter(const TArray<TriMesh*>& TMeshPtrs) {
				for (int i = 0; i < TMeshPtrs.Num(); i++) {
					AStaticMeshActor* MeshActor = TMeshPtrs[i]->MeshActor;
					MeshToCt.Add(MeshActor, 0);
					Keys.Add(MeshActor);
				}		
			}
		
			void Add(const AStaticMeshActor* MeshActor) {
				MeshToCt[MeshActor] += 1;
			}

			bool AnyOdd() {
				for (int i = 0; i < Keys.Num(); i++) {
					if (MeshToCt[Keys[i]] % 2 == 1) {
						return true;
					}
				}
				return false;
			}

			void Reset() {
				for (int i = 0; i < Keys.Num(); i++) {
					MeshToCt[Keys[i]] = 0;
				}
			}

		private:
			
			TMap<AStaticMeshActor*, int> MeshToCt;
			TArray<AStaticMeshActor*> Keys;
			
		};	
		
		// populate BBox with vertices and precomputed overlap-checking values
		// This implementation of a bounding box includes rotations, which makes it a little more computationally
		// expensive to check if a point lies inside or if a line intersects a face. However, this saves processor time
		// in the long run since *far* more work is done checking where meshes overlap, if they potentially overlap. And,
		// bounding boxes that rotate with meshes are much smaller, thus giving less false positives.
		void Internal_SetBoundingBox(
			BoundingBox& BBox,
			const FVector& Extent,
			const FTransform& TForm, 
			const FVector& Center,
			bool IsStaticMesh
		) {
			if (IsStaticMesh) {
				for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
					const FVector RotatedCenter = TForm.TransformVector(Center);
					BBox.Vertices[i] = TForm.TransformPosition(BaseExtent[i] * Extent) + RotatedCenter;
				}
			}
			else {
				for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
					BBox.Vertices[i] = TForm.TransformPositionNoScale(BaseExtent[i] * Extent);
				}
			}
			for (int i = 0; i < BoundingBox::FACE_CT; i++) {
				const int* SideFaceIndices = BBoxFaceIndices[i];
				const FVector& VertA = BBox.Vertices[SideFaceIndices[0]];
				const FVector& VertB = BBox.Vertices[SideFaceIndices[1]];
				const FVector& VertC = BBox.Vertices[SideFaceIndices[2]];
				BBox.FaceNormals[i] = FVector::CrossProduct(VertB - VertA, VertC - VertA).GetUnsafeNormal();
			}
			const FVector& RefPoint = BBox.Vertices[0];
			for (int i = 0; i < 3; i++) {
				FVector& OverlapCheckVector = BBox.OverlapCheckVectors[i];
				OverlapCheckVector = BBox.Vertices[i + 1] - RefPoint;
				BBox.OverlapCheckSqMags[i] = FVector::DotProduct(OverlapCheckVector, OverlapCheckVector);
			}	
		}
		
		// If the points were all on a line, you would only need to check magnitude; implicit scaling by cos(theta) in
		// dot product does the work of checking in 3 dimensions
		// Ref: https://math.stackexchange.com/questions/1472049/check-if-a-point-is-inside-a-rectangular-shaped-area-3d
		bool Internal_IsPointInsideBox(const BoundingBox& BBox, const FVector& Point) {
			const FVector V = Point - BBox.Vertices[0];
			float SideCheck = FVector::DotProduct(V, BBox.OverlapCheckVectors[0]);
			if (SideCheck <= 0.0f) {
				return false;
			}
			if (SideCheck >= BBox.OverlapCheckSqMags[0]) {
				return false;
			}
			SideCheck = FVector::DotProduct(V, BBox.OverlapCheckVectors[1]);
			if (SideCheck <= 0.0f) {
				return false;
			}
			if (SideCheck >= BBox.OverlapCheckSqMags[1]) {
				return false;
			}
			SideCheck = FVector::DotProduct(V, BBox.OverlapCheckVectors[2]);
			if (SideCheck <= 0.0f) {
				return false;
			}
			if (SideCheck >= BBox.OverlapCheckSqMags[2]) {
				return false;
			}
			return true;
		}

		// clever trick: assume the pt is inside the triangle. then, the pt makes three triangles - one with every pair
		// of tri vertices. the sum of those areas is equal to the area of the triangle.
		bool Internal_DoesPointTouchTri(const Tri& T, const FVector& P) {
			return (
				Tri::GetArea(P, T.A, T.B) + Tri::GetArea(P, T.B, T.C) + Tri::GetArea(P, T.C, T.A)
				<= T.Area + NEAR_EPSILON
			);
		}

		// does point touch this rectangular face? assumes pt has been projected onto plane with vertices.
		// asks if the angles between the edges and the point make sense, and if the line segments of the point projected
		// onto the edges are both shorter than the lengths of the edges.
		bool Internal_DoesPointTouchFace(
			const FVector& VertA,
			const FVector& VertB,
			const FVector& VertC,
			const FVector& Pt
		) {
			const FVector FaceEdgeB = VertB - VertA;	
			const FVector FaceEdgeC = VertC - VertA;
			const FVector PtVec = Pt - VertA;
			const float LenFEB = FaceEdgeB.Size() + NEAR_EPSILON;
			const float LenFEC = FaceEdgeC.Size() + NEAR_EPSILON;
			const float LenPVC = PtVec.Size();
			const float CosTheta = FVector::DotProduct(PtVec, FaceEdgeB) / (LenPVC * LenFEB);
			const float CosPhi = FVector::DotProduct(PtVec, FaceEdgeC) / (LenPVC * LenFEC);
			return (CosTheta >= 0) && (CosPhi >= 0) && (LenPVC * CosTheta <= LenFEB) && (LenPVC * CosPhi <= LenFEC);
		}

		// does the ray hit the rectangular face provided? Omitted vertex is assumed to make the face rectangular
		// and BAC is assumed to form a right angle
		bool Internal_Raycast(
			const FVector& Origin,
			const FVector& Dir,
			float Length,
			const FVector& FacePtA,
			const FVector& FacePtB,
			const FVector& FacePtC,
			const FVector& Normal, // could calc here, but currently precomputed for BoundingBox
			FVector& POI
		) {
			const FVector PVec = Origin - FacePtA;
			if (FVector::DotProduct(PVec, Normal) <= 0.0f) {
				return false;
			}
			const float RayNormCosTheta = FVector::DotProduct(Dir, -Normal);
			if (RayNormCosTheta <= 0.0f) {
				return false;
			}
			const float LenOProj = FVector::DotProduct(Normal, PVec);
			const float RayHitDist = LenOProj / RayNormCosTheta;
			if (RayHitDist > Length) {
				return false;
			}
			POI = Origin + Dir * RayHitDist;
			return Internal_DoesPointTouchFace(FacePtA, FacePtB, FacePtC, POI);
		}

		// thinks in triangles: the one made by the tri plane, origin -> center, origin -> projection
		// and the one made by the tri plane, origin -> projection, origin -> ray hit. Could be faster, I'm sure.
		bool Internal_Raycast(
			const FVector& Origin,
			const FVector& Dir,
			float Length,
			const Tri& T,
			FVector& POI
		) {
			const FVector PVec = Origin - T.A;
			if (FVector::DotProduct(PVec, T.Normal) <= 0.0f) {
				return false;
			}
			const float RayNormCosTheta = FVector::DotProduct(Dir, -T.Normal);
			if (RayNormCosTheta <= 0.0f) {
				return false;
			}
			const float LenOProj = FVector::DotProduct(T.Normal, PVec);
			const float RayHitDist = LenOProj / RayNormCosTheta;
			if (RayHitDist > Length) {
				return false;
			}
			POI = Origin + Dir * RayHitDist;
			return Internal_DoesPointTouchTri(T, POI);
		}
		
		bool Internal_DoesLineSegmentIntersectBox(const FVector& LSA, const FVector& LSB, const BoundingBox& BBox) {
			FVector PointOfIntersection;
			FVector Dir = LSB - LSA;
			const float Length = Dir.Size();
			Dir *= 1.0f / Length;
			const FVector* Origin = &LSA;
			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < BoundingBox::FACE_CT; j++) {
					const int* FaceIndices = BBoxFaceIndices[j];
					const FVector& FaceA = BBox.Vertices[FaceIndices[0]];
					const FVector& FaceB = BBox.Vertices[FaceIndices[1]];
					const FVector& FaceC = BBox.Vertices[FaceIndices[2]];
					
					// only need to check one direction, since this function is used in concert with
					// Internal_IsPointInsideBox(). If there are no vertices from box A in box B, then an intersecting
					// edge of A through B is checkable either direction.
					if (Internal_Raycast(
						*Origin, Dir, Length, FaceA, FaceB, FaceC, BBox.FaceNormals[j], PointOfIntersection
					)) {
						return true;
					}
				}
				Dir = -Dir;
				Origin = &LSB;
			}
			return false;
		}

		RAY_HIT Internal_RaycastBidirectional(
			const FVector& LSA,
			const FVector& LSB,
			const Tri& T,
			FVector& PointOfIntersection
		) {
			FVector Dir = LSB - LSA;
			const float Length = Dir.Size();
			Dir *= 1.0f / Length;
			if (Internal_Raycast(LSA, Dir, Length, T, PointOfIntersection)) {
				return RAY_HIT_ATOB;
			}
			Dir = -Dir;
			if (Internal_Raycast(LSB, Dir, Length, T, PointOfIntersection)) {
				return RAY_HIT_BTOA;
			}
			return RAY_HIT_NONE;
		}

		bool Internal_DoBoundingBoxesIntersect(const BoundingBox& BBoxA, const BoundingBox& BBoxB) {
			const BoundingBox* EdgesBoxPtr = &BBoxA;
			const BoundingBox* FacesBoxPtr = &BBoxB;
			for (int i = 0; i < 2; i++) {
				const BoundingBox& EdgesBox = *EdgesBoxPtr;
				const BoundingBox& FacesBox = *FacesBoxPtr;
				for (int j = 0; j < BoundingBox::EDGE_CT; j++) {
					const int* EdgeIndices = BBoxEdgeIndices[j];
					const FVector EdgeA = EdgesBox.Vertices[EdgeIndices[0]];
					const FVector EdgeB = EdgesBox.Vertices[EdgeIndices[1]];
					if (Internal_DoesLineSegmentIntersectBox(EdgeA, EdgeB, FacesBox)) {
						return true;
					}	
				}
				EdgesBoxPtr = &BBoxB;
				FacesBoxPtr = &BBoxA;
			}
			return false;
		}

		bool Internal_DoTriMeshesIntersect(const TriMesh& TMeshA, const TriMesh& TMeshB) {
			const TArray<Tri>& TMeshATris = TMeshA.Tris;
			const TArray<Tri>& TMeshBTris = TMeshB.Tris;
			const int TMeshBTriCt = TMeshBTris.Num();
			FVector POI;
			for (int i = 0; i < TMeshATris.Num(); i++) {
				const Tri& T0 = TMeshATris[i];
				for (int j = 0; j < TMeshBTriCt; j++) {
					const Tri& T1 = TMeshBTris[j];
					// checking if it's possible they could intersect
					const float Dist = FVector::Dist(T0.A, T1.A);
					if (
						(Dist < T0.LongestSidelen + T1.LongestSidelen)
						&& (
							Internal_RaycastBidirectional(T0.A, T0.B, T1, POI)
							|| Internal_RaycastBidirectional(T0.B, T0.C, T1, POI)
							|| Internal_RaycastBidirectional(T0.C, T0.A, T1, POI)
							|| Internal_RaycastBidirectional(T1.A, T1.B, T0, POI)
							|| Internal_RaycastBidirectional(T1.B, T1.C, T0, POI)
							|| Internal_RaycastBidirectional(T1.C, T1.A, T0, POI)
						)
					) {
						return true;
					}
				}
			}
			return false;
		}

		// TODO: currently depends on a channel added in-project. will need to have the plugin create the channel...
		// TODO: ... used here. Default channel result is overlap.
		void Internal_LineTraceThrough(
			const UWorld* World,
			const FCollisionQueryParams& Params,
			const FVector& TrStart,
			const FVector& TrEnd,
			TArray<MeshHit>& MHits
		) {
			TArray<FHitResult> HitResults;
			World->LineTraceMultiByChannel(HitResults, TrStart, TrEnd, ECC_GameTraceChannel1, Params);
			for (int i = 0; i < HitResults.Num(); i++) {
				const FHitResult& Hit = HitResults[i];
				AActor* HitActor = Hit.GetActor();
				if (HitActor != nullptr) {
					const AStaticMeshActor* HitMesh = Cast<AStaticMeshActor>(HitActor);
					if (HitMesh != nullptr) {
						MHits.Add(MeshHit(HitMesh, Hit.Location));	
					}	
				}
			}	
		}
		
		bool Internal_IsPointObscured(
			const UWorld* World,
			const FCollisionQueryParams& Params,
			const FVector& Pt,
			float MinZ,
			MeshHitCounter& MHitCtr
		) {
			TArray<MeshHit> MHits;

			// trace through Pt along z axis (could be any axis, but z might have least superfluous hits) both ways 
			const FVector TrEnd (Pt.X, Pt.Y, MinZ);
			Internal_LineTraceThrough(World, Params, Pt, TrEnd, MHits);
			Internal_LineTraceThrough(World, Params, TrEnd, Pt, MHits);

			// sort by z
			MHits.Sort(MeshHit::ZCmp);
			
			// count through sorted mesh hits
			MHitCtr.Reset();
			for (int i = 0; i < MHits.Num(); i++) {
				MHitCtr.Add(MHits[i].Mesh);
			}
			// once we've gone to the point, any odd mesh hit cts mean pt is enclosed
			return MHitCtr.AnyOdd();
		}
		
		// params should be set to complex and to ignore the mesh of the given tri; does NOT clear flags beforehand
		bool Internal_IsTriObscured(
			const UWorld* World,
			const FCollisionQueryParams& Params,
			Tri& T,
			float MinZ,
			MeshHitCounter& MHitCtr
		) {
			if (Internal_IsPointObscured(World, Params, T.A, MinZ, MHitCtr)) {
				T.SetAObscured();
			}
			if (Internal_IsPointObscured(World, Params, T.B, MinZ, MHitCtr)) {
				T.SetBObscured();
			}
			if (Internal_IsPointObscured(World, Params, T.C, MinZ, MHitCtr)) {
				T.SetCObscured();
			}
			return T.AnyObscured();
		}

		void Internal_GetTriPairPolyEdges(
			const Tri& T0,
			const Tri& T1,
			UnstructuredPolygon& PolyA,
			UnstructuredPolygon& PolyB
		) {
			FVector PointOfIntersection;
			FVector TruePOI[2][2];
			int T0HitCt = 0;
			int T1HitCt = 0;
			if (Internal_RaycastBidirectional(T0.A, T0.B, T1, PointOfIntersection)) {
				TruePOI[0][T0HitCt++] = PointOfIntersection;	
			}
			if (Internal_RaycastBidirectional(T0.B, T0.C, T1, PointOfIntersection)) {
				TruePOI[0][T0HitCt++] = PointOfIntersection;	
			}
			if (T0HitCt < 2) {
				if (Internal_RaycastBidirectional(T0.C, T0.A, T1, PointOfIntersection)) {
					TruePOI[0][T0HitCt++] = PointOfIntersection;	
				}
				if(T0HitCt < 2 && Internal_RaycastBidirectional(T1.A, T1.B, T0, PointOfIntersection)) {
					TruePOI[1][T1HitCt++] = PointOfIntersection;	
				}
				if(T0HitCt + T1HitCt < 2 && Internal_RaycastBidirectional(T1.B, T1.C, T0, PointOfIntersection)) {
					TruePOI[1][T1HitCt++] = PointOfIntersection;	
				}
				if(T0HitCt + T1HitCt < 2 && Internal_RaycastBidirectional(T1.C, T1.A, T0, PointOfIntersection)) {
					TruePOI[1][T1HitCt++] = PointOfIntersection;	
				}	
			}
				
			if (T0HitCt == 1 && T1HitCt == 1) {
				const PolyEdge P(TruePOI[0][0], TruePOI[1][0]);
				PolyA.Edges.Add(P);
				PolyB.Edges.Add(P);
			}
			else if (T0HitCt == 2) {
				const PolyEdge P(TruePOI[0][0], TruePOI[0][1]);	
				PolyA.Edges.Add(P);
				PolyB.Edges.Add(P);
			}
			else if (T1HitCt == 2) {
				const PolyEdge P(TruePOI[1][0], TruePOI[1][1]);	
				PolyA.Edges.Add(P);
				PolyB.Edges.Add(P);
			}
		}

		void Internal_FindPolyEdges(
			const TriMesh& TMeshA,
			const TriMesh& TMeshB,
			TArray<UnstructuredPolygon>& UPolysA,
			TArray<UnstructuredPolygon>& UPolysB
		) {
			const TArray<Tri>& TrisA = TMeshA.Tris;
			const TArray<Tri>& TrisB = TMeshB.Tris;
			for (int i = 0; i < TrisA.Num(); i++) {
				const Tri& T0 = TrisA[i];
				UnstructuredPolygon& PolyA = UPolysA[i];
				for (int j = 0; j < TrisB.Num(); j++) {
					const Tri& T1 = TrisB[j];
					const float Dist = FVector::Dist(T0.A, T1.A);
					if (Dist <= T0.LongestSidelen + T1.LongestSidelen) {
						UnstructuredPolygon& PolyB = UPolysB[j];
						Internal_GetTriPairPolyEdges(T0, T1, PolyA, PolyB);
					}
				}
			}
		}
	}

	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor) {
		const FBox MeshFBox = MeshActor->GetStaticMeshComponent()->GetStaticMesh()->GetBoundingBox();
		const FVector Extent = MeshFBox.GetExtent();
		const FVector Center = MeshFBox.GetCenter();
		const FTransform TForm = MeshActor->GetTransform();
		Internal_SetBoundingBox(BBox, Extent, TForm, Center, true);
	}

	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp) {
		const FVector Extent = BoxCmp->GetScaledBoxExtent();
		const FTransform TForm = BoxCmp->GetComponentTransform();
		Internal_SetBoundingBox(BBox, Extent, TForm, FVector::ZeroVector, false);	
	}

	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB) {
		for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
			if (Internal_IsPointInsideBox(BBoxA, BBoxB.Vertices[i])) {
				return true;
			}
		}
		for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
			if (Internal_IsPointInsideBox(BBoxB, BBoxA.Vertices[i])) {
				return true;
			}
		}
		return Internal_DoBoundingBoxesIntersect(BBoxA, BBoxB);
	}

	// Tests along all tri edges for intersections, but only those with indices in PotentialIntersectionIndices
	bool GetTriMeshIntersectGroups(
		TArray<int>& IntersectIndices,
		const TArray<int>& PotentialIntersectIndices,
		const TriMesh& TMesh,
		const TArray<TriMesh>& GroupTMeshes
	) {
		const int PotentialIntersectCt = PotentialIntersectIndices.Num();
		int IntersectionCt = 0;
		for (int i = 0; i < PotentialIntersectCt; i++) {
			const int& PotentialIntersectIndex = PotentialIntersectIndices[i];
			const TriMesh& OtherTMesh = GroupTMeshes[PotentialIntersectIndex];
			if (Internal_DoTriMeshesIntersect(TMesh, OtherTMesh)) {
				IntersectIndices.Add(PotentialIntersectIndex);
				if (++IntersectionCt == PotentialIntersectCt) {
					return true;
				}
			}
		}
		if (IntersectionCt > 0) {
			return true;
		}
		return false;
	}
	
	void GetGroupExtrema(TArray<TriMesh*> TMeshes, FVector& Min, FVector& Max, bool NudgeOutward) {
		const int TMeshCt = TMeshes.Num();
		if (TMeshCt == 0) {
			return;
		}
		// allows using if/else in loop
		Min = TMeshes[0]->Box.Vertices[0];
		Max = Min;
		for (int i = 0; i < TMeshCt; i++) {
			const FVector* BoxVerts = TMeshes[i]->Box.Vertices;
			for (int j = 0; j < BoundingBox::VERTEX_CT; j++) {
				const FVector& Vertex = BoxVerts[j];
				if (Vertex.X < Min.X) {
					Min.X = Vertex.X;
				}
				else if (Vertex.X > Max.X) {
					Max.X = Vertex.X;
				}
				if (Vertex.Y < Min.Y) {
					Min.Y = Vertex.Y;
				}
				else if (Vertex.Y > Max.Y) {
					Max.Y = Vertex.Y;
				}
				if (Vertex.Z < Min.Z) {
					Min.Z = Vertex.Z;
				}
				else if (Vertex.Z > Max.Z) {
					Max.Z = Vertex.Z;
				}
			}
		}
		if (NudgeOutward) {
			Max += FVector::OneVector;
			Min -= FVector::OneVector;
		}
	}

	void FlagObscuredTris(
		const UWorld* World,
		TriMesh& TMesh,
		const TArray<TriMesh*>& OtherTMeshes,
		const FVector& GroupExtMin 
	) {
		MeshHitCounter MHitCtr(OtherTMeshes);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(TMesh.MeshActor);
		Params.bIgnoreBlocks = true;
		Params.bTraceComplex = true;
		const float MinZ = GroupExtMin.Z;

		TArray<Tri>& Tris = TMesh.Tris;
		for (int i = 0; i < Tris.Num(); i++) {
			// flags obscured vertices on tris, ignoring t/f output
			Internal_IsTriObscured(World, Params, Tris[i], MinZ, MHitCtr);
		}
	}

	void PopulateUnstructuredPolygons(TArray<TriMesh*>& Group, TArray<TArray<UnstructuredPolygon>>& GroupUPolys) {
		const int GroupCt = Group.Num();
		for (int i = 0; i < GroupCt; i++) {
			GroupUPolys.Add(TArray<UnstructuredPolygon>());
			TArray<UnstructuredPolygon>& UPolys = GroupUPolys[i];
			// creating all of 
			UPolys.Init(UnstructuredPolygon(), Group[i]->Tris.Num());
		}
		for (int i = 0; i < GroupCt - 1; i++) {
			const TriMesh& TMeshA = *Group[i];
			TArray<UnstructuredPolygon>& UPolysA = GroupUPolys[i];
			for (int j = i + 1; j < GroupCt; j++) {
				const TriMesh& TMeshB = *Group[j];
				TArray<UnstructuredPolygon>& UPolysB = GroupUPolys[j];
				Internal_FindPolyEdges(TMeshA, TMeshB, UPolysA, UPolysB);	
			}
		}	
	}


}
