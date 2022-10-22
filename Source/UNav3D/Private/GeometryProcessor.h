#pragma once


class UStaticMesh;
namespace Geometry {
	struct TriMesh;	
}

class GeometryProcessor {

	
public:

	enum GEOPROC_RESPONSE {
		GEOPROC_SUCCESS,
		GEOPROC_ERR_BAD_INDEX_MAX,
		GEOPROC_ERR_BAD_INDEX,
		GEOPROC_ALLOC_FAIL
	};

	GeometryProcessor();
	~GeometryProcessor();
	
	// Pulls Static Mesh data and populates TMesh with it. If TForm != nullptr, it will be used to transform the vertices
	GEOPROC_RESPONSE CreateTriMesh(Geometry::TriMesh& TMesh, UStaticMesh* StaticMesh, const FTransform* TForm=nullptr) const;

private:

	// Copies the index buffer of the mesh into a new buffer
	uint16* GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const;
	
	// Copies the vertex buffer of the mesh into TMesh.Vertices
	GEOPROC_RESPONSE GetVertices(const FStaticMeshLODResources& LOD, Geometry::TriMesh& TMesh, uint32& VertexCt) const;
	
	// Populates TMesh with Tris given the previously filled vertex and index buffers
	static GEOPROC_RESPONSE PopulateTriMesh(
		Geometry::TriMesh& TMesh,
		uint16* Indices,
		uint32 IndexCt,
		uint32 VertexCt
	);
};

