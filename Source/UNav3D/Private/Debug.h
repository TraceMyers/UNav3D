#pragma once

struct TriMesh;
struct Tri;
class UWorld;

#define UNAV_DBG
#define DBG_DRAW_TIME 5.0f

namespace UNavDbg {
	
	void PrintTriMesh(const TriMesh& TMesh);

	void PrintTriMeshMulti(const TArray<TriMesh>& TMeshes);

	void DrawTriMeshBoundingBox(UWorld* World, const TriMesh& TMesh);

	void DrawTriMeshBoundingBoxMulti(UWorld* World, const TArray<TriMesh>& TMeshes);

	void DrawTriMeshVertices(const UWorld* World, const TriMesh& TMesh);
	
	void DrawTriMeshVerticesMulti(const UWorld* World, const TArray<TriMesh>& TMeshes);

	void DrawTriMeshTris(const UWorld* World, const TriMesh& TMesh);
	
	void DrawTriMeshTrisMulti(const UWorld* World, const TArray<TriMesh>& TMeshes);

	void PrintTriMeshIntersectGroups(const TArray<TArray<TriMesh*>> IntersectGroups);
}