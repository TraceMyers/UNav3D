#pragma once

namespace Geometry {
	struct TriMesh;
	struct Tri;
}
class UWorld;

#define UNAV_DBG
#define DBG_DRAW_TIME 5.0f

namespace UNavDbg {
	
	void PrintTriMesh(const Geometry::TriMesh& TMesh);

	void PrintTriMeshMulti(const TArray<Geometry::TriMesh>& TMeshes);

	void DrawTriMeshBoundingBox(UWorld* World, const Geometry::TriMesh& TMesh);

	void DrawTriMeshBoundingBoxMulti(UWorld* World, const TArray<Geometry::TriMesh>& TMeshes);

	void DrawTriMeshVertices(const UWorld* World, const Geometry::TriMesh& TMesh);
	
	void DrawTriMeshVerticesMulti(const UWorld* World, const TArray<Geometry::TriMesh>& TMeshes);

	void DrawTriMeshTris(const UWorld* World, const Geometry::TriMesh& TMesh);
	
	void DrawTriMeshTrisMulti(const UWorld* World, const TArray<Geometry::TriMesh>& TMeshes);
}