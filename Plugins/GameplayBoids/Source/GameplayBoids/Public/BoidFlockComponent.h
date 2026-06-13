#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BoidTypes.h"
#include "BoidGrid.h"
#include "BoidFlockComponent.generated.h"

/**
 * @brief A self-contained boid flock, simulated as data-oriented parallel arrays (SoA).
 *
 * Each component is an independent simulation centered on its own world location, so placing
 * several (one per enclosure, or one on the player) gives several independent flocks. A boid
 * is just an index into the parallel arrays; external code refers to one through an FBoidHandle.
 */
UCLASS(ClassGroup = (GameplayBoids), meta = (BlueprintSpawnableComponent))
class GAMEPLAYBOIDS_API UBoidFlockComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBoidFlockComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	FVector3f SteerTowards(const FVector3f& Direction, const FVector3f& Velocity) const;

	FVector3f ComputeBoundsForce(int32 Index) const;

	FVector3f ComputeSteeringForce(int32 Index) const;

	void Integrate(float DeltaTime);

	void DrawDebug() const;

	// --- Config (editable in the owning actor's details panel) ---

	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	FBoidSimParams Params;

	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	FBoidGrid Grid;

	UPROPERTY(EditAnywhere, Category = "GameplayBoids", meta = (ClampMin = "1"))
	int32 MaxBoids = 1000;

	/** Half-size of the volume boids spawn into and are softly kept within, around the owner. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	FVector3f SpawnExtent = FVector3f(1500.f);

	// --- Debug draw (only while the GameplayBoids.DrawDebug CVar is on) ---

	/** Draw a point at each boid. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoids = true;

	/** Draw the grid cells colored by occupancy (heatmap). */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawGrid = false;

	/** Draw the volume boids spawn into. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawSpawnBounds = false;

	/** Draw the soft-bounds volume: outer edge and the inner edge where turn-back begins. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBounds = false;

	// --- Per-boid SoA state: parallel arrays sharing one index per boid ---

	TArray<FVector3f> Positions;

	TArray<FVector3f> Velocities;

	/** Unused for now (all 0); kept so the swap-remove path already handles a third array. */
	TArray<uint8> SpeciesIds;

	/** Per-boid steering force for the current frame; written by the steering pass, divided by Mass and integrated by Integrate. */
	TArray<FVector3f> Forces;

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
