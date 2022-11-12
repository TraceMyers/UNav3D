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

	// because side-to-side connection between tris is disconnected from neighbor adding, it's helpful to have
	// a builder that populates connection flags before assigning to the tri. V0,V1 are in 0,1,2, translating to A,B,C
	// and SideNo is in 0,1,2 for order of adding
	static inline void AddNeighborFlag(uint32& Flags, int V0, int V1, int SideNo);

	// As of 11/11/2022, only used for adding built neighbor flags
	inline void AddFlags(uint32 Flags);

	// neighbors may not be added in order AB, BC, CA; this organizes them so they are, and so every tri always
	// has 3 neighbors, where nonexistent neighbor <- nullptr; neighbors should only be nonexistent when a mesh
	// is not continuous along the surface, which may be true during some stages of reformation
	inline void OrganizeNeighbors();

	inline void ClearFlags();

	static inline void ShiftBatchFlags();

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

	inline bool IsChanged() const;
	
	void MarkForCull();

	void MarkProblemCase();

	void MarkForPolygon();

	void SetBatch(uint16 BatchNo);

	uint16 GetBatch() const;

	bool IsInBatch() const;

	void SetGroup(uint16 GroupNo);
	
	uint16 GetGroup() const;

	bool IsInGroup() const;

	void SetSearched();

	void UnsetSearched();

	bool IsSearched() const;
	
	void CalculateNormal(FVector& Normal) const;

	static FVector CalculateNormal(const FVector &A, const FVector& B, const FVector& C);

	static constexpr int MAX_BATCH_CT = 4095; // per mesh
	static constexpr int MAX_GROUP_CT = 4095; // per batch
	
	enum {AB=0, BC=1, CA=2};
	
	FVector& A;
	FVector& B;
	FVector& C;
	FVector Normal;
	uint32 Flags;
	TArray<Tri*> Neighbors;
	// -- for faster intersection checking --
	float Area; 
	float LongestSidelenSq;
	
};