#include "DoubleVector.h"

DoubleVector::DoubleVector() :
	X(0.0), Y(0.0), Z(0.0)
{}

DoubleVector::DoubleVector(const DoubleVector& V) {
	memcpy(this, &V, sizeof(DoubleVector));
}

DoubleVector::DoubleVector(const FVector& V) :
	X(V.X), Y(V.Y), Z(V.Z)
{}

DoubleVector::DoubleVector(double _X, double _Y, double _Z) :
	X(_X), Y(_Y), Z(_Z)
{}

DoubleVector DoubleVector::operator*(double D) const {
	return DoubleVector(X * D, Y * D, Z * D);	
}

double DoubleVector::SizeSquared(const FVector& V) {
	const DoubleVector D(V);
	return D.X * D.X + D.Y * D.Y + D.Z * D.Z;
}

double DoubleVector::DotProduct(const FVector& A, const FVector& B) {
	const DoubleVector DA(A), DB(B);
	return DA.X * DB.X + DA.Y * DB.Y + DA.Z * DB.Z;
}

double DoubleVector::DotProduct(const DoubleVector& A, const DoubleVector& B) {
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}