#pragma once

struct TriMesh;
struct Tri;
struct Polygon;
class UWorld;
struct BoundingBox;
struct UNavMesh;
class TriGrid;

#define UNAV_DBG
#define UNAV_DEV
#define DBG_DRAW_TIME 30.0f

#define LOCTEXT_NAMESPACE "UNav3D"
#define UNAV_GENMSG(x) \
	FMessageDialog::Open( \
		EAppMsgType::Ok, FText::Format(LOCTEXT("UNav3D", "UNAV3D Message: {0}", FText::FromString(x)) \
	);
#define UNAV_GENERR(x) \
	FMessageDialog::Open( \
		EAppMsgType::Ok, FText::Format(LOCTEXT("UNav3D", "UNAV3D ERROR: {0}"), FText::FromString(x)) \
	);
#undef LOCTEXT_NAMESPACE

namespace UNavDbg {
	
	void PrintTriMesh(const TriMesh& TMesh);

	void PrintTriMeshMulti(const TArray<TriMesh>& TMeshes);

	void DrawTriMeshBoundingBox(const UWorld* World, const TriMesh& TMesh);

	void DrawTriMeshBoundingBoxMulti(const UWorld* World, const TArray<TriMesh>& TMeshes);

	void DrawBoundingBox(const UWorld* World, const BoundingBox& BBox);

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

	void DrawTris(const UWorld* World, const TArray<Tri>& Tris, FColor Color=FColor::Red);
	
	void DrawTris(const UWorld* World, const TArray<Tri*>& Tris, FColor Color=FColor::Red);

	void DrawPolygons(const UWorld* World, const TArray<Polygon>& Polygons, FColor Color=FColor::Magenta);

	void PrintTri(const Tri& T);

	bool DoesTriMatchVertexCaptures(const Tri& T);
	
	// to use this make sure the build configuration is on debug and set a breakpoint inside the function
	void BreakOnVertexCaptureMatch(const Tri& T);
	
	// to use this make sure the build configuration is on debug and set a breakpoint inside the function
	void BreakOnVertexCaptureMatch(const Tri& A, const Tri& B);

	void SaveLine(const FVector& A, const FVector& B);

	void DrawSavedLines(const UWorld* World);

	void PrintThreadSuccess(FCriticalSection* Mutex);
}