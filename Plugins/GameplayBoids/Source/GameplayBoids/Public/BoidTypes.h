#pragma once

#include "CoreMinimal.h"
#include "BoidTypes.generated.h"

/**
 * @brief Stable external reference to a single boid.
 *
 * Boids live in dense SoA arrays whose indices shift on swap-remove, so external code
 * holds a handle and resolves it to the current index through the subsystem.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidHandle
{
	GENERATED_BODY()

	/** Slot map slot, stable for the boid's lifetime (not the dense array index). */
	UPROPERTY()
	int32 Slot = INDEX_NONE;

	/** Reuse counter; a handle is stale once it no longer matches its slot's generation. */
	UPROPERTY()
	uint32 Generation = 0;

	FORCEINLINE bool IsSet() const { return Slot != INDEX_NONE; }
};

/**
 * @brief Steering and locomotion tuning shared by a flock.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidSimParams
{
	GENERATED_BODY()

	// --- Movement ---

	/** Lower speed clamp, so boids never stall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Movement", meta = (ClampMin = "0"))
	float MinSpeed = 300.f;

	/** Upper speed clamp under normal steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Movement", meta = (ClampMin = "0"))
	float MaxSpeed = 600.f;

	/** Per-rule steering force cap before weighting; higher = snappier turns (effective agility is MaxSteerForce / Mass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Movement", meta = (ClampMin = "0"))
	float MaxSteerForce = 600.f;

	/** Inertia: steering forces are divided by this to get acceleration. Heavier boids react more sluggishly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Movement", meta = (ClampMin = "0.01"))
	float Mass = 1.f;

	/** Decay rate (1/s) pulling impulse-boosted speed back down to MaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Movement", meta = (ClampMin = "0"))
	float OverSpeedDamping = 3.f;

	// --- Behavior ---

	/** Radius within which other boids count as flockmates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float PerceptionRadius = 400.f;

	/** Flockmates closer than this are pushed away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float SeparationRadius = 150.f;

	/** View cone (centered on velocity); flockmates outside it are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0", ClampMax = "360"))
	float FieldOfViewDegrees = 270.f;

	/** Strength of the push away from nearby flockmates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float SeparationWeight = 2.f;

	/** Strength of matching neighbors' heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float AlignmentWeight = 1.2f;

	/** Strength of steering toward the neighbors' center of mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float CohesionWeight = 1.f;

	/** Max flockmates considered per boid; caps worst-case cost when boids clump up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "1"))
	int32 MaxNeighbors = 16;

	// --- Bounds ---

	/** Strength of the turn-back force at the simulation bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Bounds", meta = (ClampMin = "0"))
	float BoundsWeight = 4.f;

	/** Distance inside the edge where the turn-back force begins ramping up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Bounds", meta = (ClampMin = "1"))
	float BoundsMargin = 300.f;
};

UENUM()
enum class EBoidObstacleShape : uint8
{
	Sphere
};

/**
 * @brief A convex obstacle boids are kept from penetrating, as a tagged shape.
 *
 * Boids never trace against meshes; instead each obstacle answers SignedDistance(P), and the
 * flock pushes any boid found inside back out to the surface.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidObstacle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids")
	EBoidObstacleShape Shape = EBoidObstacleShape::Sphere;

	/** World-space center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids")
	FVector3f Center = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids", meta = (ClampMin = "0"))
	float Radius = 300.f;

	/** Signed distance from Position to the surface (negative inside); OutNormal points outward. */
	float SignedDistance(const FVector3f& Position, FVector3f& OutNormal) const
	{
		switch (Shape)
		{
		case EBoidObstacleShape::Sphere:
		default:
		{
			const FVector3f Offset = Position - Center;
			const float Distance = Offset.Size();
			OutNormal = Distance > UE_KINDA_SMALL_NUMBER ? Offset / Distance : FVector3f::UpVector;
			return Distance - Radius;
		}
		}
	}

	/** Radius of a sphere bounding this obstacle, used to size the grid query. */
	float BoundingRadius() const
	{
		switch (Shape)
		{
		case EBoidObstacleShape::Sphere:
		default:
			return Radius;
		}
	}
};

/**
 * @brief Stable external reference to a registered obstacle (slot map handle).
 *
 * Gameplay holds this to update or remove a runtime obstacle (e.g. a wall that grows then
 * expires), since the dense obstacle array shifts on swap-remove.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidObstacleHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Slot = INDEX_NONE;

	UPROPERTY()
	uint32 Generation = 0;

	FORCEINLINE bool IsSet() const { return Slot != INDEX_NONE; }
};
