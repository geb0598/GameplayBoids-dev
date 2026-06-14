#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BoidTypes.h"
#include "BoidGrid.h"
#include "BoidSlotMap.h"
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

	/**
	 * @brief Applies a one-shot radial velocity impulse to every boid within Radius of Center.
	 *
	 * Positive Impulse pushes boids outward (explosion); negative pulls them inward (implosion).
	 * The kick is scaled by each species' Mass (heavier boids move less) and falls off linearly to
	 * zero at the edge.
	 *
	 * @param Center   World-space center of the impulse.
	 * @param Radius   Boids within this distance are affected.
	 * @param Impulse  Velocity change at the center; sign chooses push (+) or pull (-).
	 */
	UFUNCTION(BlueprintCallable, Category = "GameplayBoids")
	void AddRadialImpulse(const FVector& Center, float Radius, float Impulse);

	/** Registers an obstacle boids will be kept out of; returns a handle to update or remove it. */
	UFUNCTION(BlueprintCallable, Category = "GameplayBoids")
	FBoidObstacleHandle AddObstacle(const FBoidObstacle& Obstacle);

	/** Replaces a registered obstacle's data (e.g. a wall that grows out of the floor). */
	UFUNCTION(BlueprintCallable, Category = "GameplayBoids")
	void UpdateObstacle(const FBoidObstacleHandle& Handle, const FBoidObstacle& Obstacle);

	/** Unregisters an obstacle; its handle becomes stale. */
	UFUNCTION(BlueprintCallable, Category = "GameplayBoids")
	void RemoveObstacle(const FBoidObstacleHandle& Handle);

	/** Removes every registered obstacle. */
	UFUNCTION(BlueprintCallable, Category = "GameplayBoids")
	void ClearObstacles();

private:
	void CreateSpeciesRenderers();

	void SpawnInitialBoids();

	/** Sim params of the species that owns the boid at the given dense index. */
	const FBoidSimParams& ParamsFor(int32 Index) const;

	FVector3f SteerTowards(const FVector3f& Direction, const FVector3f& Velocity, const FBoidSimParams& Params) const;

	FVector3f ComputeBoundsForce(int32 Index) const;

	FVector3f ComputeSteeringForce(int32 Index) const;

	void Integrate(float DeltaTime);

	/** Pushes any boid found inside a registered obstacle back out to its surface (slides along it). */
	void ResolveObstacles();

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

	/** Obstacles registered at BeginPlay (static level geometry); runtime ones use AddObstacle. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	TArray<FBoidObstacle> InitialObstacles;

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

	/** Draw registered obstacles. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawObstacles = false;

	// --- Per-boid debug: drawn for the first DebugSampleCount boids only (too heavy for all) ---

	/** How many boids get the detailed per-boid debug below. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug", meta = (ClampMin = "0"))
	int32 DebugSampleCount = 8;

	/** Arrow along each sampled boid's velocity. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoidVelocity = false;

	/** Arrow along each sampled boid's current steering force. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoidForce = false;

	/** Perception radius sphere around each sampled boid. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoidPerception = false;

	/** Separation radius sphere around each sampled boid. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoidSeparation = false;

	/** Field-of-view cone (along velocity, out to the perception radius) for each sampled boid. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoidFOV = false;

	/** Collision radius sphere around each sampled boid. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids|Debug")
	bool bDrawBoidCollision = false;

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

	/** Stable handles over the swap-removed boid arrays. */
	TBoidSlotMap<FBoidHandle> BoidSlots;

	// --- Registered obstacles (dense) ---

	TArray<FBoidObstacle> Obstacles;

	/** Stable handles over the swap-removed obstacle array. */
	TBoidSlotMap<FBoidObstacleHandle> ObstacleSlots;
};
