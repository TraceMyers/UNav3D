#pragma once

#define UNAV_GEO_DBG

class UStaticMesh;
struct TriMesh;

class GeometryProcessor {
	
public:

	enum GEOPROC_RESPONSE {
		GEOPROC_SUCCESS,
		GEOPROC_ERR_BAD_INDEX_MAX,
		GEOPROC_ERR_BAD_INDEX
	};

	GeometryProcessor();
	~GeometryProcessor();
	
	// Pulls Static Mesh data and populates TMesh with it.
	// If TForm != nullptr, it will be used to transform the vertices
	GEOPROC_RESPONSE CreateTriMesh(TriMesh& TMesh, UStaticMesh* StaticMesh, const FTransform* TForm=nullptr) const;

private:

	uint16* GetIndices(const FStaticMeshLODResources& LOD, uint32& IndexCt) const;
	FVector* GetVertices(const FStaticMeshLODResources& LOD, uint32& VertexCt) const;
	static GEOPROC_RESPONSE PopulateTriMesh(
		TriMesh& TMesh,
		uint16* Indices,
		FVector* Vertices,
		uint32 IndexCt,
		uint32 VertexCt
	);
};

