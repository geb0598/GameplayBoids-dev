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

	/** Spacing between boids: flockmates closer than this push each other away (soft, steering only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float SeparationRadius = 150.f;

	/** Boid's own size for obstacle collision: it is kept this far from any obstacle surface (hard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float CollisionRadius = 30.f;

	/** Distance from an obstacle surface at which soft steering avoidance starts ramping up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float AvoidDistance = 250.f;

	/** Strength of the soft steering away from obstacles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids|Behavior", meta = (ClampMin = "0"))
	float AvoidWeight = 3.f;

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
	Sphere,
	Box,
	Capsule
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

	/** Radius of the sphere, or of the capsule's tube. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids", meta = (ClampMin = "0", EditCondition = "Shape == EBoidObstacleShape::Sphere || Shape == EBoidObstacleShape::Capsule", EditConditionHides))
	float Radius = 300.f;

	/** Box half-extents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids", meta = (EditCondition = "Shape == EBoidObstacleShape::Box", EditConditionHides))
	FVector3f Extent = FVector3f(300.f);

	/** Capsule: half-length of the central segment (between the two hemisphere centers), along local Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids", meta = (ClampMin = "0", EditCondition = "Shape == EBoidObstacleShape::Capsule", EditConditionHides))
	float HalfHeight = 300.f;

	/** Orientation of the box or capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids", meta = (EditCondition = "Shape == EBoidObstacleShape::Box || Shape == EBoidObstacleShape::Capsule", EditConditionHides))
	FRotator Rotation = FRotator::ZeroRotator;

	/** Signed distance from Position to the surface (negative inside); OutNormal points outward. */
	float SignedDistance(const FVector3f& Position, FVector3f& OutNormal) const
	{
		switch (Shape)
		{
		case EBoidObstacleShape::Box:
		{
			// Work in the box's local frame, then in the positive octant (abs); restore the sign at the end.
			const FQuat Orientation = Rotation.Quaternion();
			const FVector Local = Orientation.UnrotateVector(FVector(Position) - FVector(Center));
			const FVector Q = Local.GetAbs() - FVector(Extent);

			const FVector OutsideVec(FMath::Max(Q.X, 0.0), FMath::Max(Q.Y, 0.0), FMath::Max(Q.Z, 0.0));
			const double Outside = OutsideVec.Size();
			const double Inside = FMath::Min(FMath::Max3(Q.X, Q.Y, Q.Z), 0.0);

			FVector LocalNormal;
			if (Outside > UE_KINDA_SMALL_NUMBER)
			{
				LocalNormal = OutsideVec; // nearest surface point -> Local (face or corner)
			}
			else if (Q.X >= Q.Y && Q.X >= Q.Z)
			{
				LocalNormal = FVector(1.0, 0.0, 0.0); // inside: push out the nearest face
			}
			else if (Q.Y >= Q.Z)
			{
				LocalNormal = FVector(0.0, 1.0, 0.0);
			}
			else
			{
				LocalNormal = FVector(0.0, 0.0, 1.0);
			}

			LocalNormal *= FVector(FMath::Sign(Local.X), FMath::Sign(Local.Y), FMath::Sign(Local.Z));
			OutNormal = FVector3f(Orientation.RotateVector(LocalNormal.GetSafeNormal()));
			return static_cast<float>(Outside + Inside);
		}
		case EBoidObstacleShape::Capsule:
		{
			// Distance to the central segment (local Z, ±HalfHeight) minus the tube radius.
			const FQuat Orientation = Rotation.Quaternion();
			const FVector Local = Orientation.UnrotateVector(FVector(Position) - FVector(Center));

			const double ClampedZ = FMath::Clamp(Local.Z, -static_cast<double>(HalfHeight), static_cast<double>(HalfHeight));
			const FVector ToSurface = Local - FVector(0.0, 0.0, ClampedZ);
			const double Distance = ToSurface.Size();

			OutNormal = FVector3f(Orientation.RotateVector(Distance > UE_KINDA_SMALL_NUMBER ? ToSurface / Distance : FVector::UpVector));
			return static_cast<float>(Distance - Radius);
		}
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
		case EBoidObstacleShape::Box:
			return Extent.Size();
		case EBoidObstacleShape::Capsule:
			return HalfHeight + Radius;
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

/**
 * @brief Shared convex shape: face planes in local space, registered once per mesh and pointed at
 * by many instances.
 *
 * A point is inside when it is behind every face plane. Gameplay builds this from a mesh's convex
 * collision; the plugin only stores and evaluates it.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidConvexGeometry
{
	GENERATED_BODY()

	/** Face planes (local space): xyz = outward unit normal, w = offset, so a face's signed distance is dot(N, P) - W. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids")
	TArray<FVector4f> Planes;

	/** Distance from the local origin to the farthest point, used to size the grid query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids", meta = (ClampMin = "0"))
	float BoundingRadius = 0.f;
};

/** Stable handle to a registered FBoidConvexGeometry (shape), shared across instances. */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidConvexGeometryHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Slot = INDEX_NONE;

	UPROPERTY()
	uint32 Generation = 0;

	FORCEINLINE bool IsSet() const { return Slot != INDEX_NONE; }
};

/** A placed convex obstacle: which geometry to use, plus its world transform. */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidConvexInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids")
	FBoidConvexGeometryHandle Geometry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids")
	FVector3f Center = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayBoids")
	FRotator Rotation = FRotator::ZeroRotator;
};

/** Stable handle to a placed convex obstacle instance. */
USTRUCT(BlueprintType)
struct GAMEPLAYBOIDS_API FBoidConvexHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Slot = INDEX_NONE;

	UPROPERTY()
	uint32 Generation = 0;

	FORCEINLINE bool IsSet() const { return Slot != INDEX_NONE; }
};
