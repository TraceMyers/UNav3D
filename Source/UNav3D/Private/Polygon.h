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

	PolyEdge(const FVector &A, const FVector& B);
	inline bool GetAOk() const;
	inline bool GetBOk() const;
	inline void SetANotOk();
	inline void SetBNotOk();
	
	const FVector& A;
	const FVector& B;
	uint32 Flags; // only using 2 of the 4 bytes
};

// a collection of tri-on-tri intersections, per tri
struct UnstructuredPolygon {
	TArray<PolyEdge> Edges;	
};