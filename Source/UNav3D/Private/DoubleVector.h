#pragma once

struct DoubleVector {

	DoubleVector();
	DoubleVector(const DoubleVector& V);
	DoubleVector(const FVector& V);
	DoubleVector(double X, double Y, double Z);

	DoubleVector operator*(double D) const;
	
	static double SizeSquared(const FVector& V);
	static double DotProduct(const FVector& A, const FVector& B);
	static double DotProduct(const DoubleVector& A, const DoubleVector& B);

	double X;
	double Y;
	double Z;
};