#pragma once

struct TriMesh;
struct Tri;
struct Polygon;
class UWorld;
struct BoundingBox;
struct UNavMesh;
class TriGrid;

#define UNAV_DBG
#define DBG_DRAW_TIME 128.0f

namespace UNavDbg {
	
	void PrintTriMesh(const TriMesh& TMesh);

	void PrintTriMeshMulti(const TArray<TriMesh>& TMeshes);

	void DrawTriMeshBoundingBox(UWorld* World, const TriMesh& TMesh);

	void DrawTriMeshBoundingBoxMulti(UWorld* World, const TArray<TriMesh>& TMeshes);

	void DrawBoundingBox(UWorld* World, const BoundingBox& BBox);

	void DrawTriMeshVertices(const UWorld* World, const TriMesh& TMesh);
	
	void DrawTriMeshVerticesMulti(const UWorld* World, const TArray<TriMesh>& TMeshes);

	void DrawTriGridTris(const UWorld* World, const TriGrid& Grid);

	void DrawTriMeshTris(const UWorld* World, const TriMesh& TMesh);
	
	void DrawTriMeshTrisMulti(const UWorld* World, const TArray<TriMesh>& TMeshes);

	void DrawNavMeshTris(const UWorld* World, const UNavMesh& NMesh);

	void PrintTriMeshIntersectGroups(const TArray<TArray<TriMesh*>> IntersectGroups);

	void DrawPolygon(const UWorld* World, const Polygon& P);

	void DrawAllPolygons(const UWorld* World, const TArray<TArray<TArray<Polygon>>>& Polygons);

	void DrawTriMeshNormals(const UWorld* World, const TriMesh& TMesh);

	void DrawNavMeshNormals(const UWorld* World, const UNavMesh& NMesh);
}