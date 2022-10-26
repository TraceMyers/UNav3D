#include "Geometry.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"

namespace Geometry {

	namespace {

		// The unscaled extents of a bounding box
		const FVector BaseExtent[RECT_PRISM_PTS] {
			FVector(-1.0f, -1.0f, -1.0f), // neighbors 1, 2, 3
			FVector( 1.0f, -1.0f, -1.0f),
			FVector(-1.0f,  1.0f, -1.0f),
			FVector(-1.0f, -1.0f,  1.0f),
			FVector( 1.0f,  1.0f, -1.0f), // neighbors 1, 2, 7
			FVector( 1.0f, -1.0f,  1.0f), // neighbors 1, 3, 7
			FVector(-1.0f,  1.0f,  1.0f), // neighbors 2, 3, 7
			FVector( 1.0f,  1.0f,  1.0f)
		};
		
		constexpr int BBoxFaceIndices[RECT_PRISM_FACES][3] {
			{0, 1, 2}, {0, 1, 3}, {0, 2, 3},
			{7, 4, 5}, {7, 5, 6}, {7, 4, 6}
		};

		constexpr int BBoxEdgeIndices[RECT_PRISM_EDGES][2] {
			{0, 1}, {0, 2}, {1, 4}, {2, 4},
			{3, 5}, {3, 6}, {7, 5}, {7, 6},
			{1, 5}, {4, 7}, {2, 6}, {0, 3}
		};

		enum TRI_FLAGS {
			PT_A_OBSCURED = 0x0001,
			PT_B_OBSCURED = 0x0002,
			PT_C_OBSCURED = 0x0004
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

		// used for counting hits through a point to check if a point lies between mesh hits
		// data meant to be populated only at constructor
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

			bool IsOdd(const AStaticMeshActor* MeshActor) {
				return MeshToCt[MeshActor] % 2 == 1;
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
			
			TMap<AStaticMeshActor*, int> MeshToCt;
			TArray<AStaticMeshActor*> Keys;
			
		};	
		
