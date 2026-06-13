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

	/** Per-rule steering force cap before weighting; governs turn agility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Movement", meta = (ClampMin = "0"))
	float MaxSteerForce = 600.f;

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

	// --- Bounds ---

	/** Strength of the turn-back force at the simulation bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Bounds", meta = (ClampMin = "0"))
	float BoundsWeight = 4.f;

	/** Distance inside the edge where the turn-back force begins ramping up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Bounds", meta = (ClampMin = "1"))
	float BoundsMargin = 300.f;
};
