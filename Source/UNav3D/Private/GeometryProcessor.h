#pragma once


class UStaticMesh;
namespace Geometry {
	struct TriMesh;	
}

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
	GEOPROC_RESPONSE PopulateTriMesh(Geometry::TriMesh& TMesh, bool DoTransform=true) const;

	// Takes Populated TriMeshes, groups them by overlap, and reforms the overlapped meshes into continuous meshes
	// Populates OutMeshes, Empties InMeshes
	GEOPROC_RESPONSE ReformTriMeshes(
		UWorld* World,
		TArray<Geometry::TriMesh>& InMeshes,
		TArray<Geometry::TriMesh>& OutMeshes
	);

private:

	// Copies the index buffer of the mesh into a new buffer
	uint16* GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const;
	
	// Copies the vertex buffer of the mesh into TMesh.Vertices
	GEOPROC_RESPONSE GetVertices(const FStaticMeshLODResources& LOD, Geometry::TriMesh& TMesh, uint32& VertexCt) const;
	
	// Populates TMesh with Tris given the previously filled vertex and index buffers
	static GEOPROC_RESPONSE Populate(
		Geometry::TriMesh& TMesh,
		uint16* Indices,
		uint32 IndexCt,
		uint32 VertexCt
	);

	// Group Meshes together if they overlap
	static void GetIntersectGroups(
		UWorld* World,
		TArray<TArray<Geometry::TriMesh*>>& Groups,
		TArray<Geometry::TriMesh>& InMeshes
	);

	// Removes Tris that are obscured (inside other meshes in their group). Note: if Mesh A is fully enclosed by Mesh B
	// without intersections linking them, they will be part of separate groups, so Mesh A tris will not be culled.
	void CullInnerTris(const UWorld* World, TArray<TArray<Geometry::TriMesh*>>& Groups);
	
};

