#pragma once


class UStaticMesh;
struct TriMesh;
struct Tri;
struct UnstructuredPolygon;
struct PolyNode;
struct Polygon;
struct UNavMesh;

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
		const UWorld* World,
		TArray<TriMesh>& InMeshes,
		TArray<UNavMesh>& OutMeshes
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

	// Any tris that have vertices inside other meshes have those vertices flagged
	static void FlagObscuredTris(const UWorld* World, TArray<TArray<TriMesh*>>& Groups);

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
	static void PopulateNodes(const Tri& T, const UnstructuredPolygon& UPoly, TArray<PolyNode>& PolygonNodes);

	// helper to PopulateNodes that searches for whether or not nodes exist at A and B first, adds
	// if not, and links them
	static void AddPolyNodes(TArray<PolyNode>& Nodes, const FVector& A, const FVector& B);

	// makes n polygons given n closed loop graphs created by intersections + edges on a tri
	static void BuildPolygonsFromTri(
		Tri& T,
		TArray<PolyNode>& PolygonNodes,
		TArray<Polygon>& TMeshPolygons,
		int TriIndex
	);

	static void PopulateUnmarkedTriData(
		const TArray<TriMesh*>& Group,
		TArray<FVector*>& Vertices,
		TArray<FIntVector>& TriVertexIndices
	);

	static void CreateNewTriData(
		TArray<Polygon>& Polygons,
		TArray<FVector>& Vertices,
		TArray<FIntVector>& TriVertexIndices,
		TArray<FVector*>& Normals
	);
};

