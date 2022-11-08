#pragma once

// Used before populating a TriGrid
struct TempTri {
	
	TempTri(FVector* _A, FVector* _B, FVector* _C, FVector* _Normal=nullptr) :
		A(_A), B(_B), C(_C), Normal(_Normal)
	{}

	FVector GetCenter() const;
	
	FVector* A;	
	FVector* B;
	FVector* C;
	FVector* Normal;
};

// Triangle; saves space by storing references to vertices on the TriMesh.
// Edges must be re-pointed if Tri's container reallocates
struct Tri {

	Tri();
	
	Tri(FVector& _A, FVector& _B, FVector& _C);
	
	// meant for use in constructor
	static float GetLongestTriSidelenSq(const FVector& A, const FVector& B, const FVector& C);

	// get the area of a triangle with vertices A, B, C
	static float GetArea(const FVector& A, const FVector& B, const FVector& C);

	// get the area of tri T
	static float GetArea(const Tri& T);

	FVector GetCenter() const;

	// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
	inline bool IsAObscured() const;
	
	// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
	inline bool IsBObscured() const;
	
	// asking if this vertex is inside any other meshes; need to be set by Geometry::FlagObscuredTris() first
	inline bool IsCObscured() const;

	// asking if this vertex is inside bounds volume its mesh belongs to
	inline bool IsAInsideBV() const;
	
	// asking if this vertex is inside bounds volume its mesh belongs to
	inline bool IsBInsideBV() const;
	
	// asking if this vertex is inside bounds volume its mesh belongs to
	inline bool IsCInsideBV() const;

	// asking if the entire tri inside the bounds volume its mesh belongs to
	inline bool IsInsideBV() const;

	// asking if any vertex is inside the bounds volume its mesh belongs to
	inline bool AnyInsideBV() const;

	inline bool IsCull() const;

	inline bool IsProblemCase() const;

	inline bool IsMarkedForPolygon() const;

	// mark A as inside another mesh
	inline void SetAObscured();
	
	// mark B as inside another mesh
	inline void SetBObscured();
	
	// mark C as inside another mesh
	inline void SetCObscured();

	// mark A as inside its bounds volume
	inline void SetAInsideBV();
	
	// mark B as inside its bounds volume
	inline void SetBInsideBV();
	
	// mark C as inside its bounds volume
	inline void SetCInsideBV();

	inline void SetInsideBV();

	// are any vertices obscured?
	inline bool AnyObscured() const;

	// are all vertices obscured?
	inline bool AllObscured() const;

	inline void SetOnBoxEdge();

	inline bool IsOnBoxEdge() const;

	inline bool IsChanged() const;
	
	void MarkForCull();

	void MarkProblemCase();

	void MarkForPolygon();

	void CalculateNormal(FVector& Normal) const;

	static FVector CalculateNormal(const FVector &A, const FVector& B, const FVector& C);

	FVector& A;
	FVector& B;
	FVector& C;
	FVector Normal;
	uint32 Flags;
	// -- for faster intersection checking --
	float Area; 
	float LongestSidelenSq;
	
};