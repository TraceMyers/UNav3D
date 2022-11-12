#include "Tri.h"

namespace {

	constexpr int NEIGHBOR_0_SHIFT = 10;
	constexpr int NEIGHBOR_1_SHIFT = 13;
	constexpr int NEIGHBOR_2_SHIFT = 16;
	constexpr float ONE_THIRD = 1.0f / 3.0f;
	FVector ZeroVec (0.0f, 0.0f, 0.0f);
	
	enum TRI_FLAGS {
		// whether the vertices are inside other meshes
		TRI_A_OBSCURED =	0x00000001,
		TRI_B_OBSCURED =	0x00000002,
		TRI_C_OBSCURED =	0x00000004,
		TRI_OBSCURED =		TRI_A_OBSCURED | TRI_B_OBSCURED | TRI_C_OBSCURED,
		// for the intersections-to-polygons process
		TRI_CULL =			0x00000008,
		TRI_PROBLEM_CASE =	0x00000010,
		TRI_TO_POLYGON =	0x00000020,
		TRI_CHANGED =		TRI_CULL | TRI_PROBLEM_CASE | TRI_TO_POLYGON,
		// whether vertices are inside the bounds volume
		TRI_A_INSIDE_BV =	0x00000040,
		TRI_B_INSIDE_BV =	0x00000080,
		TRI_C_INSIDE_BV =	0x00000100,
		TRI_INSIDE_BV =		TRI_A_INSIDE_BV | TRI_B_INSIDE_BV | TRI_C_INSIDE_BV,
		// cleared before flags above used
		TRI_SEARCHED =		0x00000001,
		// neighbors, which side? only valid directly after the batching phase; otherwise, neighbors are always AB,BC,CA
		TRI_AB =			0x00000001,
		TRI_BC =			0x00000002,
		TRI_CA =			0x00000004,
		TRI_NEIGHBOR_0 =	(TRI_AB | TRI_BC | TRI_CA) << NEIGHBOR_0_SHIFT,
		TRI_NEIGHBOR_1 =	(TRI_AB | TRI_BC | TRI_CA) << NEIGHBOR_1_SHIFT,
		TRI_NEIGHBOR_2 =	(TRI_AB | TRI_BC | TRI_CA) << NEIGHBOR_2_SHIFT,
		// The last two share the same space and are useful at different stages of processing
		// group number in last 1.5 bytes; allows for 4095 groups per batch
		TRI_IN_GROUP =		0xfff00000,
		// batch number; allows for 4095 batches per mesh; placement changes during processing
		TRI_BATCH_BFRGRP =	0x000fff00,
		TRI_BATCH_AFTGRP =	0xfff00000,
	};
	uint32 TRI_BATCH = TRI_BATCH_BFRGRP;
	int TRI_BATCH_SHIFT = 8;
}

FVector TempTri::GetCenter() const {
	return (*A + *B + *C) * ONE_THIRD;
}

Tri::Tri() :
	A(ZeroVec), B(ZeroVec), C(ZeroVec),
	Normal(FVector::OneVector),
	Flags(0x0),
	Area(0.0f),
	LongestSidelenSq(0.0f)
{
	Neighbors.Reserve(3);	
}

Tri::Tri(FVector& _A, FVector& _B, FVector& _C) :
	A(_A), B(_B), C(_C),
	Normal(FVector::CrossProduct(_A - _C, _A - _B).GetUnsafeNormal()),
	Flags(0x0),
	Area(GetArea(_A, _B, _C)),
	LongestSidelenSq(GetLongestTriSidelenSq(_A, _B, _C))
{
	Neighbors.Reserve(3);	
}

float Tri::GetLongestTriSidelenSq(const FVector& A, const FVector& B, const FVector& C) {
	const float AB = (A - B).SizeSquared();
	const float BC = (B - C).SizeSquared();
	const float CA = (C - A).SizeSquared();
	if (AB > BC) {
		if (AB > CA) {
			return AB;
		}
		return CA;
	}
	if (BC > CA) {
		return BC;
	}
	return CA;
}

float Tri::GetArea(const FVector& A, const FVector& B, const FVector& C) {
	return FVector::CrossProduct(A - B, A - C).Size() * 0.5f;
}

float Tri::GetArea(const Tri& T) {
	return FVector::CrossProduct(T.A - T.B, T.A - T.C).Size() * 0.5f;
}

FVector Tri::GetCenter() const {
	return (A + B + C) * ONE_THIRD;
}

void Tri::AddNeighborFlag(uint32& ExtFlags, int V0, int V1, int SideNo) {
	uint32 SideFlag;
	switch(V0) {
	case 0:
		switch(V1) {
		case 1:
			SideFlag = TRI_AB;
			break;
		case 2:
			SideFlag = TRI_CA;
			break;
		default:
			check(V1 < 1 || V1 > 2)
			return;
		}
		break;
	case 1:
		switch(V1) {
		case 0:
			SideFlag = TRI_AB;
			break;
		case 2:
			SideFlag = TRI_BC;
			break;
		default:
			check(V1 != 0 && V1 != 2)
			return;
		}
		break;
	case 2:
		switch(V1) {
		case 0:
			SideFlag = TRI_CA;
			break;
		case 1:
			SideFlag = TRI_BC;
			break;
		default:
			check(V1 < 0 || V1 > 1)
			return;
		}
		break;
	default:
		check(V0 < 0 || V0 > 2)
		return;
	}	
	ExtFlags |= SideFlag << (10 + 3 * SideNo);
}

void Tri::AddFlags(uint32 _Flags) {
	Flags |= _Flags;
}

