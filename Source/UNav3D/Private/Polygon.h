#pragma once

// edges are between adjacent vertices; index Num() - 1 connects to 0; Every polygon is a stepping stone
// between a triangle that was intersected and a new set of triangles with no intersections.
struct Polygon {
	Polygon(int _TMeshTriIndex)
		: TMeshTriIndex(_TMeshTriIndex), Mode(ADD)
	{}

	TArray<FVector> Vertices;
	int TMeshTriIndex;
	enum POLYGON_MODE {ADD, SUBTRACT} Mode;
};

// used to denote intersections between triangles, for building polygons
struct PolyEdge {
	
	enum POLY_FLAGS {
		VERTEX_FLAGS =		0x000f,
		A_OK =				0x0001,
		B_OK =				0x0002,
		
		EDGE_FLAGS =		0x00f0,
		ON_EDGE_AB =		0x0010,
		ON_EDGE_BC =		0x0020,
		ON_EDGE_CA =		0x0040,
		
		OTHER_EDGE_FLAGS =	0x0f00,
		OTHER_ON_EDGE_AB =	0x0100,
		OTHER_ON_EDGE_BC =	0x0200,
		OTHER_ON_EDGE_CA =	0x0400
	};
	
	PolyEdge(const FVector &A, const FVector& B, uint16 TriEdgeFlags);
	inline bool IsAOk() const;
	inline bool IsBOk() const;
	inline bool IsOnTriEdgeAB() const;
	inline bool IsOnTriEdgeBC() const;
	inline bool IsOnTriEdgeCA() const;
	inline void SetANotOk();
	inline void SetBNotOk();

	// as of 10/27/2022, used in Geometry::Internal_GetTriPairPolyEdge() in order to instantiate one less block
	// of memory when copying nearly identical PolyEdges into two TArrays; the only difference between them
	// is their 0x00f0 and 0x0f00 flags need to be swapped.
	inline void FlipTriEdgeFlags();
	
	const FVector& A;
	const FVector& B;
	uint32 Flags; // only using 2 of the 4 bytes
	TArray<float> TrDropDistances; // distances from A (toward B) marking where this edge passes in and out of other meshes
};

// a collection of tri-on-tri intersections, per tri
struct UnstructuredPolygon {
	TArray<PolyEdge> Edges;	
};