#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BoidTypes.h"
#include "BoidGrid.h"
#include "BoidFlockComponent.generated.h"

class UBoidSpeciesAsset;
class UInstancedStaticMeshComponent;

/**
 * @brief One species in a flock: which species asset to spawn, and how many.
 */
USTRUCT(BlueprintType)
struct FBoidSpeciesEntry
{
	GENERATED_BODY()

	/** Species definition (mesh + behavior). */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	TObjectPtr<UBoidSpeciesAsset> Asset;

	/** How many boids of this species to spawn. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids", meta = (ClampMin = "0"))
	int32 Count = 500;
};

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
	void CreateSpeciesRenderers();

	void SpawnInitialBoids();

	/** Sim params of the species that owns the boid at the given dense index. */
	const FBoidSimParams& ParamsFor(int32 Index) const;

	FVector3f SteerTowards(const FVector3f& Direction, const FVector3f& Velocity, const FBoidSimParams& Params) const;

	FVector3f ComputeBoundsForce(int32 Index) const;

	FVector3f ComputeSteeringForce(int32 Index) const;

	void Integrate(float DeltaTime);

	void UpdateRenderInstances();

	void DrawDebug() const;

	// --- Config (editable in the owning actor's details panel) ---

	/** Species in this flock; the array index is each boid's SpeciesId. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	TArray<FBoidSpeciesEntry> Species;

	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	FBoidGrid Grid;

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

	/** Draw the soft-bounds volume boids are kept within. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBounds = false;

	// --- Per-species renderers: one instanced-mesh component per Species entry ---

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SpeciesRenderers;

	/** Reused per frame to bucket boid transforms by species before updating the renderers. */
	TArray<TArray<FTransform>> SpeciesTransforms;

	// --- Per-boid SoA state: parallel arrays sharing one index per boid ---

	TArray<FVector3f> Positions;

	TArray<FVector3f> Velocities;

	/** Species index of each boid, into the Species / SpeciesRenderers arrays. */
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