void Tri::OrganizeNeighbors() {
	while (Neighbors.Num() < 3) {
		Neighbors.Add(nullptr);	
	}
	Tri* ABTri = nullptr;
	Tri* BCTri = nullptr;
	Tri* CATri = nullptr;
	const uint32 Neighbor0 = Flags & TRI_NEIGHBOR_0;
	const uint32 Neighbor1 = Flags & TRI_NEIGHBOR_1;
	const uint32 Neighbor2 = Flags & TRI_NEIGHBOR_2;
	if (Neighbor0) {
		if (Neighbor0 == TRI_AB << NEIGHBOR_0_SHIFT) {
			ABTri = Neighbors[0];
		}
		else if (Neighbor0 == TRI_BC << NEIGHBOR_0_SHIFT) {
			BCTri = Neighbors[0];
		}
		else {
			CATri = Neighbors[0];
		}
		if (Neighbor1) {
			if (Neighbor1 == TRI_AB << NEIGHBOR_1_SHIFT) {
				ABTri = Neighbors[1];
			}
			else if (Neighbor1 == TRI_BC << NEIGHBOR_1_SHIFT) {
				BCTri = Neighbors[1];
			}
			else {
				CATri = Neighbors[1];
			}
			if (Neighbor2) {
				if (Neighbor2 == TRI_AB << NEIGHBOR_2_SHIFT) {
					ABTri = Neighbors[2];
				}
				else if (Neighbor2 == TRI_BC << NEIGHBOR_2_SHIFT) {
					BCTri = Neighbors[2];
				}
				else {
					CATri = Neighbors[2];
				}
			}
		}
	}
	Neighbors[0] = ABTri;
	Neighbors[1] = BCTri;
	Neighbors[2] = CATri;
}

void Tri::ClearFlags() {
	Flags = 0x0;
}

void Tri::ShiftBatchFlags() {
	TRI_BATCH_SHIFT += 12;
	TRI_BATCH = TRI_BATCH_AFTGRP;
}

bool Tri::IsAObscured() const {
	return Flags & TRI_A_OBSCURED;
}

bool Tri::IsBObscured() const {
	return Flags & TRI_B_OBSCURED;
}

bool Tri::IsCObscured() const {
	return Flags & TRI_C_OBSCURED;
}

bool Tri::IsAInsideBV() const {
	return Flags & TRI_A_INSIDE_BV;	
}

bool Tri::IsBInsideBV() const {
	return Flags & TRI_B_INSIDE_BV;	
}

bool Tri::IsCInsideBV() const {
	return Flags & TRI_C_INSIDE_BV;	
}

bool Tri::IsInsideBV() const {
	return (Flags & TRI_INSIDE_BV) == TRI_INSIDE_BV;
}

bool Tri::AnyInsideBV() const {
	return Flags & TRI_INSIDE_BV;
}

bool Tri::IsCull() const {
	return Flags & TRI_CULL;	
}

bool Tri::IsProblemCase() const {
	return Flags & TRI_PROBLEM_CASE;	
}

bool Tri::IsMarkedForPolygon() const {
	return Flags & TRI_TO_POLYGON;
}

void Tri::SetAObscured() {
	Flags |= TRI_A_OBSCURED;	
}

void Tri::SetBObscured() {
	Flags |= TRI_B_OBSCURED;	
}

void Tri::SetCObscured() {
	Flags |= TRI_C_OBSCURED;	
}

void Tri::SetAInsideBV() {
	Flags |= TRI_A_INSIDE_BV;	
}

void Tri::SetBInsideBV() {
	Flags |= TRI_B_INSIDE_BV;	
}

void Tri::SetCInsideBV() {
	Flags |= TRI_C_INSIDE_BV;	
}

void Tri::SetInsideBV() {
	Flags |= TRI_INSIDE_BV;
}

bool Tri::AnyObscured() const {
	return Flags & TRI_OBSCURED;
}

bool Tri::AllObscured() const {
	return (Flags & TRI_OBSCURED) == TRI_OBSCURED;
}

bool Tri::IsChanged() const {
	return Flags & TRI_CHANGED;	
}

void Tri::MarkForCull() {
	Flags |= TRI_CULL;
}

void Tri::MarkProblemCase() {
	Flags |= TRI_PROBLEM_CASE;
}

void Tri::MarkForPolygon() {
	Flags |= TRI_TO_POLYGON;	
}

void Tri::SetBatch(uint16 BatchNo) {
	Flags &= ~TRI_BATCH;
	Flags |= BatchNo << TRI_BATCH_SHIFT;
}

uint16 Tri::GetBatch() const {
	return (Flags & TRI_BATCH) >> TRI_BATCH_SHIFT;	
}

bool Tri::IsInBatch() const {
	return Flags & TRI_BATCH;
}

void Tri::SetGroup(uint16 GroupNo) {
	Flags &= ~TRI_IN_GROUP;
	Flags |= GroupNo << 20;
}

uint16 Tri::GetGroup() const {
	return (Flags & TRI_IN_GROUP) >> 20;	
}

bool Tri::IsInGroup() const {
	return Flags & TRI_IN_GROUP;	
}


void Tri::SetSearched() {
	Flags |= TRI_SEARCHED;	
}

void Tri::UnsetSearched() {
	Flags &= ~TRI_SEARCHED;	
}

bool Tri::IsSearched() const {
	return Flags & TRI_SEARCHED;	
}

void Tri::CalculateNormal(FVector& _Normal) const {
	_Normal = FVector::CrossProduct(A - C, A - B).GetUnsafeNormal();
}

FVector Tri::CalculateNormal(const FVector& _A, const FVector& _B, const FVector& _C) {
	return FVector::CrossProduct(_A - _C, _A - _B).GetUnsafeNormal();
}

