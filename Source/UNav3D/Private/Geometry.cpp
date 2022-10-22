#include "Geometry.h"

#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"

namespace Geometry {

	namespace {
		
		const FVector BaseExtent[BoundingBox::RECT_PRISM_PTS] {
			FVector(-1.0f, -1.0f, -1.0f),
			FVector(1.0f, -1.0f, -1.0f),
			FVector(-1.0f, 1.0f, -1.0f),
			FVector(-1.0f, -1.0f, 1.0f),
			FVector(1.0f, 1.0f, -1.0f),
			FVector(1.0f, -1.0f, 1.0f),
			FVector(-1.0f, 1.0f, 1.0f),
			FVector(1.0f, 1.0f, 1.0f)
		};
		
		void Internal_SetBoundingBox(BoundingBox& BBox, const FVector& Extent, const FTransform TForm, bool DoTransform) {
			if (DoTransform) {
				for (int i = 0; i < BoundingBox::RECT_PRISM_PTS; i++) {
					BBox.Vertices[i] = TForm.TransformPositionNoScale(BaseExtent[i] * Extent);
				}
			}
			else {
				for (int i = 0; i < BoundingBox::RECT_PRISM_PTS; i++) {
					BBox.Vertices[i] = BaseExtent[i] * Extent;
				}
			}
			const FVector& RefPoint = BBox.Vertices[0];
			for (int i = 0; i < 3; i++) {
				FVector& OverlapCheckVector = BBox.OverlapCheckVectors[i];
				OverlapCheckVector = BBox.Vertices[i + 1] - RefPoint;
				BBox.OverlapCheckSqMags[i] = FVector::DotProduct(OverlapCheckVector, OverlapCheckVector);
			}	
		}
		
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

		bool Internal_DoesLineSegmentIntersect(const BoundingBox& Box, const FVector& PointA, const FVector& PointB) {
			return false;
		}

	}
	
	void SetBoundingBox(BoundingBox& BBox, const AStaticMeshActor* MeshActor, bool DoTransform) {
		const FVector Extent = MeshActor->GetStaticMeshComponent()->GetStaticMesh()->GetBoundingBox().GetExtent();
		const FTransform TForm = MeshActor->GetTransform();
		Internal_SetBoundingBox(BBox, Extent, TForm, DoTransform);	
	}

	void SetBoundingBox(BoundingBox& BBox, const UBoxComponent* BoxCmp, bool DoTransform) {
		const FVector Extent = BoxCmp->GetScaledBoxExtent();
		const FTransform TForm = BoxCmp->GetComponentTransform();
		Internal_SetBoundingBox(BBox, Extent, TForm, DoTransform);	
	}
	
	bool DoBoundingBoxesOverlap(const BoundingBox& BBoxA, const BoundingBox& BBoxB) {
		for (int i = 0; i < BoundingBox::RECT_PRISM_PTS; i++) {
			if (Internal_IsPointInside(BBoxA, BBoxB.Vertices[i])) {
				return true;
			}
		}
		for (int i = 0; i < BoundingBox::RECT_PRISM_PTS; i++) {
			if (Internal_IsPointInside(BBoxB, BBoxA.Vertices[i])) {
				return true;
			}
		}
		// TODO: check edge-face intersections, too
		return false;
	}
	
}
