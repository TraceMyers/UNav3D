#pragma once

// Triangle; saves space by storing references to vertices on the TriMesh.
// Edges must be re-pointed if Tri's container reallocates
struct Tri {
	
	Tri(const FVector& _A, const FVector& _B, const FVector& _C);

	// meant for use in constructor
	static float GetLongestTriSidelen(const FVector& A, const FVector& B, const FVector& C);

	// get the area of a triangle with vertices A, B, C
	static float GetArea(const FVector& A, const FVector& B, const FVector& C);

	// get the area of tri T
	static float GetArea(const Tri& T);

	// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
	inline bool IsAObscured() const;
	
	// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
	inline bool IsBObscured() const;
	
	// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
	inline bool IsCObscured() const;

	// mark A as inside another mesh
	inline void SetAObscured();
	
	// mark B as inside another mesh
	inline void SetBObscured();
	
	// mark C as inside another mesh
	inline void SetCObscured();

	// are any vertices obscured?
	inline bool AnyObscured() const;

	// are all vertices obscured?
	inline bool AllObscured() const;

	void MarkForCull();

	void MarkProblemCase();

	// 48 bytes
	const FVector& A;
	const FVector& B;
	const FVector& C;
	const FVector Normal;
	uint32 Flags; // only using 2 of the 4 bytes
	// for faster intersection checking
	const float LongestSidelen;
	const float Area;
	
};