#pragma once

#include "CoreMinimal.h"
#include "BoidGrid.generated.h"

/**
 * @brief Uniform spatial grid for boid neighbor queries.
 *
 * Fixed cell size and extent, rebuilt from scratch every frame with a counting sort and
 * re-centered on the build center so it follows the player for free.
 */
USTRUCT()
struct GAMEPLAYBOIDS_API FBoidGrid
{
	GENERATED_BODY()

public:
	/** Cell edge length; set to the perception radius so a neighbor query spans a 3x3x3 block. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Grid", meta = (ClampMin = "1"))
	float CellSize = 200.f;

	/** Half-size of the region covered, centered on the build center. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Grid", meta = (ClampMin = "0"))
	FVector3f HalfExtent = FVector3f(2000.f);

	/**
	 * @brief Rebuilds the grid around Center from the current boid positions.
	 *
	 * Call once per frame before querying. Boids are bucketed into cells with a counting sort:
	 * count how many fall in each cell, prefix-sum those counts into per-cell start offsets,
	 * then scatter each boid index into its cell's run. The outcome is SortedIndices grouped
	 * by cell and indexed through CellStart (see those members for the exact layout).
	 */
	void Build(TConstArrayView<FVector3f> Positions, const FVector3f& Center);

	/** True once Build has placed at least one boid. */
	FORCEINLINE bool IsBuilt() const { return NumBoids > 0; }

	/**
	 * @brief Invokes Func for every boid in cells overlapping the query sphere.
	 *
	 * Visits candidates, not exact hits: a cell may extend past the sphere, so the caller
	 * filters by actual distance. Visit order is deterministic (cell order, then build order).
	 *
	 * @param Center  Query sphere center, world space.
	 * @param Radius  Query sphere radius; cells overlapping it are visited.
	 * @param Func    Called as Func(int32 BoidIndex) for each candidate boid.
	 */
	template <typename FuncType>
	void ForEachBoidInCellRange(const FVector3f& Center, float Radius, FuncType&& Func) const
	{
		if (!IsBuilt())
		{
			return;
		}

		const FIntVector MinCoords = CellCoordsClamped(Center - FVector3f(Radius));
		const FIntVector MaxCoords = CellCoordsClamped(Center + FVector3f(Radius));

		for (int32 Z = MinCoords.Z; Z <= MaxCoords.Z; ++Z)
		{
			for (int32 Y = MinCoords.Y; Y <= MaxCoords.Y; ++Y)
			{
				for (int32 X = MinCoords.X; X <= MaxCoords.X; ++X)
				{
					const int32 Cell = FlattenIndex(X, Y, Z);
					for (int32 i = CellStart[Cell]; i < CellStart[Cell + 1]; ++i)
					{
						Func(SortedIndices[i]);
					}
				}
			}
		}
	}
	
	/**
	 * @brief Invokes Func for every cell in the grid, including empty ones; for debug visualization.
	 *
	 * @param Func  Called as Func(const FBox& CellBounds, int32 BoidCount) per cell.
	 */
	template <typename FuncType>
	void ForEachCell(FuncType&& Func) const
	{
		if (!IsBuilt())
		{
			return;
		}

		for (int32 Z = 0; Z < Dims.Z; ++Z)
		{
			for (int32 Y = 0; Y < Dims.Y; ++Y)
			{
				for (int32 X = 0; X < Dims.X; ++X)
				{
					const int32 Cell = FlattenIndex(X, Y, Z);
					const int32 Count = CellStart[Cell + 1] - CellStart[Cell];
					const FVector3f Min = Origin + FVector3f(X, Y, Z) * CellSize;
					Func(FBox(FVector(Min), FVector(Min + FVector3f(CellSize))), Count);
				}
			}
		}
	}

private:
	FORCEINLINE int32 FlattenIndex(int32 X, int32 Y, int32 Z) const
	{
		return X + Dims.X * Y + Dims.X * Dims.Y * Z;
	}

	FORCEINLINE int32 FlattenIndex(FIntVector Coord) const
	{
		return FlattenIndex(Coord.X, Coord.Y, Coord.Z);
	}

	FORCEINLINE FIntVector CellCoordsClamped(const FVector3f& Position) const
	{
		const FVector3f Local = (Position - Origin) / CellSize;
		return FIntVector(
			FMath::Clamp(FMath::FloorToInt(Local.X), 0, Dims.X - 1),
			FMath::Clamp(FMath::FloorToInt(Local.Y), 0, Dims.Y - 1),
			FMath::Clamp(FMath::FloorToInt(Local.Z), 0, Dims.Z - 1));
	}

	FVector3f Origin = FVector3f::ZeroVector;
	
	FIntVector Dims = FIntVector(1, 1, 1);
	
	int32 NumBoids = 0;

	/**
	 * Per-cell start offsets into SortedIndices, prefix-summed (size NumCells + 1).
	 * The boids in cell c occupy SortedIndices[CellStart[c], CellStart[c + 1]).
	 */
	TArray<int32> CellStart;

	/** Boid indices grouped by cell; read a cell's run via the CellStart offsets. */
	TArray<int32> SortedIndices;

	/** Build-time temporary: the cell each boid landed in, cached during counting and reused while scattering. */
	TArray<int32> CellOfBoid;

	/** Build-time temporary: per-cell write cursor advanced during the scatter pass (size NumCells). */
	TArray<int32> CellCursor;
};
