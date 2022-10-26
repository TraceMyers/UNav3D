#pragma once

#define UNAV_DBG

#ifdef UNAV_DBG

#define DBG_DRAW_TIME 5.0f

#define PRINT_TRIMESH_VERTEX_CT(TMesh) \
	printf( \
		"Bounds Volume found Mesh %s with %d vertices.\n", \
		TCHAR_TO_ANSI(*TMesh.MeshActor->GetName()), \
		TMesh.VertexCt \
	);

#define DRAW_TRIMESH_BOUNDING_BOXES(World, TMeshes) \
	for (int i = 0; i < TMeshes.Num(); i++) {  \
		const Geometry::TriMesh& TMesh = TMeshes[i];  \
		for (int j = 0; j < Geometry::RECT_PRISM_PTS; j++) {  \
			ADebugMarker::Spawn(World, TMesh.Box.Vertices[j], DBG_DRAW_TIME); \
		}  \
		constexpr int StartIndices [4] {0, 4, 5, 6};  \
		constexpr int EndIndices [4][3] {{1, 2, 3}, {1, 2, 7}, {1, 3, 7}, {2, 3, 7}};  \
		const FVector* Vertices = TMesh.Box.Vertices;  \
		for (int j = 0; j < 4; j++) {  \
			for (int k = 0; k < 3; k++) { \
				DrawDebugLine(World, Vertices[StartIndices[j]], Vertices[EndIndices[j][k]], FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
			} \
		}  \
	}  

#define PRINT_GEOPROC_NEGATIVE_RESPONSE(TMesh, Response) \
	if (Response != GeometryProcessor::GEOPROC_SUCCESS) { \
			printf( \
				"GeomProcessor: Mesh %s response: %d\n", \
				TCHAR_TO_ANSI(*TMesh.MeshActor->GetName()), \
				Response \
			); \
		}

#define DRAW_TRIMESH_VERTICES(World, TMesh, _it) \
	for (_it = 0; _it < TMesh.VertexCt; _it++) { \
		DrawDebugCircle(World, TMesh.Vertices[_it], 2.0f, 3, FColor::Blue, false, DBG_DRAW_TIME); \
	}

#define DRAW_TRIMESH_TRIS(World, TMesh, _it) \
	const auto& Tris = TMesh.Tris; \
	for (_it = 0; _it < Tris.Num(); _it++) { \
		const auto& Tri = Tris[_it]; \
		if (Tri.IsAObscured()) { \
			if (Tri.IsBObscured() && Tri.IsCObscured()) { \
				DrawDebugLine(World, Tri.A, Tri.B, FColor::Red, false, DBG_DRAW_TIME, 0, 2.0f); \
				DrawDebugLine(World, Tri.B, Tri.C, FColor::Red, false, DBG_DRAW_TIME, 0, 2.0f); \
				DrawDebugLine(World, Tri.C, Tri.A, FColor::Red, false, DBG_DRAW_TIME, 0, 2.0f); \
			} \
			else { \
				DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
				DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
				DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
			} \
		} \
		else if (Tri.IsBObscured()) { \
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
		} \
		else if (Tri.IsCObscured()) { \
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Magenta, false, DBG_DRAW_TIME, 0, 2.0f); \
		} \
		else { \
			DrawDebugLine(World, Tri.A, Tri.B, FColor::Green, false, DBG_DRAW_TIME, 0, 2.0f); \
			DrawDebugLine(World, Tri.B, Tri.C, FColor::Green, false, DBG_DRAW_TIME, 0, 2.0f); \
			DrawDebugLine(World, Tri.C, Tri.A, FColor::Green, false, DBG_DRAW_TIME, 0, 2.0f); \
		} \
	}

#else

#define PRINT_TRIMESH_VERTEX_CT(TMesh)
#define DRAW_TRIMESH_BOUNDING_BOXES(World, TMeshes)
#define PRINT_GEOPROC_NEGATIVE_RESPONSE(TMesh, Response)
#define DRAW_TRIMESH_VERTICES(World, TMesh)

#endif
