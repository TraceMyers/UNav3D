#pragma once


struct PolyEdge;
struct PolyNode;
class UStaticMesh;
struct TriMesh;
struct Tri;
struct UnstructuredPolygon;
struct UPolyNode;
struct Polygon;
struct UNavMesh;
class TriGrid;

// GeometryProcessor's job to work on geometrical objects, given information learned by using Geometry.h
class GeometryProcessor {
	
public:

	enum GEOPROC_RESPONSE {
		GEOPROC_SUCCESS,
		GEOPROC_HIGH_INDEX,
		GEOPROC_ALLOC_FAIL,
		GEOPROC_BATCH_SZ_0,
		GEOPROC_BAD_BATCH_NO
	};

	GeometryProcessor();
	~GeometryProcessor();
	
	static uint32 GetTriMeshBatch(
		TArray<TArray<Tri*>>& BatchTris,
		const TriMesh& TMesh,
		uint32& StartTriIndex,
		uint16 BatchSz,
		uint16 BatchNo
	);
	
	// Pulls Static Mesh data and populates TMesh with it. If TForm != nullptr, it will be used to transform the vertices
	GEOPROC_RESPONSE PopulateTriMesh(TriMesh& TMesh, bool DoTransform=true) const;

	// Takes Populated TriMeshes and groups them by overlap
	static void GroupTriMeshes(TArray<TriMesh>& TMeshes, TArray<TArray<TriMesh*>>& Groups);

	// Takes a group of overlapping TriMeshes, simplifies the individual meshes, and reforms the group into one mesh
	static void ReformTriMesh(
		TArray<TriMesh*>* Group, FCriticalSection* Mutex, const FThreadSafeBool* IsThreadRun, UNavMesh* NMesh
	);

private:

	static constexpr float EPSILON = 3e-4f;

	// Copies the index buffer of the mesh into a new buffer
	uint16* GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const;
	
	// Copies the vertex buffer of the mesh into TMesh.Vertices
	GEOPROC_RESPONSE GetVertices(const FStaticMeshLODResources& LOD, TriMesh& TMesh, uint32& VertexCt) const;

	static inline Tri* GetUnbatchedTri(const TriGrid& Grid);

	static bool GetNewStartTriIndex(const TriGrid& Grid, uint32& StartTriIndex);
	
	// Populates TMesh with Tris given the previously filled vertex and index buffers
	static GEOPROC_RESPONSE Populate(
		TriMesh& TMesh,
		uint16* Indices,
		uint32 IndexCt,
		uint32 VertexCt
	);

	// flag tris with flags that relate to their location relative to the bounds volume
	static void FlagTrisWithBV(TArray<TriMesh*>& TMeshes);

	// Find where meshes intersect and build polygons out of the exposed portions of tris
	static void BuildPolygonsAtMeshIntersections(
		TArray<TriMesh*>& Group,
		TArray<TArray<Polygon>>& Polygons,
		FCriticalSection* Mutex
	);

	static void FormMeshFromGroup(
		TArray<TriMesh*>& Group,
		TArray<TArray<Polygon>>& Polygons,
		UNavMesh* NMesh,
		FCriticalSection* Mutex
	);

	// each edge (intersections and tri edges) on a tri has points along it which mark where the edge
	// is inside and where it's outside of other meshes. those points *should* link up with points on
	// other edges, and exposed sections should link up to form polygons. this function creates nodes
	// and links them together into graphs
	static void PopulateNodes(const Tri& T, const UnstructuredPolygon& UPoly, TArray<UPolyNode>& PolygonNodes);

	static bool DoesEdgeConnect(const TArrayView<UPolyNode>& Nodes, const PolyEdge& Edge);

	// helper to PopulateNodes that searches for whether or not nodes exist at A and B first, adds
	// if not, and links them
	static void AddUPolyNodes(TArray<UPolyNode>& Nodes, const FVector& A, const FVector& B, int& NodeCtr);

	// makes n polygons given n closed loop graphs created by intersections + edges on a tri
	static void Polygonize(
		Tri& T,
		TArray<UPolyNode>& PolygonNodes,
		TArray<Polygon>& Polygons,
		int TriIndex
	);

	static void PopulateUnmarkedTriData(
		const TArray<TriMesh*>& Group,
		TArray<FVector*>& Vertices,
		TArray<FIntVector>& TriVertexIndices
	);

	// attempts to create a new set of tris from each polygon. assumes simple polygons of >= 3 vertices
	static void Triangulize(
		TArray<Polygon>& Polygons,
		TArray<FVector>& Vertices,
		TArray<FIntVector>& TriVertexIndices,
		TArray<FVector*>& Normals
	);

	static inline void LinkPolygonEdges(Polygon& P);

	static inline void AddTriData(
		TArray<FIntVector>& Indices,
		const PolyNode& A,
		const PolyNode& B,
		const PolyNode& C,
		const FVector& Normal,
		int IndexOffset
	);
	
	// re-polygonizes nearby tris with similar normals and reforms triangles from those polygons, simplifying the
	// meshes
	
};

