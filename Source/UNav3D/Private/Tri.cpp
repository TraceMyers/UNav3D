#include "Tri.h"
#include <utility>

namespace {
	
	enum TRI_FLAGS {
		TRI_A_OBSCURED =	0x0001,
		TRI_B_OBSCURED =	0x0002,
		TRI_C_OBSCURED =	0x0004,
		TRI_CULL =			0x0008,
		TRI_PROBLEM_CASE =	0x0010,
		TRI_TO_POLYGON =	0x0020,
	};

	constexpr float ONE_THIRD = 1.0f / 3.0f;
	static FVector ZeroVec (0.0f, 0.0f, 0.0f);
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
{}

Tri::Tri(FVector& _A, FVector& _B, FVector& _C) :
	A(_A), B(_B), C(_C),
	Normal(FVector::CrossProduct(_A - _C, _A - _B).GetUnsafeNormal()),
	Flags(0x0),
	Area(GetArea(_A, _B, _C)),
	LongestSidelenSq(GetLongestTriSidelenSq(_A, _B, _C))
{}

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

bool Tri::IsAObscured() const {
	return Flags & TRI_A_OBSCURED;
}

bool Tri::IsBObscured() const {
	return Flags & TRI_B_OBSCURED;
}

bool Tri::IsCObscured() const {
	return Flags & TRI_C_OBSCURED;
}

bool Tri::IsTriCull() const {
	return Flags & TRI_CULL;	
}

bool Tri::IsTriProblemCase() const {
	return Flags & TRI_PROBLEM_CASE;	
}

bool Tri::IsTriMarkedForPolygon() const {
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

bool Tri::AnyObscured() const {
	return IsAObscured() || IsBObscured() || IsCObscured();
}

bool Tri::AllObscured() const {
	return IsAObscured() && IsBObscured() && IsCObscured();
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
