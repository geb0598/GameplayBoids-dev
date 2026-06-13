#include "BoidGrid.h"

void FBoidGrid::Build(TConstArrayView<FVector3f> Positions, const FVector3f& Center)
{
	NumBoids = Positions.Num();
	if (NumBoids == 0)
	{
		return;
	}

	Origin = Center - HalfExtent;
	Dims = FIntVector(
		FMath::Max(1, FMath::CeilToInt(2.f * HalfExtent.X / CellSize)),
		FMath::Max(1, FMath::CeilToInt(2.f * HalfExtent.Y / CellSize)),
		FMath::Max(1, FMath::CeilToInt(2.f * HalfExtent.Z / CellSize)));

	const int32 NumCells = Dims.X * Dims.Y * Dims.Z;

	CellStart.Reset();
	CellStart.SetNumZeroed(NumCells + 1);
	SortedIndices.SetNumUninitialized(NumBoids);
	CellOfBoid.SetNumUninitialized(NumBoids);
	CellCursor.SetNumUninitialized(NumCells);

	for (int32 i = 0; i < NumBoids; ++i)
	{
		const int32 Cell = FlattenIndex(CellCoordsClamped(Positions[i]));
		CellOfBoid[i] = Cell;
		++CellStart[Cell + 1];
	}

	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		CellStart[Cell + 1] += CellStart[Cell];
	}

	FMemory::Memcpy(CellCursor.GetData(), CellStart.GetData(), NumCells * sizeof(int32));
	for (int32 i = 0; i < NumBoids; ++i)
	{
		SortedIndices[CellCursor[CellOfBoid[i]]++] = i;
	}
}
