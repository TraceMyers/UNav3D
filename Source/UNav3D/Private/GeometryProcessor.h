#pragma once


class UStaticMesh;
struct TriMesh;	

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
	// Populates OutMeshes, Empties InMeshes
	GEOPROC_RESPONSE ReformTriMeshes(
		const UWorld* World,
		TArray<TriMesh>& InMeshes,
		TArray<TriMesh>& OutMeshes
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

	void BuildPolygonsAtMeshIntersections(TArray<TArray<TriMesh*>>& Groups);
	
};

