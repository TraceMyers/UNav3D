#include "Geometry.h"
#include "Data.h"
#include "Debug.h"
#include "DoubleVector.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"
#include "SelectionSet.h"
#include "UNavMesh.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"

namespace Geometry {
	
	static constexpr float ONE_THIRD = 1.0f / 3.0f;
	static constexpr float NEAR_EPSILON = 1e-2f;
	static constexpr float GLANCE_EPSILON = 1e-4f;
	static constexpr float NEAR_FACTOR = 1.0f + 1e-4f;

	// -----------------------------------------------------------------------------------------------------------------
	// -------------------------------------------------------------------------------------------------------- Internal
	// -----------------------------------------------------------------------------------------------------------------
	
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
			
			MeshHit() {}
			
			MeshHit(const AStaticMeshActor* _Mesh, const FVector& _Location, const FVector & _Normal, float _Distance) :
				Mesh{_Mesh}, Location(_Location), Normal(_Normal), Distance(_Distance)
			{}

			void SetData(const AStaticMeshActor* _Mesh, const FVector& Loc, const FVector& _Normal, float Dist) {
				Mesh = _Mesh;
				Location = Loc;
				Normal = _Normal;
				Distance = Dist;
			}
			
			static bool DistCmp(const MeshHit& A, const MeshHit& B) {
				return A.Distance < B.Distance;
			}
			
			static bool DistCmp2(const MeshHit& A, const MeshHit& B) {
				return A.Distance > B.Distance;
			}
	
