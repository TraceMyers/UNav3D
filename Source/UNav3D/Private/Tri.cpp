﻿#include "Tri.h"
#include <utility>

namespace {
	
	enum TRI_FLAGS {
		TRI_OBSCURED =		0x000f,
		TRI_A_OBSCURED =	0x0001,
		TRI_B_OBSCURED =	0x0002,
		TRI_C_OBSCURED =	0x0004,
		TRI_CHANGED =		0x00f0,
		TRI_CULL =			0x0010,
		TRI_PROBLEM_CASE =	0x0020,
		TRI_TO_POLYGON =	0x0040,
		TRI_ON_BOX_EDGE =	0x0100,
		TRI_INSIDE_BV =		0xf000,
		TRI_A_INSIDE_BV =	0x1000,
		TRI_B_INSIDE_BV =	0x2000,
		TRI_C_INSIDE_BV =	0x4000,
	};

	constexpr float ONE_THIRD = 1.0f / 3.0f;
	FVector ZeroVec (0.0f, 0.0f, 0.0f);
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
	return IsAObscured() || IsBObscured() || IsCObscured();
}

bool Tri::AllObscured() const {
	return IsAObscured() && IsBObscured() && IsCObscured();
}

void Tri::SetOnBoxEdge() {
	Flags |= TRI_ON_BOX_EDGE;	
}

bool Tri::IsOnBoxEdge() const {
	return Flags & TRI_ON_BOX_EDGE;	
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

void Tri::CalculateNormal(FVector& _Normal) const {
	_Normal = FVector::CrossProduct(A - C, A - B).GetUnsafeNormal();
}

FVector Tri::CalculateNormal(const FVector& _A, const FVector& _B, const FVector& _C) {
	return FVector::CrossProduct(_A - _C, _A - _B).GetUnsafeNormal();
}

