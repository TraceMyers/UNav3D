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

		// populate BBox with vertices and precomputed overlap-checking values
		void Internal_SetBoundingBox(
			BoundingBox& BBox,
			const FVector& Extent,
			const FTransform TForm,
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

		// Sometimes BBoxes will overlap without either having points in the other, but this can be checked with
		// a line test
		bool Internal_DoesLineSegmentIntersectBox(const BoundingBox& Box, const FVector& PointA, const FVector& PointB) {
			return false;
		}

		void Internal_GetLineSegmentIntersectionsOneWay(
			const UWorld* World,
			TArray<AStaticMeshActor*>& MeshActors,
			FCollisionQueryParams& Params,
			const FVector& A, 
			const FVector& B
		) {
			TArray<FHitResult> OutHits;
			// TODO: currently dependent on a trace channel set in this project; there are methods to add one to the...
			// TODO: ... project via this plugin
			World->LineTraceMultiByChannel(
				OutHits,
				A,
				B,
				ECollisionChannel::ECC_GameTraceChannel1,
				Params
			);
			for (int i = 0; i < OutHits.Num(); i++) {
				FHitResult& HitResult = OutHits[i];
				AActor* Actor = HitResult.GetActor();
				if (Actor != nullptr) {
					AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
					if (MeshActor && MeshActors.Find(MeshActor) == -1) {
						MeshActors.Add(MeshActor);
					}
				}
			}
		}

		// Get all static mesh actors intersections along A -> B and B -> A
		void Internal_GetLineSegmentIntersections(
			const UWorld* World,
			TArray<AStaticMeshActor*>& MeshActors, 
			FCollisionQueryParams& Params,
			const FVector& A, 
			const FVector& B
		) {
			Internal_GetLineSegmentIntersectionsOneWay(World, MeshActors, Params, A, B);
			Internal_GetLineSegmentIntersectionsOneWay(World, MeshActors, Params, B, A);
		}
		
		// Get poi between a ray and another triangle. returns false when no intersection
		// https://stackoverflow.com/questions/42740765/intersection-between-line-and-triangle-in-3d
		// Uses Cramer's Rule to solve a system of equations with as many unknowns as there are equations.
		// Is uni-directional.
		// TODO: test is two-directional?
		bool Internal_Raycast(
			const FVector& Origin,
			const FVector& Dir,
			float Length,
			const Tri& T,
			FVector& POI
		) {
			const FVector E1 (T.B - T.A);
			const FVector E2 (T.C - T.A);
			const FVector ScaledTriNormal = FVector::CrossProduct(E1, E2);
			const float Determinant = -FVector::DotProduct(Dir, ScaledTriNormal);
			const float InvDeterminant = 1.0f / Determinant;
			const FVector AO (Origin - T.A);
			const FVector DAO = FVector::CrossProduct(AO, Dir);
			const float U = FVector::DotProduct(E2, DAO) * InvDeterminant;
			const float V = -FVector::DotProduct(E1, DAO) * InvDeterminant;
			const float W = FVector::DotProduct(AO, ScaledTriNormal) * InvDeterminant;
			POI = Origin + W * Dir;
			return Determinant >= 1e-6f && W >= 0.0f && U >= 0.0f && V >= 0.0f && (U + V) <= 1.0f && Length >= W;	
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

		// TODO: test
		bool Internal_DoTriMeshesIntersect(const TriMesh& TMeshA, const TriMesh& TMeshB) {
			const TArray<Tri>& TMeshATris = TMeshA.Tris;
			const TArray<Tri>& TMeshBTris = TMeshB.Tris;
			const int TMeshBTriCt = TMeshBTris.Num();
			for (int i = 0; i < TMeshATris.Num(); i++) {
				const Tri& T0 = TMeshATris[i];
				for (int j = 0; j < TMeshBTriCt; j++) {
					const Tri& T1 = TMeshBTris[j];
					const float SqDist = FVector::DistSquared(T0.Center, T1.Center);
					if (
						// (SqDist < T0.SqPerimeter || SqDist < T1.SqPerimeter)
						true
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
	}

	bool DoTriMeshesIntersect(UWorld* World, const TriMesh& TMeshA, const TriMesh& TMeshB) {
		const TArray<Tri>& TMeshATris = TMeshA.Tris;
		const TArray<Tri>& TMeshBTris = TMeshB.Tris;
		const int TMeshBTriCt = TMeshBTris.Num();
		for (int i = 0; i < TMeshATris.Num(); i++) {
			const Tri& T0 = TMeshATris[i];
			for (int j = 0; j < TMeshBTriCt; j++) {
				const Tri& T1 = TMeshBTris[j];
				const float SqDist = FVector::DistSquared(T0.Center, T1.Center);
				if (
					// (SqDist < T0.SqPerimeter || SqDist < T1.SqPerimeter)
					true
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
		// TODO: check edge-face intersections, too
		return false;
	}

	// Tests along all tri edges for intersections. 
	bool GetTriMeshIntersections(
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

}
