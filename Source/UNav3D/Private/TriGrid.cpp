#include "TriGrid.h"
#include "Tri.h"
#include "Geometry.h"
#include "UNavMesh.h"

TriGrid::TriGrid() :
	Container(nullptr), _Num(0), InitSuccess(false)
{}

void TriGrid::Init(const TriMesh& TMesh, const TArray<TempTri>& Tris) {
	Init(TMesh.Box, Tris);	
}

void TriGrid::Init(const UNavMesh& NMesh, const TArray<TempTri>& Tris) {
	Init(NMesh.Box, Tris);
}

void TriGrid::Init(const BoundingBox& BBox, const TArray<TempTri>& Tris) {
	FVector Maximum;
	Geometry::GetAxisAlignedExtrema(BBox, Minimum, Maximum);
	FVector Dimensions = Maximum - Minimum;

	// nudging the container containing the tris outward by .05% of its size
	const float DiagLen = Dimensions.Size();
	const FVector DiagDir = Dimensions * (1.0f / DiagLen);
	const float NudgeAmt = 0.005f * DiagLen;
	Minimum -= DiagDir * NudgeAmt;
	Dimensions += (2.0f * NudgeAmt) * DiagDir;
	HalfDiagLen = 0.5f * DiagLen;
	HalfDimensions = Dimensions * 0.5f;
	QuarterBoxDimensions = Dimensions * (1.0f / (CONTAINER_SIDELEN * 4));

	Center = Minimum + DiagDir * (0.5f * DiagLen);
	
	InvGridFactor = FVector(
		CONTAINER_SIDELEN / Dimensions.X,
		CONTAINER_SIDELEN / Dimensions.Y,
		CONTAINER_SIDELEN / Dimensions.Z
	);

	TArray<TArray<TArray<TArray<const TempTri*>>>> ReorganizedTris;
	ReorganizedTris.Init(TArray<TArray<TArray<const TempTri*>>>(), CONTAINER_SIDELEN);
	for (int i = 0; i < CONTAINER_SIDELEN; i++) {
		auto& TriCols = ReorganizedTris[i];
		TriCols.Init(TArray<TArray<const TempTri*>>(), CONTAINER_SIDELEN);
		for (int j = 0; j < CONTAINER_SIDELEN; j++) {
			TriCols[j].Init(TArray<const TempTri*>(), CONTAINER_SIDELEN);
		}
	}

	const int TriCt = Tris.Num();
	for (int i = 0; i < TriCt; i++) {
		const TempTri* T = &Tris[i];
		FIntVector GridPos;
		const bool Success = WorldToGrid(T->GetCenter(), GridPos);
		if(!Success) {
			printf("TEMP ERROR TriGrid::Init() WorldToGrid fail\n");
			InitSuccess = false;
			return;
		}
		ReorganizedTris[GridPos.X][GridPos.Y][GridPos.Z].Add(T);	
	}
	
	Container = malloc(TriCt * sizeof(Tri));
	if (Container == nullptr) {
		printf("TEMP ERROR TriGrid::Init() alloc fail\n");
		InitSuccess = false;
		return;
	}
	_Num = TriCt;

	Tri* ContainerStart = (Tri*) Container;
	Tri* ContainerCur = ContainerStart;
	for (int i = 0; i < CONTAINER_SIDELEN; i++) {
		for (int j = 0; j < CONTAINER_SIDELEN; j++) {
			for (int k = 0; k < CONTAINER_SIDELEN; k++) {
				TArray<const TempTri*>& BoxTris = ReorganizedTris[i][j][k];
				const int BoxTriCt = BoxTris.Num();
				if (BoxTriCt > 0) {
					TriBox& TBox = Boxes[i][j][k];
					TBox.SetContainer((Tri*)Container);
					TBox.SetNum(BoxTriCt);
					TBox.SetStartIndex(ContainerCur - ContainerStart);
					for (int m = 0; m < BoxTriCt; m++) {
						auto& Temp = *BoxTris[m];
						Tri* T = new (ContainerCur) Tri(*Temp.A, *Temp.B, *Temp.C);
						const FVector* Normal = Temp.Normal;
						if (Normal != nullptr) {
							T->Normal = *Normal;
						}
						ContainerCur++;
					}
				}
			}
		}
	}
}

void TriGrid::Reset() {
	free(Container);
	Container = nullptr;
	_Num = 0;
	InitSuccess = false;
}

