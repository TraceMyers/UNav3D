#pragma once
#include "Tri.h"

struct TriMesh;
struct TempTri;
struct UNavMesh;
struct BoundingBox;

struct TriBox {
	
	TriBox() :
		Container(nullptr), StartIndex(0), _Num(0)
	{}

	int Num() const {
		return _Num;
	}

	Tri& operator [] (int i) const {
		return Container[StartIndex + i];
	}

	// for use in TriGrid.cpp only
	void SetContainer(Tri* _Container) {
		Container = _Container;	
	}	

	// for use in TriGrid.cpp only
	void SetStartIndex(int i) {
		StartIndex = i;
	}
	
	// for use in TriGrid.cpp only
	void SetNum(int Number) {
		_Num = Number;	
	}
	
private:

	Tri* Container;
	int StartIndex;
	int _Num;
	
};

// can be filled with a single TriBox or it can also include data from surrounding TriBoxes
struct TriContainer {

	TriContainer() :
		_Num(0), CurBox(0), CurIndex(0)
	{}

	void Reset() {
		CurBox = 0;
		CurIndex = 0;
	}

	Tri& Next() {
		const TriBox& Box = Boxes[CurBox];
		if (CurIndex >= Box.Num()) {
			CurIndex = 1;
			return Boxes[++CurBox][0];
		}
		return Box[CurIndex++];		
	}

	int Num() const {
		return _Num;
	}

	// does not reset Boxes
	void Clear() {
		memset(&_Num, 0, sizeof(int) * 3);
	}

	// only supply boxes with Num() > 0
	void SetBoxes(const TArray<TriBox*>& _Boxes) {
		for (int i = 0; i < _Boxes.Num(); i++) {
			const auto& Box = _Boxes[i];
			Boxes[i] = *Box;
			_Num += Box->Num();
		}	
	}
	
private:
	
	int _Num;
	int CurBox;
	int CurIndex;
	// TODO: this needs to be a TArray to account for the fact that a triangle may span more than 27 boxes
	TriBox Boxes[27]; 
	
};

class TriGrid {

public:
	
	TriGrid();
	
	void Init(const TriMesh& TMesh, const TArray<TempTri>& Tris);
	
	void Init(const UNavMesh& NMesh, const TArray<TempTri>& Tris);

	void Reset();
	
	TriContainer& GetNearbyTris(const Tri& T);
	
	TriContainer& GetNearbyTris(const FVector& Location, float Tolerance=0.0f);

	inline int Num() const;
	
	Tri& operator [] (int i) const {
		return ((Tri*)Container)[i];
	}

	Tri& operator [] (uint32 i) const {
		return ((Tri*)Container)[i];
	}

	void SetVertices(FVector* Vertices);

	void SetTriAt(int i, FVector* A, FVector* B, FVector* C);

	// returns the index of the vertex; for drawing mesh
	inline void GetVIndices(int i, TArray<int32>& Indices) const;
	
	inline void GetVIndices(int i, uint32* Indices) const;

	inline int GetVIndex(const FVector* V) const;

	inline int GetIndex(const Tri* T) const;
	
private:

	inline void SetOutgoing(const FIntVector& GridPos);
	
	bool WorldToGrid(const FVector& WorldPosition, FIntVector& GridPosition) const;

	void Init(const BoundingBox& BBox, const TArray<TempTri>& Tris);
	
	static constexpr int CONTAINER_SIDELEN = 8;

	void* Container;
	FVector* Vertices;
	int _Num;
	TriBox Boxes[CONTAINER_SIDELEN][CONTAINER_SIDELEN][CONTAINER_SIDELEN];
	bool InitSuccess;
	FVector InvGridFactor;
	FVector Minimum;
	FVector Center;
	FVector HalfDimensions;
	FVector QuarterBoxDimensions;
	float HalfDiagLen;
	TriContainer Outgoing;
		
};
