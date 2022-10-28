#include "Polygon.h"

static constexpr int HALF_BYTE = 4;

PolyEdge::PolyEdge(const FVector& _A, const FVector& _B, uint16 TriEdgeFlags) :
	A(_A), B(_B), Flags(TriEdgeFlags)	
{}

bool PolyEdge::IsAEnclosed() const {
	return Flags & A_ENCLOSED;	
}

bool PolyEdge::IsBEnclosed() const {
	return Flags & B_ENCLOSED;	
}

bool PolyEdge::IsOnTriEdgeAB() const {
	return Flags & ON_EDGE_AB;	
}

bool PolyEdge::IsOnTriEdgeBC() const {
	return Flags & ON_EDGE_BC;	
}

bool PolyEdge::IsOnTriEdgeCA() const {
	return Flags & ON_EDGE_CA;	
}

void PolyEdge::SetAEnclosed() {
	Flags |= A_ENCLOSED;	
}

void PolyEdge::SetBEnclosed() {
	Flags |= B_ENCLOSED;	
}

void PolyEdge::FlipTriEdgeFlags() {
	Flags = (Flags & VERTEX_FLAGS) | ((Flags & EDGE_FLAGS) << HALF_BYTE) | ((Flags & OTHER_EDGE_FLAGS) >> HALF_BYTE);
}

