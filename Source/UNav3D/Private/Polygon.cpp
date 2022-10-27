#include "Polygon.h"

enum POLYEDGE_FLAGS {
	A_OK =	0x0001,
	B_OK =	0x0002,
};
	
	
PolyEdge::PolyEdge(const FVector& _A, const FVector& _B) :
	A(_A), B(_B), Flags(0xffff)	
{}

bool PolyEdge::GetAOk() const {
	return Flags & A_OK;	
}

bool PolyEdge::GetBOk() const {
	return Flags & B_OK;	
}

void PolyEdge::SetANotOk() {
	Flags &= ~A_OK;	
}

void PolyEdge::SetBNotOk() {
	Flags &= ~B_OK;	
}