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
		bool Internal_IsPointInside(const BoundingBox& BBox, const FVector& Point) {
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
		bool Internal_DoesLineSegmentIntersect(const BoundingBox& Box, const FVector& PointA, const FVector& PointB) {
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
			if (Internal_IsPointInside(BBoxA, BBoxB.Vertices[i])) {
				return true;
			}
		}
		for (int i = 0; i < RECT_PRISM_PTS; i++) {
			if (Internal_IsPointInside(BBoxB, BBoxA.Vertices[i])) {
				return true;
			}
		}
		// TODO: check edge-face intersections, too
		return false;
	}
	
}
