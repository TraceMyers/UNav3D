#pragma once
#include "Tri.h"

struct PolyNode {
	PolyNode(const FVector& _Location) :
		Location(_Location), PolygonIndex(-1), Prev(nullptr), Next(nullptr)
	{}
	const FVector Location;
	int PolygonIndex;
	PolyNode* Prev;
	PolyNode* Next;
};

struct VBufferPolyNode {
	VBufferPolyNode() :
		Neighbor(nullptr), Location(nullptr), Prev(nullptr), Next(nullptr)
	{}
	
	VBufferPolyNode(Tri* _Neighbor, FVector* Loc) :
		Neighbor(_Neighbor), Location(Loc), Prev(nullptr), Next(nullptr)
	{}
	
	Tri* Neighbor; // Tri neighbor at the edge made by this node and Next
	FVector* Location;
	VBufferPolyNode* Prev;
	VBufferPolyNode* Next;
};

// edges are between adjacent vertices; index Num() - 1 connects to 0; Every polygon is a stepping stone
// between a triangle that was intersected and a new set of triangles with no intersections.
struct Polygon {
	Polygon(int _TriIndex, FVector& _Normal) :
		TriIndex(_TriIndex),
		Mode(SUBTRACT), // guilty until proven innocent :(
		Normal(_Normal)
	{}

	TArray<PolyNode> Vertices;
	int TriIndex;
	enum POLYGON_MODE {SUBTRACT, ADD} Mode;
	FVector& Normal;
};

// Vertices are not guaranteed to be in any particular order
// TODO: use vertex buffer
struct VBufferPolygon {

	VBufferPolyNode* Vertices;
	
private:
	
	int _Num;
	int Sz;

public:
	
	VBufferPolygon() :
		Vertices(nullptr), _Num(0), Sz(0)
	{}
	
	void Empty() {
		if (Vertices != nullptr) {
			delete Vertices;
		}
		_Num = 0;
		Sz = 0;
	}
	
	bool Alloc(int Ct) {
		if (Vertices != nullptr) {
			Empty();				
		}
		Vertices = new VBufferPolyNode[Ct];
		if (Vertices == nullptr) {
			printf("GeometryProcessor::FormPolygon() alloc fail\n");
			return false;
		}
		Sz = Ct;
		return true;
	}

	VBufferPolyNode* Add(FVector* Location, Tri* Neighbor) {
		if (_Num < Sz) {
			return new (&Vertices[_Num++]) VBufferPolyNode(Neighbor, Location);
		}
		return nullptr;
	}

	int Num() const {
		return _Num;
	}
	
};

// used to denote intersections between triangles, for building polygons
struct PolyEdge {
	
	enum POLY_FLAGS {
		VERTEX_FLAGS =		0x000f,
		A_ENCLOSED =		0x0001,
		B_ENCLOSED =		0x0002,
		
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
	inline bool IsAEnclosed() const;
	inline bool IsBEnclosed() const;
	inline bool IsOnTriEdgeAB() const;
	inline bool IsOnTriEdgeBC() const;
	inline bool IsOnTriEdgeCA() const;
	inline void SetAEnclosed();
	inline void SetBEnclosed();

	// as of 10/27/2022, used in Geometry::Internal_GetTriPairPolyEdge() in order to instantiate one less block
	// of memory when copying nearly identical PolyEdges into two TArrays; the only difference between them
	// is their 0x00f0 and 0x0f00 flags need to be swapped.
	inline void FlipTriEdgeFlags();
	
	const FVector A;
	const FVector B;
	uint32 Flags; // only using 2 of the 4 bytes
	TArray<FVector> ObscuredLocations; // locations from A (toward B) marking where this edge passes in and out of other meshes
};

struct VBufferPolyEdge {
	
	VBufferPolyEdge() :
		VertexA(nullptr), VertexB(nullptr), Neighbor(nullptr)
	{}
	
	VBufferPolyEdge(FVector* _VertexA, FVector* _VertexB, Tri* _Neighbor=nullptr) :
		VertexA(_VertexA), VertexB(_VertexB), Neighbor(_Neighbor)
	{}
	
	FVector* VertexA;
	FVector* VertexB;
	Tri* Neighbor;
};

struct UPolyNode {
	UPolyNode(const FVector& _Location) :
		Location(_Location)
	{}
	const FVector Location;
	TArray<int> Edges;
};

// a collection of tri-on-tri intersections, per tri
struct UnstructuredPolygon {
	TArray<PolyEdge> Edges;
	int TriIndex;
};

struct VBufferUnstructuredPolygon {
	TArray<VBufferPolyEdge> Edges;
};