			const AStaticMeshActor* Mesh;
			FVector Location;
			FVector Normal;
			float Distance;
		};

		// used for counting mesh hits up to a point to check if a point lies between mesh hits.
		// mesh data only populated only at constructor
		struct MeshHitCounter {

			// FudgeBVCt: on reset, if BoundsVolumeTMesh is in the keys, sets it to 1 instead of 0 so
			// 'inside' becomes 'outside'
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

		void Internal_SetBBoxAfterVertices(BoundingBox& BBox) {
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
			BBox.DiagDistance = FVector::Dist(BBox.Vertices[0], BBox.Vertices[7]);
		}

		void Internal_SetBoundingBoxFromExtrema(
			BoundingBox& BBox,
			const FVector& Min,
			const FVector& Max
		) {
			BBox.Vertices[0] = Min;
			BBox.Vertices[1] = FVector(Max.X, Min.Y, Min.Z);
			BBox.Vertices[2] = FVector(Min.X, Max.Y, Min.Z);
			BBox.Vertices[3] = FVector(Min.X, Min.Y, Max.Z);
			BBox.Vertices[4] = FVector(Max.X, Max.Y, Min.Z);
			BBox.Vertices[5] = FVector(Max.X, Min.Y, Max.Z);
			BBox.Vertices[6] = FVector(Min.X, Max.Y, Max.Z);
			BBox.Vertices[7] = Max;
			Internal_SetBBoxAfterVertices(BBox);
		}
		
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
			Internal_SetBBoxAfterVertices(BBox);
		}

		FVector Internal_UnitNormal(const FVector& A, const FVector& B, const FVector& C) {
			return FVector::CrossProduct(B - A, C - B).GetUnsafeNormal();
		}

		// Does NOT require assumption that P has been projected onto T's plane
		// clever trick: assume the pt is inside the triangle. then, the pt makes three triangles - one with every pair
		// of tri vertices. the sum of those areas is equal to the area of the triangle.
		bool Internal_DoesPointTouchTri(const Tri& T, const FVector& P) {
			// return (
			// 	Tri::GetArea(P, T.A, T.B) + Tri::GetArea(P, T.B, T.C) + Tri::GetArea(P, T.C, T.A)
			// 	<= T.Area * NEAR_FACTOR
			// );
			const FVector N1 = Internal_UnitNormal(T.A, T.B, P);	
			const FVector N2 = Internal_UnitNormal(T.B, T.C, P);
			if (FVector::DotProduct(N1, N2) < 1.0f - 1e-2f) {
				return false;
			}
			const FVector N3 = Internal_UnitNormal(T.C, T.A, P);
			if (FVector::DotProduct(N2, N3) < 1.0f - 1e-2f) {
				return false;
			}
			return true;
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
			FVector& POI,
			float& HitDistance
		) {
			const FVector PVec = Origin - FacePtA;
			const double LenOProj = DoubleVector::DotProduct(Normal, PVec);
			if (LenOProj <= 0.0) {
				return false;
			}
			const double RayNormCosTheta = DoubleVector::DotProduct(Dir, -Normal);
			if (RayNormCosTheta <= 0.0) {
				return false;
			}
			HitDistance = LenOProj / RayNormCosTheta;
			if (HitDistance > Length) {
				return false;
			}
			POI = Origin + Dir * HitDistance;
			return Internal_DoesPointTouchFace(FacePtA, FacePtB, FacePtC, POI);
		}

		// thinks in triangles: the one made by the tri plane, origin -> center, origin -> projection
		// and the one made by the tri plane, origin -> projection, origin -> ray hit. Could be faster, I'm sure.
		// TODO: check if this could be modified to check in both directions
		bool Internal_Raycast(
			const FVector& Origin,
			const FVector& Dir,
			float Length,
			const Tri& T,
			FVector& POI,
			float& HitDistance
		) {
			const FVector PVec = Origin - T.A;
			const float LenOProj = FVector::DotProduct(T.Normal, PVec);
			if (LenOProj <= 0.0f) {
				return false;
			}
			const float RayNormCosTheta = FVector::DotProduct(Dir, -T.Normal);
			if (RayNormCosTheta <= 0.0f) {
				return false;
			}
			HitDistance = LenOProj / RayNormCosTheta;
			if (HitDistance > Length) {
				return false;
			}
			POI = Origin + Dir * HitDistance;
			return Internal_DoesPointTouchTri(T, POI);
		}
		
		bool Internal_DoesLineSegmentIntersectBox(
			const FVector& LSA,
			const FVector& LSB,
			const BoundingBox& BBox,
			FVector& PointOfIntersection,
			float& HitDistance
		) {
			FVector Dir = LSB - LSA;
			const float Length = Dir.Size();
			Dir *= 1.0f / Length;
			const FVector* Origin = &LSA;
			// checking all lsa -> lsb first rather than alternating because: if the intersection exists, and if you do
			// IsPointInsideBox() checks before this, checking in either direction will return a positive, so in
			// those cases each alternating check in the opposite direction is superfluous
			for (int j = 0; j < BoundingBox::FACE_CT; j++) {
				const int* FaceIndices = BBoxFaceIndices[j];
				const FVector& FaceA = BBox.Vertices[FaceIndices[0]];
				const FVector& FaceB = BBox.Vertices[FaceIndices[1]];
				const FVector& FaceC = BBox.Vertices[FaceIndices[2]];
				
				if (Internal_Raycast(
					*Origin,
					Dir,
					Length,
					FaceA,
					FaceB,
					FaceC,
					BBox.FaceNormals[j],
					PointOfIntersection,
					HitDistance
				)) {
					return true;
				}
			}
			Dir = -Dir;
			Origin = &LSB;
			for (int j = 0; j < BoundingBox::FACE_CT; j++) {
				const int* FaceIndices = BBoxFaceIndices[j];
				const FVector& FaceA = BBox.Vertices[FaceIndices[0]];
				const FVector& FaceB = BBox.Vertices[FaceIndices[1]];
				const FVector& FaceC = BBox.Vertices[FaceIndices[2]];
				
				if (Internal_Raycast(
					*Origin,
					Dir,
					Length,
					FaceA,
					FaceB,
					FaceC,
					BBox.FaceNormals[j],
					PointOfIntersection,
					HitDistance
				)) {
					HitDistance = Length - HitDistance;
					return true;
				}
			}
			return false;
		}

		RAY_HIT Internal_TriLineTrace(
			const FVector& LSA,
			const FVector& LSB,
			const Tri& T,
			FVector& PointOfIntersection,
			float& HitDistance
		) {
			FVector Dir = LSB - LSA;
			const float Length = Dir.Size();
			Dir *= 1.0f / Length;
			if (Internal_Raycast(LSA, Dir, Length, T, PointOfIntersection, HitDistance)) {
				return RAY_HIT_ATOB;
			}
			Dir = -Dir;
			if (Internal_Raycast(LSB, Dir, Length, T, PointOfIntersection, HitDistance)) {
				return RAY_HIT_BTOA;
			}
			return RAY_HIT_NONE;
		}

		bool Internal_DoBoundingBoxesIntersect(const BoundingBox& BBoxA, const BoundingBox& BBoxB, bool BothWays=false) {
			const BoundingBox* EdgesBoxPtr = &BBoxA;
			const BoundingBox* FacesBoxPtr = &BBoxB;
			FVector POI;
			float HitDist;
			for (int i = 0; i < 2; i++) {
				const BoundingBox& EdgesBox = *EdgesBoxPtr;
				const BoundingBox& FacesBox = *FacesBoxPtr;
				for (int j = 0; j < BoundingBox::EDGE_CT; j++) {
					const int* EdgeIndices = BBoxEdgeIndices[j];
					const FVector EdgeA = EdgesBox.Vertices[EdgeIndices[0]];
					const FVector EdgeB = EdgesBox.Vertices[EdgeIndices[1]];
					if (Internal_DoesLineSegmentIntersectBox(
						EdgeA, EdgeB, FacesBox, POI, HitDist
					)) {
						return true;
					}
					// if a test for whether either has points inside each other has already been done,
					// this is unnecessary, since an intersection would only occur if an edge passed through a box,
					// making that edge checkable in either direction
					if (
						BothWays 
						&& Internal_DoesLineSegmentIntersectBox(
							EdgeB, EdgeA, FacesBox, POI, HitDist
					)) {
						return true;
					}
				}
				EdgesBoxPtr = &BBoxB;
				FacesBoxPtr = &BBoxA;
			}
			return false;
		}

		bool Internal_DoTriMeshesIntersect(const TriMesh& TMeshA, const TriMesh& TMeshB) {
			const auto& TMeshATris = TMeshA.Grid;
			const auto& TMeshBTris = TMeshB.Grid;
			const int TMeshBTriCt = TMeshBTris.Num();
			FVector POI;
			float HitDistance;
			for (int i = 0; i < TMeshATris.Num(); i++) {
				const Tri& T0 = TMeshATris[i];
				for (int j = 0; j < TMeshBTriCt; j++) {
					const Tri& T1 = TMeshBTris[j];
					// checking if it's possible they could intersect
					const float DistSq = FVector::DistSquared(T0.A, T1.A);
					if (
						(DistSq < T0.LongestSidelenSq + T1.LongestSidelenSq)
						&& (
							Internal_TriLineTrace(T0.A, T0.B, T1, POI, HitDistance)
							|| Internal_TriLineTrace(T0.B, T0.C, T1, POI, HitDistance)
							|| Internal_TriLineTrace(T0.C, T0.A, T1, POI, HitDistance)
							|| Internal_TriLineTrace(T1.A, T1.B, T0, POI, HitDistance)
							|| Internal_TriLineTrace(T1.B, T1.C, T0, POI, HitDistance)
							|| Internal_TriLineTrace(T1.C, T1.A, T0, POI, HitDistance)
						)
					) {
						return true;
					}
				}
			}
			return false;
		}
		

		// equivalent of LineTraceMulti, but targets only provided TriMeshes, goes both directions, and doesn't
		// ignore an actor after one overlap/hit. currently significantly slower but has the needed features.
		void Internal_LineTraceThrough(
			const FVector& TrStart,
			const FVector& TrEnd,
			const TArray<TriMesh*>& TriMeshes,
			TArray<MeshHit>& MHits
		) {
			const float TraceDistance = FVector::Dist(TrStart, TrEnd);
			float HitDistance;
			FVector PointOfIntersection;
			FVector Dir = TrEnd - TrStart;
			const float Length = Dir.Size();
			Dir *= 1 / Length;
			const FVector OppDir = -Dir;
			for (const TriMesh* TMesh : TriMeshes) {
				const AStaticMeshActor* MeshActor = TMesh->MeshActor;
				const auto& Tris = TMesh->Grid;
				for (int i = 0; i < Tris.Num(); i++) {
					const Tri& T = Tris[i];
					if (Internal_Raycast(TrStart, Dir, Length, T, PointOfIntersection, HitDistance)) {
						MHits.Add(MeshHit(MeshActor, PointOfIntersection, T.Normal, HitDistance));
					}
					if (Internal_Raycast(TrEnd, OppDir, Length, T, PointOfIntersection, HitDistance)) {
						HitDistance = TraceDistance - HitDistance;
						MHits.Add(MeshHit(MeshActor, PointOfIntersection, T.Normal, HitDistance));
					}
				}
			}
		}

		// Single mesh, single hit test version of other implementation
		bool Internal_LineTraceThrough(
			const FVector& TrStart,
			const FVector& TrEnd,
			const TriMesh& TMesh,
			MeshHit& MHit
		) {
			const float TraceDistance = FVector::Dist(TrStart, TrEnd);
			float HitDistance;
			FVector PointOfIntersection;
			FVector Dir = TrEnd - TrStart;
			const float Length = Dir.Size();
			Dir *= 1 / Length;
			const FVector OppDir = -Dir;
			const AStaticMeshActor* MeshActor = TMesh.MeshActor;
			const auto& Tris = TMesh.Grid;
			for (int i = 0; i < Tris.Num(); i++) {
				const Tri& T = Tris[i];
				if (Internal_Raycast(TrStart, Dir, Length, T, PointOfIntersection, HitDistance)) {
					MHit.SetData(MeshActor, PointOfIntersection, T.Normal, HitDistance);
					return true;
				}
				if (Internal_Raycast(TrEnd, OppDir, Length, T, PointOfIntersection, HitDistance)) {
					HitDistance = TraceDistance - HitDistance;
					MHit.SetData(MeshActor, PointOfIntersection, T.Normal, HitDistance);
					return true;
				}
			}
			return false;
		}
		
		bool Internal_IsPointObscured(
			const FVector& Pt,
			const TArray<TriMesh*>& OtherMeshes,
			float MinZ,
			MeshHitCounter& MHitCtr
		) {
			TArray<MeshHit> MHits;

			// trace through Pt along z axis (could be any axis, but z might have least superfluous hits) both ways 
			const FVector TrStart (Pt.X, Pt.Y, MinZ);
			Internal_LineTraceThrough(TrStart, Pt, OtherMeshes, MHits);

			// sort the hits by distance
			MHits.Sort(MeshHit::DistCmp);
			
			// count through sorted mesh hits
			MHitCtr.Reset();
			for (int i = 0; i < MHits.Num(); i++) {
				MHitCtr.Add(MHits[i].Mesh);
			}
			// once we've gone to the point, any odd mesh hit cts mean pt is enclosed
			return MHitCtr.AnyOdd();
		}

		// traces along line going through A, ending at B, marking distances from A (only those between A and B)
		// where the line segment is inside other meshes and where it's outside other meshes; returns true
		// if A is enclosed. If there are an even number of distances (including 0), B's enclosed status
		// is the same as A, otherwise opposite
		// FudgeLast purposefully miscounts the number of hits with the last mesh by 1; useful for boundsvolume collision
		bool Internal_GetObscuredDistances(
			const FVector& A,
			const FVector& B,
			const TArray<TriMesh*>& OtherMeshes,
			TArray<FVector>& ObscuredLocations,
			float BBoxDiagDistance,
			MeshHitCounter& MHitCtr
		) {
			FVector TrDir = (A - B);
			const float ABLen = TrDir.Size();
			TrDir *= 1.0f / ABLen;
			const FVector TrEnd = A + TrDir * BBoxDiagDistance;
			TArray<MeshHit> MHits;
			// trace both directions, from outside the box to B and vice-versa, getting overlaps
			Internal_LineTraceThrough(B, TrEnd, OtherMeshes, MHits);
			
			// sort by distance, farthest to shortest
			MHits.Sort(MeshHit::DistCmp2);

			MHitCtr.Reset();
			if (OtherMeshes.Find(&Data::BoundsVolumeTMesh) != -1 && !IsPointInsideBox(Data::BoundsVolumeTMesh.Box, TrEnd)) {
				MHitCtr.Add(Data::BoundsVolumeTMesh.MeshActor);		
			}
			
			bool PassedA = false;
			bool AEnclosed = false;
			bool CurrentlyInsideMesh = false;
			for (int i = 0; i < MHits.Num(); i++) {
				// iterate over hits from point outside box to B, through A
				const MeshHit& MHit = MHits[i];
				
				if (!PassedA) {
					if (MHit.Distance < ABLen) {
						// we've now passed pt A and want to start keeping track of distances
						PassedA = true;
						// if we're inside a mesh...
						if (MHitCtr.AnyOdd()) {
							// note that A is inside a mesh
							AEnclosed = true;
							MHitCtr.Add(MHit.Mesh);
							if (MHitCtr.AnyOdd()) {
								CurrentlyInsideMesh = true;
							}
							else {
								ObscuredLocations.Add(MHit.Location);	
							}
						}
						else {
							// hits are all even
							MHitCtr.Add(MHit.Mesh);
							// must now be odd: if hits were even, adding another hit will definitely make a count odd
							CurrentlyInsideMesh = true;
							ObscuredLocations.Add(MHit.Location);	
						}
					}
					else {
						MHitCtr.Add(MHit.Mesh);
					}
				}
				else {
					// keep track of any distance from pt A where we pass in or out of all other meshes
					MHitCtr.Add(MHit.Mesh);
					if (CurrentlyInsideMesh) {
						if (!MHitCtr.AnyOdd()) {
							ObscuredLocations.Add(MHit.Location);	
							CurrentlyInsideMesh = false;
						}	
					}
					else if (MHitCtr.AnyOdd()) {
						ObscuredLocations.Add(MHit.Location);	
						CurrentlyInsideMesh = true;
					}
				}
			}
			// if no hits were registered between A and B, return whether A (and B) is enclosed
			if (!PassedA) {
				return MHitCtr.AnyOdd();
			}
			return AEnclosed;
		}
		
		// params should be set to complex and to ignore the mesh of the given tri; does NOT clear flags beforehand
		bool Internal_IsTriObscured(
			Tri& T,
			const TArray<TriMesh*>& OtherMeshes,
			float MinZ,
			MeshHitCounter& MHitCtr
		) {
			if (Internal_IsPointObscured(T.A, OtherMeshes, MinZ, MHitCtr)) {
				T.SetAObscured();
			}
			if (Internal_IsPointObscured(T.B, OtherMeshes, MinZ, MHitCtr)) {
				T.SetBObscured();
			}
			if (Internal_IsPointObscured(T.C, OtherMeshes, MinZ, MHitCtr)) {
				T.SetCObscured();
			}
			return T.AnyObscured();
		}

		// if exists, find an intersection between tris, add PolyEdge to both polys
		// NOTE: NOT using flags! (since they're currently unused)
		bool Internal_GetTriPairPolyEdge(
			const Tri& T0,
			const Tri& T1,
			UnstructuredPolygon& PolyA,
			UnstructuredPolygon& PolyB
		) {
			
			FVector PointOfIntersection;
			FVector TruePOI[2];
			int HitCt = 0;
			uint16 TriEdgeFlags = 0x0;
			float HitDistance;
			
			if (Internal_TriLineTrace(T0.A, T0.B, T1, PointOfIntersection, HitDistance)) {
				TruePOI[HitCt] = PointOfIntersection;
				++HitCt;
			}
			if (Internal_TriLineTrace(T0.B, T0.C, T1, PointOfIntersection, HitDistance)) {
				TruePOI[HitCt] = PointOfIntersection;
				if (++HitCt == 2) {
					goto HIT_CT_2;
				}
			}
			if (Internal_TriLineTrace(T0.C, T0.A, T1, PointOfIntersection, HitDistance)) {
				TruePOI[HitCt] = PointOfIntersection;
				if (++HitCt == 2) {
					goto HIT_CT_2;
				}
			}
			if(Internal_TriLineTrace(T1.A, T1.B, T0, PointOfIntersection, HitDistance)) {
				TruePOI[HitCt] = PointOfIntersection;
				if (++HitCt == 2) {
					goto HIT_CT_2;
				}
			}
			if(Internal_TriLineTrace(T1.B, T1.C, T0, PointOfIntersection, HitDistance)) {
				TruePOI[HitCt] = PointOfIntersection;
				if (++HitCt == 2) {
					goto HIT_CT_2;
				}
			}
			if(Internal_TriLineTrace(T1.C, T1.A, T0, PointOfIntersection, HitDistance)) {
				TruePOI[HitCt] = PointOfIntersection;
				if (++HitCt == 2) {
					goto HIT_CT_2;
				}
			}
			return false;

			// triangles can intersect each other in 3 ways:
			// - one of T0's edges intersects T1's center and one of T1's edges intersects T0's center
			// - two of T0's edges intersect T1's center
			// - two of T1's edges intersect T0's center
			HIT_CT_2:
			// could be using refs here, so polyedge could just store refs
			const PolyEdge P(TruePOI[0], TruePOI[1], TriEdgeFlags);
			PolyA.Edges.Add(P);
			PolyB.Edges.Add(P);
			return true;
		}

		// finds intersections between triangles on meshes, creating PolyEdges for use in building polygons
		void Internal_FindPolyEdges(
			const TriMesh& TMeshA,
			const TriMesh& TMeshB,
			const TArray<TriMesh*>& OtherMeshes,
			TArray<UnstructuredPolygon>& UPolysA,
			TArray<UnstructuredPolygon>& UPolysB,
			const float BBoxDiagDist
		) {
			const auto& TrisA = TMeshA.Grid;
			const auto& TrisB = TMeshB.Grid;
			MeshHitCounter MHitCtr(OtherMeshes);
			
			for (int i = 0; i < TrisA.Num(); i++) {
				const Tri& T0 = TrisA[i];
				if (T0.IsCull()) {
					continue;
				}
				
				UnstructuredPolygon& PolyA = UPolysA[i];
				for (int j = 0; j < TrisB.Num(); j++) {
					const Tri& T1 = TrisB[j];
					if (T1.IsCull()) {
						continue;
					}
					
					const float DistSq = FVector::DistSquared(T0.A, T1.A);
					if (DistSq < 0.5f * (T0.Area * T1.Area)) {
						UnstructuredPolygon& PolyB = UPolysB[j];
						
						// if an intersection between these triangles exists, put it in both polys
						if (Internal_GetTriPairPolyEdge(T0, T1, PolyA, PolyB)) {
							PolyEdge& PolyEdge0 = PolyA.Edges.Last();
							PolyEdge& PolyEdge1 = PolyB.Edges.Last();

							// check where the edge line segment is inside and outside all other meshes to help
							// with polygon creation
							const bool AEnclosed = Internal_GetObscuredDistances(
								PolyEdge0.A,
								PolyEdge0.B,
								OtherMeshes,
								PolyEdge0.ObscuredLocations,
								BBoxDiagDist,
								MHitCtr
							);
							if (AEnclosed) {
								PolyEdge0.SetAEnclosed();
								PolyEdge1.SetAEnclosed();
								// if A is enclosed and the inside-outside distance point ct is even, B is also enclosed
								if (PolyEdge0.ObscuredLocations.Num() % 2 == 0) {
									PolyEdge0.SetBEnclosed();
									PolyEdge1.SetBEnclosed();
								}
							}
							else if (PolyEdge0.ObscuredLocations.Num() % 2 == 1) {
								// if A is not enclosed and the distance point ct is odd, B is enclosed	
								PolyEdge0.SetBEnclosed();
								PolyEdge1.SetBEnclosed();
							}
							PolyEdge1.ObscuredLocations = PolyEdge0.ObscuredLocations;
						}
					}
				}
			}
		}

		bool Internal_GetBoundsVolumePolyEdge(
			Tri& T,
			UnstructuredPolygon& UPoly,
			const TArray<TriMesh*>& OtherMeshes,
			float BBoxDiagDistance,
			MeshHitCounter& MHitCtr
		) {
			const TriMesh& BV = Data::BoundsVolumeTMesh;
			MeshHit HitA;
			MeshHit HitB;
			bool DidHitA = false;
			bool DidHitB = false;
			PolyEdge* TEdgeA = nullptr;
			PolyEdge* TEdgeB = nullptr;
			const int CAIndex = UPoly.Edges.Num() - 1;
			if (T.IsAInsideBV()) {
				if (T.IsBInsideBV()) {
					// C enclosed
					DidHitA = Internal_LineTraceThrough(T.C, T.A, BV, HitA);
					DidHitB = Internal_LineTraceThrough(T.B, T.C, BV, HitB);
					TEdgeA = &UPoly.Edges[CAIndex];
					TEdgeB = &UPoly.Edges[CAIndex - 1];
				}
				else if (T.IsCInsideBV()) {
					// B enclosed
					DidHitA = Internal_LineTraceThrough(T.A, T.B, BV, HitA);
					DidHitB = Internal_LineTraceThrough(T.B, T.C, BV, HitB);
					TEdgeA = &UPoly.Edges[CAIndex - 2];
					TEdgeB = &UPoly.Edges[CAIndex - 1];
				}
				else {
					// B, C enclosed
					DidHitA = Internal_LineTraceThrough(T.A, T.B, BV, HitA);
					DidHitB = Internal_LineTraceThrough(T.C, T.A, BV, HitB);
					TEdgeA = &UPoly.Edges[CAIndex - 2];
					TEdgeB = &UPoly.Edges[CAIndex];
				}
			}
			else if (T.IsBInsideBV()) {
				if (T.IsCInsideBV()) {
					// A enclosed
					DidHitA = Internal_LineTraceThrough(T.A, T.B, BV, HitA);
					DidHitB = Internal_LineTraceThrough(T.C, T.A, BV, HitB);
					TEdgeA = &UPoly.Edges[CAIndex - 2];
					TEdgeB = &UPoly.Edges[CAIndex];
				}
				else {
					// C, A enclosed
					DidHitA = Internal_LineTraceThrough(T.A, T.B, BV, HitA);
					DidHitB = Internal_LineTraceThrough(T.B, T.C, BV, HitB);
					TEdgeA = &UPoly.Edges[CAIndex - 2];
					TEdgeB = &UPoly.Edges[CAIndex - 1];
				}
			}
			else if (T.IsCInsideBV()) {
				// A, B enclosed
				DidHitA = Internal_LineTraceThrough(T.C, T.A, BV, HitA);
				DidHitB = Internal_LineTraceThrough(T.B, T.C, BV, HitB);
				TEdgeA = &UPoly.Edges[CAIndex];
				TEdgeB = &UPoly.Edges[CAIndex - 1];
			}
			if (DidHitA && DidHitB) {
				const uint32 Flags = 0x0f;
				UPoly.Edges.Add(PolyEdge(HitA.Location, HitB.Location, Flags));
				PolyEdge& Edge = UPoly.Edges.Last();
				if (Internal_GetObscuredDistances(
					HitA.Location,
					HitB.Location,
					OtherMeshes,
					Edge.ObscuredLocations,
					BBoxDiagDistance,
					MHitCtr
				)) {
					Edge.SetAEnclosed();
				}
				// if this edge has a node that is enclosed, it shouldn't be added to the tri edge
				// otherwise, add it
				if (!Edge.IsAEnclosed()) {
					TEdgeA->ObscuredLocations.Add(HitA.Location);
				}
				if (!Edge.IsBEnclosed()) {
					TEdgeB->ObscuredLocations.Add(HitB.Location);
				}
				return true;
		}
			T.MarkForCull();
			return false;
		}

		void Internal_PopulatePolyEdgesFromTriEdges(
			const TriMesh& TMesh,
			const TArray<TriMesh*>& OtherMeshes,
			TArray<UnstructuredPolygon>& UPolys,
			float BBoxDiagDistance,
			float MinZ
		) {
			MeshHitCounter MHitCtr(OtherMeshes);
			const auto& TriGrid = TMesh.Grid;
			constexpr uint32 flags = PolyEdge::ON_EDGE_AB | PolyEdge::ON_EDGE_BC | PolyEdge::ON_EDGE_CA;
			for (int i = 0; i < TriGrid.Num(); i++) {
				Tri& T = TriGrid[i];
				UNavDbg::BreakOnVertexCaptureMatch(T);
				if (T.IsCull()) {
					// Tri already culled because it's outside of the bounds volume
					continue;
				}
				UnstructuredPolygon& UPoly = UPolys[i];
				if (UPoly.Edges.Num() == 0) {
					// if there are no intersections, just check if the tri points are inside other meshes
					Internal_IsTriObscured(T, OtherMeshes, MinZ, MHitCtr);
					continue;
				}
				PolyEdge PEdgeAB(T.A, T.B, flags);
				const bool AObscured = Internal_GetObscuredDistances(
					T.A, T.B, OtherMeshes, PEdgeAB.ObscuredLocations, BBoxDiagDistance, MHitCtr
				);
				PolyEdge PEdgeBC(T.B, T.C, flags);
				const bool BObscured = Internal_GetObscuredDistances(
					T.B, T.C, OtherMeshes, PEdgeBC.ObscuredLocations, BBoxDiagDistance, MHitCtr
				);
				PolyEdge PEdgeCA(T.C, T.A, flags);
				const bool CObscured = Internal_GetObscuredDistances(
					T.C, T.A, OtherMeshes, PEdgeCA.ObscuredLocations, BBoxDiagDistance, MHitCtr
				);
				TArray<PolyEdge>& Edges = UPoly.Edges;
				if (AObscured) {
					T.SetAObscured();
					PEdgeAB.SetAEnclosed();
					PEdgeCA.SetBEnclosed();
				}
				if (BObscured) {
					T.SetBObscured();
					PEdgeBC.SetAEnclosed();
					PEdgeAB.SetBEnclosed();
				}
				if (CObscured) {
					T.SetCObscured();
					PEdgeCA.SetAEnclosed();
					PEdgeBC.SetBEnclosed();
				}
				Edges.Add(PEdgeAB);
				Edges.Add(PEdgeBC);
				Edges.Add(PEdgeCA);

				if (!T.IsInsideBV()) {
					Internal_GetBoundsVolumePolyEdge(T, UPoly, OtherMeshes, BBoxDiagDistance, MHitCtr);
				}
			}
		}

		// as of 11/5/2022, no longer using built-in line tracing
		// set meshes to respond to built-in line traces; currently unused
		void Internal_SetMeshesOverlap(TArray<TriMesh>& TMeshes) {
			for (int i = 0; i < TMeshes.Num(); i++) {
				TMeshes[i].MeshActor->GetStaticMeshComponent()->SetCollisionResponseToChannel(
					ECC_GameTraceChannel1,
					ECR_Overlap
				);
			}
		}
		
		// as of 11/5/2022, no longer using built-in line tracing
		// set meshes to respond to built-in line traces; currently unused
		void Internal_SetMeshesOverlap(TArray<TriMesh*>& TMeshes) {
			for (int i = 0; i < TMeshes.Num(); i++) {
				TMeshes[i]->MeshActor->GetStaticMeshComponent()->SetCollisionResponseToChannel(
					ECC_GameTraceChannel1,
					ECR_Overlap
				);
			}
		}
		
		// as of 11/5/2022, no longer using built-in line tracing
		// set meshes to ignore built-in line traces; currently unused
		void Internal_SetMeshesIgnore(TArray<TriMesh>& TMeshes) {
			for (int i = 0; i < TMeshes.Num(); i++) {
				TMeshes[i].MeshActor->GetStaticMeshComponent()->SetCollisionResponseToChannel(
					ECC_GameTraceChannel1,
					ECR_Ignore
				);
			}
		}
		
		// as of 11/5/2022, no longer using built-in line tracing
		// set meshes to ignore built-in line traces; currently unused
		void Internal_SetMeshesIgnore(TArray<TriMesh*>& TMeshes) {
			for (int i = 0; i < TMeshes.Num(); i++) {
				TMeshes[i]->MeshActor->GetStaticMeshComponent()->SetCollisionResponseToChannel(
					ECC_GameTraceChannel1,
					ECR_Ignore
				);
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------
	// -------------------------------------------------------------------------------------------------------- External
	// -----------------------------------------------------------------------------------------------------------------

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

	void SetBoundingBox(UNavMesh& NMesh, const TArray<TriMesh*> Group) {
		FVector Min;
		FVector Max;
		GetGroupExtrema(Group, Min, Max);
		Internal_SetBoundingBoxFromExtrema(NMesh.Box, Min, Max);	
	}
	
	// If the points were all on a line, you would only need to check magnitude; implicit scaling by cos(theta) in
	// dot product does the work of checking in 3 dimensions
	// Ref: https://math.stackexchange.com/questions/1472049/check-if-a-point-is-inside-a-rectangular-shaped-area-3d
	bool IsPointInsideBox(const BoundingBox& BBox, const FVector& Point) {
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

	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB) {
		for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
			if (IsPointInsideBox(BBoxA, BBoxB.Vertices[i])) {
				return true;
			}
		}
		for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
			if (IsPointInsideBox(BBoxB, BBoxA.Vertices[i])) {
				return true;
			}
		}
		return Internal_DoBoundingBoxesIntersect(BBoxA, BBoxB);
	}

	bool IsBoxAInBoxB(const BoundingBox& BBoxA, const BoundingBox& BBoxB) {
		for (int i = 0; i < BoundingBox::VERTEX_CT; i++) {
			if (!IsPointInsideBox(BBoxB, BBoxA.Vertices[i])) {
				return false;
			}
		}
		return true;
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
	
	void GetGroupExtrema(const TArray<TriMesh*>& TMeshes, FVector& Min, FVector& Max, bool NudgeOutward) {
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

	void GetAxisAlignedExtrema(const BoundingBox& BBox, FVector& Min, FVector& Max, float NudgeOutward) {
		const FVector* BoxVerts = BBox.Vertices;
		Min = BoxVerts[0];
		Max = Min;
		for (int j = 1; j < BoundingBox::VERTEX_CT; j++) {
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
		if (NudgeOutward != 0.0f) {
			const FVector Nudge(NudgeOutward, NudgeOutward, NudgeOutward);
			Max += Nudge;
			Min -= Nudge;
		}
	}

	void FlagTrisOutsideBoxForCull(const BoundingBox& BBox, const TriMesh& TMesh) {
		const auto& Grid = TMesh.Grid;
		for (int i = 0; i < Grid.Num(); i++) {
			Tri& T = Grid[i];
			if (
				!IsPointInsideBox(BBox, T.A)
				&& !IsPointInsideBox(BBox, T.B)
				&& !IsPointInsideBox(BBox,T.C)
			) {
				T.MarkForCull();	
			}	
		}	
	}

	void FlagTriVerticesInsideBoundsVolume(const TriMesh& TMesh) {
		const auto& Grid = TMesh.Grid;
		const auto& BBox = Data::BoundsVolumeTMesh.Box;
		for (int i = 0; i < Grid.Num(); i++) {
			Tri& T = Grid[i];
			if (IsPointInsideBox(BBox, T.A)) {
				T.SetAInsideBV();
			}
			if (IsPointInsideBox(BBox, T.B)) {
				T.SetBInsideBV();
			}
			if (IsPointInsideBox(BBox, T.C)) {
				T.SetCInsideBV();
			}
			if (!T.AnyInsideBV()) {
				T.MarkForCull();
			}
		}
	}

	void FindIntersections(
		TArray<TriMesh*>& Group,
		TArray<TArray<UnstructuredPolygon>>& GroupUPolys,
		bool LastIsBoundsVolume
	) {
		FVector GroupBBoxMin;
		FVector GroupBBoxMax;
		if (LastIsBoundsVolume) {
			TArray<TriMesh*> ModifiedGroup = Group;
			ModifiedGroup.RemoveAtSwap(Group.Num() - 1);
			GetGroupExtrema(ModifiedGroup, GroupBBoxMin, GroupBBoxMax, true);
		}
		else {
			GetGroupExtrema(Group, GroupBBoxMin, GroupBBoxMax, true);
		}
		const float BBoxDiagDist = FVector::Dist(GroupBBoxMin, GroupBBoxMax);
		const float MinZ = GroupBBoxMin.Z;
		
		const int GroupCt = Group.Num();
		// find all intersections between tris in this group and mark where those intersections are inside
		// and where they are outside other meshes
		for (int i = 0; i < GroupCt - 1; i++) {
			TriMesh& TMeshA = *Group[i];
			TArray<UnstructuredPolygon>& UPolysA = GroupUPolys[i];
			for (int j = i + 1; j < GroupCt; j++) {
				TriMesh& TMeshB = *Group[j];
				TArray<UnstructuredPolygon>& UPolysB = GroupUPolys[j];
				TArray<TriMesh*> GroupExcludingAandB = Group;
				GroupExcludingAandB.Remove(&TMeshA);
				GroupExcludingAandB.Remove(&TMeshB);
				Internal_FindPolyEdges(
					TMeshA, TMeshB, GroupExcludingAandB, UPolysA, UPolysB, BBoxDiagDist
				);	
			}
		}
		// for any tri that has intersections, mark where the tri edges are inside and outside other meshes;
		// if no intersections, just note which vertices are inside and which are outside
		for (int i = 0; i < GroupCt - LastIsBoundsVolume; i++) {
			TriMesh& TMesh = *Group[i];
			TArray<TriMesh*> GroupExcludingThisMesh = Group;
			GroupExcludingThisMesh.Remove(&TMesh);
			Internal_PopulatePolyEdgesFromTriEdges(
				TMesh, GroupExcludingThisMesh, GroupUPolys[i], BBoxDiagDist, MinZ
			);
		}
	}
	
	// clever trick: assume the pt is inside the triangle. then, the pt makes three triangles - one with every pair
	// of tri vertices. the sum of those areas is equal to the area of the triangle. Added epsilon to make room for
	// fp error
	bool DoesPointTouchTri(const FVector& A, const FVector& B, const FVector& C, const FVector& P) {
		return (
			Tri::GetArea(P, A, B) + Tri::GetArea(P, B, C) + Tri::GetArea(P, C, A)
			<= Tri::GetArea(A, B, C) * NEAR_FACTOR
		);
	}

	bool DoesTriHaveSimilarVectors(const Tri& T, const FVector& A, const FVector& B, const FVector& C) {
		constexpr float Epsilon = 1000.0f;
		if (
			(
				FVector::DistSquared(T.A, A) <= Epsilon
				&& FVector::DistSquared(T.B, B) <= Epsilon
				&& FVector::DistSquared(T.C, C) <= Epsilon
			)
			|| (
				FVector::DistSquared(T.A, A) <= Epsilon
				&& FVector::DistSquared(T.B, C) <= Epsilon
				&& FVector::DistSquared(T.C, B) <= Epsilon
			)
			|| (
				FVector::DistSquared(T.A, B) <= Epsilon
				&& FVector::DistSquared(T.B, A) <= Epsilon
				&& FVector::DistSquared(T.C, C) <= Epsilon
			)
			|| (
				FVector::DistSquared(T.A, B) <= Epsilon
				&& FVector::DistSquared(T.B, C) <= Epsilon
				&& FVector::DistSquared(T.C, A) <= Epsilon
			)
			|| (
				FVector::DistSquared(T.A, C) <= Epsilon
				&& FVector::DistSquared(T.B, A) <= Epsilon
				&& FVector::DistSquared(T.C, B) <= Epsilon
			)
			|| (
				FVector::DistSquared(T.A, C) <= Epsilon
				&& FVector::DistSquared(T.B, B) <= Epsilon
				&& FVector::DistSquared(T.C, A) <= Epsilon
			)
		) {
			return true;
		}
		return false;
	}

	bool IsEar(const PolyNode& P) {
		for (const PolyNode* WalkNode = P.Next->Next; WalkNode != P.Prev; WalkNode = WalkNode->Next) {
			if (DoesPointTouchTri(P.Prev->Location, P.Location, P.Next->Location, WalkNode->Location)) {
				return false;
			}
		}
		return true;
	}

	VERTEX_T GetPolyVertexType(
		const FVector& Normal, const FVector& W, const FVector& V, const FVector& U, VERTEX_T PrevType
	) {
		// Q is the normal to the plane that passes through pts i-1 and i, orthogonal to the polygon, which vertex
		// i+1 needs to be on the correct side of to make an interior triangle with i and i-1
		const FVector Q = FVector::CrossProduct(V, Normal);
		const float CosTheta = FVector::DotProduct(Q, W);
		const float CosPhi = FVector::DotProduct(Q, U);

		if (PrevType == VERTEX_INTERIOR) {
			if (CosTheta > 0.0f && CosPhi <= 0.0f) {
				return VERTEX_EXTERIOR;
			}
			return VERTEX_INTERIOR;
		}
		if (CosTheta > 0.0f && CosPhi >= 0.0f) {
			return VERTEX_EXTERIOR;	
		}
		return VERTEX_INTERIOR;
	}
}
