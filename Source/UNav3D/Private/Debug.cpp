#include "Debug.h"
#include "Engine/StaticMeshActor.h"
#include "DebugMarker.h"
#include "CoreMinimal.h"
#include "Data.h"
#include "DrawDebugHelpers.h"
#include "TriMesh.h"
#include "Tri.h"
#include "Polygon.h"
#include "UNavMesh.h"

void UNavDbg::PrintTriMesh(const TriMesh& TMesh) {
	printf(
		"Bounds Volume found Mesh %s with %d vertices and %d tris.\n", 
		TCHAR_TO_ANSI(*TMesh.MeshActor->GetName()), TMesh.VertexCt, TMesh.Grid.Num()
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

void UNavDbg::DrawBoundingBox(UWorld* World, const BoundingBox& BBox) {
	for (int j = 0; j < BoundingBox::VERTEX_CT; j++) { 
		ADebugMarker::Spawn(World, BBox.Vertices[j], DBG_DRAW_TIME);
	} 
	constexpr int StartIndices [4] {0, 4, 5, 6};  
	constexpr int EndIndices [4][3] {{1, 2, 3}, {1, 2, 7}, {1, 3, 7}, {2, 3, 7}};  
	const FVector* Vertices = BBox.Vertices;  
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

void UNavDbg::DrawTriGridTris(const UWorld* World, const TriGrid& Tris) {
	for (int i = 0; i < Tris.Num(); i++) {
		const auto& Tri = Tris[i];
		if (Tri.IsTriCull()) {
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Red, false, DBG_DRAW_TIME, 0, 1.0f);
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Red, false, DBG_DRAW_TIME, 0, 1.0f); 
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Red, false, DBG_DRAW_TIME, 0, 1.0f);
		}
		else if (Tri.IsAObscured() || Tri.IsBObscured() || Tri.IsCObscured()) {
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 1.0f);
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 1.0f);
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 1.0f);
		}
		else {
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Green, false, DBG_DRAW_TIME, 0, 1.0f);
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Green, false, DBG_DRAW_TIME, 0, 1.0f);
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Green, false, DBG_DRAW_TIME, 0, 1.0f);
		}
	}
}

void UNavDbg::DrawTriMeshTris(const UWorld* World, const TriMesh& TMesh) {
	const auto& Tris = TMesh.Grid; 
	DrawTriGridTris(World, Tris);	
}

void UNavDbg::DrawTriMeshTrisMulti(const UWorld* World, const TArray<TriMesh>& TMeshes) {
	for (int i = 0; i < TMeshes.Num(); i++) {
		const TriMesh& TMesh = TMeshes[i];
		DrawTriMeshTris(World, TMesh);
	}  
}

void UNavDbg::DrawNavMeshTris(const UWorld* World, const UNavMesh& NMesh) {
	auto& Grid = NMesh.Grid;
	DrawTriGridTris(World, Grid);
}

void UNavDbg::PrintTriMeshIntersectGroups(const TArray<TArray<TriMesh*>> IntersectGroups) {
	for (int i = 0; i < IntersectGroups.Num(); i++) {
		printf("group {%d}:\n", i + 1);
		auto& IntersectGroup = IntersectGroups[i];
		for (int j = 0; j < IntersectGroup.Num(); j++) {
			const auto TMesh = IntersectGroup[j];
			printf("TMesh: %s\n", TCHAR_TO_ANSI(*TMesh->MeshActor->GetName()));
		}
		putchar('\n');
	}
}

void UNavDbg::DrawPolygon(const UWorld* World, const Polygon& P) {
	const TArray<PolyNode>& Points = P.Vertices;
	for (int i = 0; i < Points.Num() - 1; i++) {
		DrawDebugLine(World, Points[i].Location, Points[i + 1].Location, FColor::Purple, false, DBG_DRAW_TIME, 0, 1.5f);	
	}
	DrawDebugLine(World, Points[0].Location, Points.Last().Location, FColor::Purple, false, DBG_DRAW_TIME, 0, 1.5f);	
}

void UNavDbg::DrawAllPolygons(const UWorld* World, const TArray<TArray<TArray<Polygon>>>& Polygons) {
	for (int i = 0; i < Polygons.Num(); i++) {
		auto& GroupPolygons = Polygons[i];
		for (int j = 0; j < GroupPolygons.Num(); j++) {
			auto& TMeshPolygons = GroupPolygons[j];
			for (int k = 0; k < TMeshPolygons.Num(); k++) {
				DrawPolygon(World, TMeshPolygons[k]);
			}
		}
	}
}

void UNavDbg::DrawTriMeshNormals(const UWorld* World, const TriMesh& TMesh) {
	for (int i = 0; i < TMesh.Grid.Num(); i++) {
		auto& T = TMesh.Grid[i];
		DrawDebugLine(World, T.GetCenter(), T.GetCenter() + T.Normal * 10.0f, FColor::Green, false, DBG_DRAW_TIME, 0, 1.5f);
	}
}

void UNavDbg::DrawNavMeshNormals(const UWorld* World, const UNavMesh& NMesh) {
	for (int i = 0; i < NMesh.Grid.Num(); i++) {
		auto& T = NMesh.Grid[i];
		DrawDebugLine(World, T.GetCenter(), T.GetCenter() + T.Normal * 10.0f, FColor::Red, false, DBG_DRAW_TIME, 0, 1.5f);
	}
}

void UNavDbg::DrawTris(const UWorld* World, const TArray<Tri>& Tris, FColor Color) {
	for (int i = 0; i < Tris.Num(); i++) {
		const auto& Tri = Tris[i];
		DrawDebugLine(World, Tri.A, Tri.B, Color, false, DBG_DRAW_TIME, 0, 1.0f);
		DrawDebugLine(World, Tri.B, Tri.C, Color, false, DBG_DRAW_TIME, 0, 1.0f); 
		DrawDebugLine(World, Tri.C, Tri.A, Color, false, DBG_DRAW_TIME, 0, 1.0f);
	}	
}

void UNavDbg::DrawPolygons(const UWorld* World, const TArray<Polygon>& Polygons, FColor Color) {
	for (auto& Polygon : Polygons) {
		DrawPolygon(World, Polygon);
	}	
}
