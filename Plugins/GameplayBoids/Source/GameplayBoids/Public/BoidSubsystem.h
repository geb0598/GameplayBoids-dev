#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoidTypes.h"
#include "BoidGrid.h"
#include "BoidSubsystem.generated.h"

/**
 * @brief Owns and simulates all boids as data-oriented parallel arrays (SoA).
 *
 * A boid is not an actor: it is just an index shared across the Positions/Velocities/
 * SpeciesIds arrays. External code refers to a boid through an FBoidHandle, which stays
 * valid across the swap-removes used on despawn, and never touches the arrays directly.
 */
UCLASS()
class GAMEPLAYBOIDS_API UBoidSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	
	virtual void Tick(float DeltaTime) override;
	
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBoidSubsystem, STATGROUP_Tickables); }
	
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** Adds a boid and returns a stable handle to it. */
	FBoidHandle SpawnBoid(const FVector3f& Position, const FVector3f& Velocity, uint8 SpeciesId = 0);

	/** Removes the boid at the given dense index by swap-remove; its handle becomes stale. */
	void DespawnBoid(int32 Index);

	/** Current dense index for a handle, or INDEX_NONE if that boid is gone. */
	int32 ResolveHandle(const FBoidHandle& Handle) const;

	/** Builds a handle for the boid currently at the given dense index. */
	FBoidHandle MakeHandle(int32 Index) const;

	FORCEINLINE int32 GetNumBoids() const { return Positions.Num(); }

private:
	void SpawnInitialBoids();
	
	void Integrate(float DeltaTime);
	
	void DrawDebug() const;

	// --- Config (the spawner will drive these later; defaults for now) ---
	
	FBoidSimParams Params;
	
	FBoidGrid Grid;
	
	int32 MaxBoids = 1000;

	/** Half-size of the volume initial boids spawn into, around SimCenter. */
	FVector3f SpawnExtent = FVector3f(1500.f);

	/** Center the grid and spawning track; world origin until the spawner feeds the player position. */
	FVector3f SimCenter = FVector3f::ZeroVector;

	// --- Per-boid SoA state: parallel arrays sharing one index per boid ---
	
	TArray<FVector3f> Positions;
	
	TArray<FVector3f> Velocities;
	
	TArray<uint8> SpeciesIds;

	// --- Slot map: gives handles a stable identity over the swap-removed dense arrays ---
	
	/** Slot -> current dense index, or INDEX_NONE when the slot is free. */
	TArray<int32> SlotToIndex;
	
	/** Slot -> reuse counter; a handle is stale once it no longer matches. */
	TArray<uint32> SlotGeneration;
	
	/** Dense index -> the slot that owns that boid. */
	TArray<int32> IndexToSlot;
	
	/** Slots whose boids died, waiting to be reused. */
	TArray<int32> FreeSlots;
};
