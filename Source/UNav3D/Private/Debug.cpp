#include "Debug.h"
#include "Geometry.h"
#include "Engine/StaticMeshActor.h"
#include "DebugMarker.h"
#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"

void UNavDbg::PrintTriMesh(const TriMesh& TMesh) {
	printf(
		"Bounds Volume found Mesh %s with %d vertices and %d tris.\n", 
		TCHAR_TO_ANSI(*TMesh.MeshActor->GetName()), TMesh.VertexCt, TMesh.Tris.Num()
	);
}

void UNavDbg::PrintTriMeshMulti(const TArray<TriMesh>& TMeshes) {
	for (int i = 0; i < TMeshes.Num(); i++) {
		const TriMesh& TMesh = TMeshes[i];
		PrintTriMesh(TMesh);
	}  
}

void UNavDbg::DrawTriMeshBoundingBox(UWorld* World, const TriMesh& TMesh) {
	for (int j = 0; j < BoundingBox::VERTEX_CT; j++) { 
		ADebugMarker::Spawn(World, TMesh.Box.Vertices[j], DBG_DRAW_TIME);
	} 
	constexpr int StartIndices [4] {0, 4, 5, 6};  
	constexpr int EndIndices [4][3] {{1, 2, 3}, {1, 2, 7}, {1, 3, 7}, {2, 3, 7}};  
	const FVector* Vertices = TMesh.Box.Vertices;  
	for (int j = 0; j < 4; j++) {  
		for (int k = 0; k < 3; k++) { 
			DrawDebugLine(
				World,
				Vertices[StartIndices[j]],
				Vertices[EndIndices[j][k]],
				FColor::Magenta,
				false,
				DBG_DRAW_TIME,
				0,
				2.0f
			); 
		} 
	}
}

void UNavDbg::DrawTriMeshBoundingBoxMulti(UWorld* World, const TArray<TriMesh>& TMeshes) {
	for (int i = 0; i < TMeshes.Num(); i++) {
		const TriMesh& TMesh = TMeshes[i];
		DrawTriMeshBoundingBox(World, TMesh);
	}  
}

void UNavDbg::DrawTriMeshVertices(const UWorld* World, const TriMesh& TMesh) {
	for (int i = 0; i < TMesh.VertexCt; i++) { 
		DrawDebugCircle(
			World,
			TMesh.Vertices[i],
			2.0f,
			3,
			FColor::Blue,
			false,
			DBG_DRAW_TIME
		);
	}
}

void UNavDbg::DrawTriMeshVerticesMulti(const UWorld* World, const TArray<TriMesh>& TMeshes) {
	for (int i = 0; i < TMeshes.Num(); i++) {
		const TriMesh& TMesh = TMeshes[i];
		DrawTriMeshVertices(World, TMesh);
	}  
}

void UNavDbg::DrawTriMeshTris(const UWorld* World, const TriMesh& TMesh) {
	const auto& Tris = TMesh.Tris; 
	for (int i = 0; i < Tris.Num(); i++) {
		const auto& Tri = Tris[i];
		if (Tri.IsAObscured()) {
			if (Tri.IsBObscured() && Tri.IsCObscured()) {
				DrawDebugLine(World, Tri.A, Tri.B, FColor::Red, false, DBG_DRAW_TIME, 0, 2.0f);
				DrawDebugLine(World, Tri.B, Tri.C, FColor::Red, false, DBG_DRAW_TIME, 0, 2.0f); 
				DrawDebugLine(World, Tri.C, Tri.A, FColor::Red, false, DBG_DRAW_TIME, 0, 2.0f);
			}
			else {
				DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); 
				DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
				DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
			}
		}
		else if (Tri.IsBObscured()) {
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
		}
		else if (Tri.IsCObscured()) {
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f);
		}
		else {
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Green, false, DBG_DRAW_TIME, 0, 2.0f);
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Green, false, DBG_DRAW_TIME, 0, 2.0f);
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Green, false, DBG_DRAW_TIME, 0, 2.0f);
		}
	}
}

void UNavDbg::DrawTriMeshTrisMulti(const UWorld* World, const TArray<TriMesh>& TMeshes) {
	for (int i = 0; i < TMeshes.Num(); i++) {
		const TriMesh& TMesh = TMeshes[i];
		DrawTriMeshTris(World, TMesh);
	}  
}