TriContainer& TriGrid::GetNearbyTris(const Tri& T) {
	Outgoing.Clear();
	const FVector ToCenter = Center - T.A;
	const float Distance = Center.Size();
	// if (Distance > T.LongestSidelen + HalfDiagLen) {
	// 	return Outgoing;
	// }
	// FIntVector GridPos;
	// if (WorldToGrid(T.A, GridPos)) {
	// 	SetOutgoing(GridPos);
	// 	return Outgoing;
	// }
	// FVector ClosePosition = T.A;
	// const FVector Manhattan (FMath::Abs(ToCenter.X), FMath::Abs(ToCenter.Y), FMath::Abs(ToCenter.Z));
	// if (Manhattan.X > HalfDimensions.X) {
	// 	const float XDist = Manhattan.X - HalfDimensions.X;
	// 	if (XDist < T.LongestSidelen) {
	// 		ClosePosition.X += FMath::Sign(ToCenter.X) * (XDist + QuarterBoxDimensions.X);
	// 	}
	// 	else {
	// 		return Outgoing;
	// 	}
	// }
	// if (Manhattan.Y > HalfDimensions.Y) {
	// 	const float YDist = Manhattan.Y - HalfDimensions.Y;
	// 	if (YDist < T.LongestSidelen) {
	// 		ClosePosition.Y += FMath::Sign(ToCenter.Y) * (YDist + QuarterBoxDimensions.Y);
	// 	}
	// 	else {
	// 		return Outgoing;
	// 	}
	// }
	// if (Manhattan.Z > HalfDimensions.Z) {
	// 	const float ZDist = Manhattan.Z - HalfDimensions.Z;
	// 	if (ZDist < T.LongestSidelen) {
	// 		ClosePosition.Z += FMath::Sign(ToCenter.Z) * (ZDist + QuarterBoxDimensions.Z);
	// 	}
	// 	else {
	// 		return Outgoing;
	// 	}
	// }
	// if (WorldToGrid(ClosePosition, GridPos)) {
	// 	SetOutgoing(GridPos);
	// }
	return Outgoing;
}

TriContainer& TriGrid::GetNearbyTris(const FVector& Location, float Tolerance) {
	Outgoing.Clear();
	FIntVector GridPos;
	if (WorldToGrid(Location, GridPos)) {
		SetOutgoing(GridPos);	
	}
	return Outgoing;
}

int TriGrid::Num() const {
	return _Num;
}

void TriGrid::SetVertices(FVector* _Vertices) {
	Vertices = _Vertices;	
}

void TriGrid::SetTriAt(int i, FVector* A, FVector* B, FVector* C) {
	Tri* T = new ((Tri*)Container + i) Tri(*A, *B, *C);
}

void TriGrid::GetVIndices(int i, TArray<int32>& Indices) const {
	const Tri& T = ((Tri*)Container)[i];
	Indices.Add(&T.A - Vertices);
	Indices.Add(&T.B - Vertices);
	Indices.Add(&T.C - Vertices);
}

void TriGrid::GetVIndices(int i, uint32* Indices) const {
	const Tri& T = ((Tri*)Container)[i];
	Indices[0] = &T.A - Vertices;
	Indices[1] = &T.B - Vertices;
	Indices[2] = &T.C - Vertices;
}

int TriGrid::GetVIndex(const FVector* V) const {
	return V - Vertices;
}

int TriGrid::GetIndex(const Tri* T) const {
	return T - ((Tri*)Container);
}

void TriGrid::SetOutgoing(const FIntVector& GridPos) {
	constexpr int CONTAINER_SIDELEN_M1 = CONTAINER_SIDELEN - 1;
	const int XMin = GridPos.X == 0 ? 0 : GridPos.X - 1;
	const int XMax = GridPos.X == CONTAINER_SIDELEN_M1 ? CONTAINER_SIDELEN_M1 : GridPos.X + 1;
	const int YMin = GridPos.Y == 0 ? 0 : GridPos.Y - 1;
	const int YMax = GridPos.Y == CONTAINER_SIDELEN_M1 ? CONTAINER_SIDELEN_M1 : GridPos.Y + 1;
	const int ZMin = GridPos.Z == 0 ? 0 : GridPos.Z - 1;
	const int ZMax = GridPos.Z == CONTAINER_SIDELEN_M1 ? CONTAINER_SIDELEN_M1 : GridPos.Z + 1;
	TArray<TriBox*> NearbyBoxes;
	NearbyBoxes.Add(&Boxes[GridPos.X][GridPos.Y][GridPos.Z]);
	for (int x = XMin; x <= XMax; x++) {
		for (int y = YMin; y < YMax; y++) {
			for (int z = ZMin; z < ZMax; z++) {
				auto& Box = Boxes[x][y][z];
				if (
					(x != GridPos.X || y != GridPos.Y || z != GridPos.Z)
					&& Box.Num() > 0
				) {
					NearbyBoxes.Add(&Box);
				}	
			}
		}
	}
	Outgoing.SetBoxes(NearbyBoxes);
}

bool TriGrid::WorldToGrid(const FVector& WorldPosition, FIntVector& GridPosition) const {
	GridPosition = FIntVector((WorldPosition - Minimum) * InvGridFactor);
	if (
		GridPosition.X < 0 || GridPosition.X >= CONTAINER_SIDELEN
		|| GridPosition.Y < 0 || GridPosition.Y >= CONTAINER_SIDELEN
		|| GridPosition.Z < 0 || GridPosition.Z >= CONTAINER_SIDELEN
	) {
		return false;
	}
	return true;
}


