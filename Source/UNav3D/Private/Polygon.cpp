#include "Polygon.h"

static constexpr int ONE_BYTE = 8;

PolyEdge::PolyEdge(const FVector& _A, const FVector& _B, uint16 TriEdgeFlags) :
	A(_A), B(_B), Flags(A_OK | B_OK | TriEdgeFlags)	
{}

bool PolyEdge::IsAOk() const {
	return Flags & A_OK;	
}

bool PolyEdge::IsBOk() const {
	return Flags & B_OK;	
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

void PolyEdge::SetANotOk() {
	Flags &= ~A_OK;	
}

void PolyEdge::SetBNotOk() {
	Flags &= ~B_OK;	
}

void PolyEdge::FlipTriEdgeFlags() {
	Flags = (Flags & VERTEX_FLAGS) | ((Flags & EDGE_FLAGS) << ONE_BYTE) | ((Flags & OTHER_EDGE_FLAGS) >> ONE_BYTE);
}

