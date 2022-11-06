#pragma once


class UStaticMesh;
struct TriMesh;
struct Tri;
struct UnstructuredPolygon;
struct UPolyNode;
struct Polygon;
struct UNavMesh;

// GeometryProcessor's job to work on geometrical objects, given information learned by using Geometry.h
class GeometryProcessor {
	
public:

	enum GEOPROC_RESPONSE {
		GEOPROC_SUCCESS,
		GEOPROC_HIGH_INDEX,
		GEOPROC_ALLOC_FAIL
	};

	GeometryProcessor();
	~GeometryProcessor();
	
	// Pulls Static Mesh data and populates TMesh with it. If TForm != nullptr, it will be used to transform the vertices
	GEOPROC_RESPONSE PopulateTriMesh(TriMesh& TMesh, bool DoTransform=true) const;

	// Takes Populated TriMeshes, groups them by overlap, and reforms the overlapped meshes into continuous meshes
	// Populates OutMeshes with new
	static GEOPROC_RESPONSE PopulateNavMeshes(
		TArray<TriMesh>& InMeshes,
		TArray<UNavMesh>& OutMeshes
	);

	// re-polygonizes nearby tris with similar normals and reforms triangles from those polygons, simplifying the
	// meshes
	static GEOPROC_RESPONSE SimplifyNavMeshes(
		TArray<UNavMesh>& NMeshes
	);

private:

	// Copies the index buffer of the mesh into a new buffer
	uint16* GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const;
	
	// Copies the vertex buffer of the mesh into TMesh.Vertices
	GEOPROC_RESPONSE GetVertices(const FStaticMeshLODResources& LOD, TriMesh& TMesh, uint32& VertexCt) const;
	
	// Populates TMesh with Tris given the previously filled vertex and index buffers
	static GEOPROC_RESPONSE Populate(
		TriMesh& TMesh,
		uint16* Indices,
		uint32 IndexCt,
		uint32 VertexCt
	);

	// Group Meshes together if they overlap
	static void GetIntersectGroups(
		TArray<TArray<TriMesh*>>& Groups,
		TArray<TriMesh>& InMeshes
	);

	// flag tris fully outside the bounds valume for cull
	static void FlagOutsideTris(TArray<TArray<TriMesh*>>& Groups);

	// Find where meshes intersect and build polygons out of the exposed portions of tris
	static void BuildPolygonsAtMeshIntersections(
		TArray<TArray<TriMesh*>>& Groups,
		TArray<TArray<TArray<Polygon>>>& Polygons
	);

	static void FormMeshesFromGroups(
		TArray<TArray<TriMesh*>>& Groups,
		TArray<TArray<TArray<Polygon>>>& Polygons,
		TArray<UNavMesh>& NavMeshes
	);

	// each edge (intersections and tri edges) on a tri has points along it which mark where the edge
	// is inside and where it's outside of other meshes. those points *should* link up with points on
	// other edges, and exposed sections should link up to form polygons. this function creates nodes
	// and links them together into graphs
	static void PopulateNodes(const Tri& T, const UnstructuredPolygon& UPoly, TArray<UPolyNode>& PolygonNodes);

	// helper to PopulateNodes that searches for whether or not nodes exist at A and B first, adds
	// if not, and links them
	static void AddUPolyNodes(TArray<UPolyNode>& Nodes, const FVector& A, const FVector& B);

	// makes n polygons given n closed loop graphs created by intersections + edges on a tri
	static void BuildPolygonsFromTri(
		Tri& T,
		TArray<UPolyNode>& PolygonNodes,
		TArray<Polygon>& TMeshPolygons,
		int TriIndex
	);

	static void PopulateUnmarkedTriData(
		const TArray<TriMesh*>& Group,
		TArray<FVector*>& Vertices,
		TArray<FIntVector>& TriVertexIndices
	);

	// attempts to create a new set of tris from each polygon. assumes simple polygons of >= 3 vertices
	static void CreateNewTriData(
		TArray<Polygon>& Polygons,
		TArray<FVector>& Vertices,
		TArray<FIntVector>& TriVertexIndices,
		TArray<FVector*>& Normals
	);

	static inline void LinkPolygonEdges(Polygon& P);
};