		// populate BBox with vertices and precomputed overlap-checking values
		void Internal_SetBoundingBox(
			BoundingBox& BBox,
			const FVector& Extent,
			const FTransform& TForm, 
			const FVector& Center,
			bool IsStaticMesh
		) {
			if (IsStaticMesh) {
				for (int i = 0; i < RECT_PRISM_PTS; i++) {
					const FVector RotatedCenter = TForm.TransformVector(Center);
					BBox.Vertices[i] = TForm.TransformPosition(BaseExtent[i] * Extent) + RotatedCenter;
				}
			}
			else {
				for (int i = 0; i < RECT_PRISM_PTS; i++) {
					BBox.Vertices[i] = TForm.TransformPositionNoScale(BaseExtent[i] * Extent);
				}
			}
			for (int i = 0; i < RECT_PRISM_FACES; i++) {
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
		
		bool Internal_DoesPointTouchTri(const Tri& T, const FVector& P) {
			return (
				Tri::GetArea(P, T.A, T.B) + Tri::GetArea(P, T.B, T.C) + Tri::GetArea(P, T.C, T.A)
				<= T.Area + NEAR_EPSILON
			);
		}

		// assumes pt has been projected onto plane with vertices
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

		bool Internal_Raycast(
			const FVector& Origin,
			const FVector& Dir,
			float Length,
			const FVector& PlanePtA,
			const FVector& PlanePtB,
			const FVector& PlanePtC,
			const FVector& Normal, // could get here, but currently precomputed for BoundingBox
			FVector& POI
		) {
			const FVector PVec = Origin - PlanePtA;
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
			return Internal_DoesPointTouchFace(PlanePtA, PlanePtB, PlanePtC, POI);
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
			const FVector PVec = Origin - T.Center;
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
				for (int j = 0; j < RECT_PRISM_FACES; j++) {
					const int* FaceIndices = BBoxFaceIndices[j];
					const FVector& FaceA = BBox.Vertices[FaceIndices[0]];
					const FVector& FaceB = BBox.Vertices[FaceIndices[1]];
					const FVector& FaceC = BBox.Vertices[FaceIndices[2]];
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

		bool Internal_DoesLineSegmentIntersectTri(const FVector& LSA, const FVector& LSB, const Tri& T) {
			FVector PointOfIntersection;
			FVector Dir = LSB - LSA;
			const float Length = Dir.Size();
			Dir *= 1.0f / Length;
			if (Internal_Raycast(LSA, Dir, Length, T, PointOfIntersection)) {
				return true;
			}
			Dir = -Dir;
			if (Internal_Raycast(LSB, Dir, Length, T, PointOfIntersection)) {
				return true;
			}
			return false;
		}

		bool Internal_DoBoundingBoxesIntersect(const BoundingBox& BBoxA, const BoundingBox& BBoxB) {
			const BoundingBox* EdgesBoxPtr = &BBoxA;
			const BoundingBox* FacesBoxPtr = &BBoxB;
			for (int i = 0; i < 2; i++) {
				const BoundingBox& EdgesBox = *EdgesBoxPtr;
				const BoundingBox& FacesBox = *FacesBoxPtr;
				for (int j = 0; j < RECT_PRISM_EDGES; j++) {
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
			for (int i = 0; i < TMeshATris.Num(); i++) {
				const Tri& T0 = TMeshATris[i];
				for (int j = 0; j < TMeshBTriCt; j++) {
					const Tri& T1 = TMeshBTris[j];
					// checking if it's possible they could intersect
					const float Dist = FVector::Dist(T0.Center, T1.Center);
					if (
						(Dist < T0.LongestSidelen + T1.LongestSidelen)
						&& (
							Internal_DoesLineSegmentIntersectTri(T0.A, T0.B, T1)
							|| Internal_DoesLineSegmentIntersectTri(T0.B, T0.C, T1)
							|| Internal_DoesLineSegmentIntersectTri(T0.C, T0.A, T1)
							|| Internal_DoesLineSegmentIntersectTri(T1.A, T1.B, T0)
							|| Internal_DoesLineSegmentIntersectTri(T1.B, T1.C, T0)
							|| Internal_DoesLineSegmentIntersectTri(T1.C, T1.A, T0)
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
			float MaxZ,
			MeshHitCounter& MHitCtr
		) {
			TArray<MeshHit> MHits;

			// trace through Pt along z axis (could be any axis, but z might have least superfluous hits) both ways 
			const FVector TrPtA (Pt.X, Pt.Y, MinZ);
			const FVector TrPtB (Pt.X, Pt.Y, MaxZ);
			Internal_LineTraceThrough(World, Params, TrPtA, TrPtB, MHits);
			Internal_LineTraceThrough(World, Params, TrPtB, TrPtA, MHits);

			// sort by z
			MHits.Sort(MeshHit::ZCmp);
			
			// count through sorted mesh hits
			MHitCtr.Reset();
			for (int i = 0; i < MHits.Num(); i++) {
				const MeshHit& MHit = MHits[i];
				if (MHit.Location.X > Pt.X) {
					// once we've gone past the point, any odd mesh hit cts mean pt is enclosed
					return MHitCtr.AnyOdd();
				}
				MHitCtr.Add(MHit.Mesh);
			}
			return MHitCtr.AnyOdd(); // may not be necessary; could potentially just return false
		}
		
		bool Internal_IsTriObscured(
			const UWorld* World,
			const FCollisionQueryParams& Params,
			const Tri& T,
			float MinZ,
			float MaxZ,
			MeshHitCounter& MHitCtr
		) {
			if (
				Internal_IsPointObscured(World, Params, T.A, MinZ, MaxZ, MHitCtr)
				|| Internal_IsPointObscured(World, Params, T.B, MinZ, MaxZ, MHitCtr)
				|| Internal_IsPointObscured(World, Params, T.C,MinZ, MaxZ, MHitCtr)
			) {
				return true;	
			}
			return false;	
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
		for (int i = 0; i < RECT_PRISM_PTS; i++) {
			if (Internal_IsPointInsideBox(BBoxA, BBoxB.Vertices[i])) {
				return true;
			}
		}
		for (int i = 0; i < RECT_PRISM_PTS; i++) {
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
		Max = TMeshes[0]->Box.Vertices[0];
		for (int i = 0; i < TMeshCt; i++) {
			const FVector* BoxVerts = TMeshes[i]->Box.Vertices;
			for (int j = 0; j < RECT_PRISM_PTS; j++) {
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

	void GetObscuredTris(
		UWorld* World,
		TArray<Tri*> Obscured,
		const TriMesh& TMesh,
		const TArray<TriMesh*>& OtherTMeshes,
		const FVector& GroupExtMin, // group bounding box extrema
		const FVector& GroupExtMax
	) {
		MeshHitCounter MHitCtr(OtherTMeshes);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(TMesh.MeshActor);
		Params.bIgnoreBlocks = true;
		Params.bTraceComplex = true;
	}


}
