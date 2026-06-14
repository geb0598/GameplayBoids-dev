#include "BoidFlockComponent.h"

#include "BoidSpeciesAsset.h"
#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

static TAutoConsoleVariable<int32> CVarBoidDrawDebug(
	TEXT("GameplayBoids.DrawDebug"),
	0,
	TEXT("Draw boid debug points and the grid occupancy heatmap. 0 = off, 1 = on."),
	ECVF_Cheat);

// Test helper: explode in front of the camera, hitting every flock in the world.
// Args: [impulse=2000] [radius=800]. Negative impulse implodes.
static void BoidExplodeCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	const float Impulse = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 2000.f;
	const float Radius = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 800.f;

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Center = ViewLocation + ViewRotation.Vector() * 1500.f;

	for (TObjectIterator<UBoidFlockComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			It->AddRadialImpulse(Center, Radius, Impulse);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBoidExplodeCommand(
	TEXT("GameplayBoids.Explode"),
	TEXT("Explode (radial impulse) 1500u in front of the camera. Args: [impulse=2000] [radius=800]. Negative implodes."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BoidExplodeCommand));

// Test helper: add a sphere obstacle in front of the camera to every flock.
static void BoidAddObstacleCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FBoidObstacle Obstacle;
	Obstacle.Shape = EBoidObstacleShape::Sphere;
	Obstacle.Center = FVector3f(ViewLocation + ViewRotation.Vector() * 1500.f);
	Obstacle.Radius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 500.f;

	for (TObjectIterator<UBoidFlockComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			It->AddObstacle(Obstacle);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBoidAddObstacleCommand(
	TEXT("GameplayBoids.AddObstacle"),
	TEXT("Add a sphere obstacle 1500u in front of the camera. Args: [radius=500]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BoidAddObstacleCommand));

// Test helper: add a thin box wall facing the camera to every flock.
static void BoidAddBoxCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const float HalfSize = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 500.f;

	FBoidObstacle Obstacle;
	Obstacle.Shape = EBoidObstacleShape::Box;
	Obstacle.Center = FVector3f(ViewLocation + ViewRotation.Vector() * 1500.f);
	Obstacle.Extent = FVector3f(50.f, HalfSize, HalfSize);   // thin along local X (facing the camera)
	Obstacle.Rotation = FRotator(0.f, ViewRotation.Yaw, 0.f);

	for (TObjectIterator<UBoidFlockComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			It->AddObstacle(Obstacle);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBoidAddBoxCommand(
	TEXT("GameplayBoids.AddBox"),
	TEXT("Add a thin box wall facing the camera. Args: [halfSize=500]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BoidAddBoxCommand));

// Test helper: add a vertical capsule pillar in front of the camera to every flock.
static void BoidAddCapsuleCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FBoidObstacle Obstacle;
	Obstacle.Shape = EBoidObstacleShape::Capsule;
	Obstacle.Center = FVector3f(ViewLocation + ViewRotation.Vector() * 1500.f);
	Obstacle.Radius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 200.f;
	Obstacle.HalfHeight = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 400.f;
	Obstacle.Rotation = FRotator::ZeroRotator;   // vertical pillar

	for (TObjectIterator<UBoidFlockComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			It->AddObstacle(Obstacle);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBoidAddCapsuleCommand(
	TEXT("GameplayBoids.AddCapsule"),
	TEXT("Add a vertical capsule pillar in front of the camera. Args: [radius=200] [halfHeight=400]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BoidAddCapsuleCommand));

// Test helper: register a box-shaped convex hull and place an instance in front of the camera.
static void BoidAddConvexCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const float HalfSize = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 400.f;

	// A box-shaped convex hull (6 axis-aligned face planes) standing in for a real mesh's collision.
	FBoidConvexGeometry Geometry;
	Geometry.Planes = {
		FVector4f(1.f, 0.f, 0.f, HalfSize), FVector4f(-1.f, 0.f, 0.f, HalfSize),
		FVector4f(0.f, 1.f, 0.f, HalfSize), FVector4f(0.f, -1.f, 0.f, HalfSize),
		FVector4f(0.f, 0.f, 1.f, HalfSize), FVector4f(0.f, 0.f, -1.f, HalfSize)
	};
	Geometry.BoundingRadius = HalfSize * FMath::Sqrt(3.f);

	const FVector Center = ViewLocation + ViewRotation.Vector() * 1500.f;
	const FRotator Rotation(0.f, ViewRotation.Yaw, 0.f);

	for (TObjectIterator<UBoidFlockComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			const FBoidConvexGeometryHandle GeometryHandle = It->RegisterConvexGeometry(Geometry);
			It->AddConvexObstacle(GeometryHandle, Center, Rotation);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBoidAddConvexCommand(
	TEXT("GameplayBoids.AddConvex"),
	TEXT("Add a box-shaped convex obstacle facing the camera (for testing). Args: [halfSize=400]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BoidAddConvexCommand));

static void BoidClearObstaclesCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	for (TObjectIterator<UBoidFlockComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			It->ClearObstacles();
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBoidClearObstaclesCommand(
	TEXT("GameplayBoids.ClearObstacles"),
	TEXT("Remove all obstacles from every flock."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BoidClearObstaclesCommand));

UBoidFlockComponent::UBoidFlockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBoidFlockComponent::BeginPlay()
{
	Super::BeginPlay();

	CreateSpeciesRenderers();
	SpawnInitialBoids();

	for (const FBoidObstacle& Obstacle : InitialObstacles)
	{
		AddObstacle(Obstacle);
	}
}

void UBoidFlockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Positions.Num() == 0)
	{
		return;
	}

	DeltaTime = FMath::Min(DeltaTime, 1.f / 20.f);

	Grid.Build(Positions, FVector3f(GetComponentLocation()));

	Forces.SetNumUninitialized(Positions.Num());
	ParallelFor(Positions.Num(), [this](int32 Index)
	{
		Forces[Index] = ComputeSteeringForce(Index);
	});

	Integrate(DeltaTime);

	ResolveObstacles();
	ResolveConvexObstacles();

	UpdateRenderInstances();

	if (CVarBoidDrawDebug.GetValueOnGameThread() > 0)
	{
		DrawDebug();
	}
}

FBoidHandle UBoidFlockComponent::SpawnBoid(const FVector3f& Position, const FVector3f& Velocity, uint8 SpeciesId)
{
	const int32 Index = Positions.Add(Position);
	Velocities.Add(Velocity);
	SpeciesIds.Add(SpeciesId);

	return BoidSlots.Add(Index);
}

void UBoidFlockComponent::DespawnBoid(int32 Index)
{
	if (!Positions.IsValidIndex(Index))
	{
		return;
	}

	const int32 Last = Positions.Num() - 1;
	BoidSlots.RemoveAt(Index, Last);

	if (Index != Last)
	{
		Positions[Index] = Positions[Last];
		Velocities[Index] = Velocities[Last];
		SpeciesIds[Index] = SpeciesIds[Last];
	}

	Positions.Pop(EAllowShrinking::No);
	Velocities.Pop(EAllowShrinking::No);
	SpeciesIds.Pop(EAllowShrinking::No);
}

int32 UBoidFlockComponent::ResolveHandle(const FBoidHandle& Handle) const
{
	return BoidSlots.Resolve(Handle);
}

FBoidHandle UBoidFlockComponent::MakeHandle(int32 Index) const
{
	return BoidSlots.MakeHandle(Index);
}

void UBoidFlockComponent::AddRadialImpulse(const FVector& Center, float Radius, float Impulse)
{
	if (!Grid.IsBuilt() || Radius <= 0.f)
	{
		return;
	}

	const FVector3f Center3f(Center);

	Grid.ForEachBoidInCellRange(Center3f, Radius, [&](int32 Index)
	{
		const FVector3f Offset = Positions[Index] - Center3f;
		const float Dist = Offset.Size();
		if (Dist > Radius)
		{
			return;
		}

		const FVector3f Direction = Dist > UE_KINDA_SMALL_NUMBER ? Offset / Dist : FVector3f(FMath::VRand());
		const float Falloff = 1.f - Dist / Radius;
		const float Mass = FMath::Max(ParamsFor(Index).Mass, UE_KINDA_SMALL_NUMBER);

		Velocities[Index] += Direction * (Impulse * Falloff / Mass);
	});

#if ENABLE_DRAW_DEBUG
	if (CVarBoidDrawDebug.GetValueOnGameThread() > 0)
	{
		if (UWorld* World = GetWorld())
		{
			const FColor Color = Impulse >= 0.f ? FColor::Red : FColor::Cyan;
			DrawDebugSphere(World, Center, Radius, 16, Color, false, 0.5f);
		}
	}
#endif
}

FBoidObstacleHandle UBoidFlockComponent::AddObstacle(const FBoidObstacle& Obstacle)
{
	const int32 Index = Obstacles.Add(Obstacle);
	return ObstacleSlots.Add(Index);
}

void UBoidFlockComponent::UpdateObstacle(const FBoidObstacleHandle& Handle, const FBoidObstacle& Obstacle)
{
	const int32 Index = ObstacleSlots.Resolve(Handle);
	if (Index != INDEX_NONE)
	{
		Obstacles[Index] = Obstacle;
	}
}

void UBoidFlockComponent::RemoveObstacle(const FBoidObstacleHandle& Handle)
{
	const int32 Index = ObstacleSlots.Resolve(Handle);
	if (Index == INDEX_NONE)
	{
		return;
	}

	const int32 Last = Obstacles.Num() - 1;
	ObstacleSlots.RemoveAt(Index, Last);

	if (Index != Last)
	{
		Obstacles[Index] = Obstacles[Last];
	}

	Obstacles.Pop(EAllowShrinking::No);
}

void UBoidFlockComponent::ClearObstacles()
{
	Obstacles.Reset();
	ObstacleSlots.Reset();

	ConvexObstacles.Reset();
	ConvexSlots.Reset();
	ConvexGeometries.Reset();
	ConvexGeometrySlots.Reset();
}

FBoidConvexGeometryHandle UBoidFlockComponent::RegisterConvexGeometry(const FBoidConvexGeometry& Geometry)
{
	const int32 Index = ConvexGeometries.Add(Geometry);
	return ConvexGeometrySlots.Add(Index);
}

void UBoidFlockComponent::UnregisterConvexGeometry(const FBoidConvexGeometryHandle& Geometry)
{
	const int32 Index = ConvexGeometrySlots.Resolve(Geometry);
	if (Index == INDEX_NONE)
	{
		return;
	}

	const int32 Last = ConvexGeometries.Num() - 1;
	ConvexGeometrySlots.RemoveAt(Index, Last);

	if (Index != Last)
	{
		ConvexGeometries[Index] = ConvexGeometries[Last];
	}

	ConvexGeometries.Pop(EAllowShrinking::No);
}

FBoidConvexHandle UBoidFlockComponent::AddConvexObstacle(const FBoidConvexGeometryHandle& Geometry, const FVector& Center, const FRotator& Rotation)
{
	FBoidConvexInstance Instance;
	Instance.Geometry = Geometry;
	Instance.Center = FVector3f(Center);
	Instance.Rotation = Rotation;

	const int32 Index = ConvexObstacles.Add(Instance);
	return ConvexSlots.Add(Index);
}

void UBoidFlockComponent::UpdateConvexObstacle(const FBoidConvexHandle& Handle, const FVector& Center, const FRotator& Rotation)
{
	const int32 Index = ConvexSlots.Resolve(Handle);
	if (Index != INDEX_NONE)
	{
		ConvexObstacles[Index].Center = FVector3f(Center);
		ConvexObstacles[Index].Rotation = Rotation;
	}
}

void UBoidFlockComponent::RemoveConvexObstacle(const FBoidConvexHandle& Handle)
{
	const int32 Index = ConvexSlots.Resolve(Handle);
	if (Index == INDEX_NONE)
	{
		return;
	}

	const int32 Last = ConvexObstacles.Num() - 1;
	ConvexSlots.RemoveAt(Index, Last);

	if (Index != Last)
	{
		ConvexObstacles[Index] = ConvexObstacles[Last];
	}

	ConvexObstacles.Pop(EAllowShrinking::No);
}

static float ConvexLocalSignedDistance(const FBoidConvexGeometry& Geometry, const FVector& LocalPoint, FVector& OutLocalNormal)
{
	OutLocalNormal = FVector::UpVector;
	if (Geometry.Planes.Num() == 0)
	{
		return TNumericLimits<float>::Max();
	}

	double MaxSD = -TNumericLimits<double>::Max();
	for (const FVector4f& Plane : Geometry.Planes)
	{
		const FVector Normal(Plane.X, Plane.Y, Plane.Z);
		const double SD = FVector::DotProduct(Normal, LocalPoint) - Plane.W;
		if (SD > MaxSD)
		{
			MaxSD = SD;
			OutLocalNormal = Normal;
		}
	}
	return static_cast<float>(MaxSD);
}

void UBoidFlockComponent::ResolveConvexObstacles()
{
	if (!Grid.IsBuilt())
	{
		return;
	}

	// The grid was built from pre-Integrate positions, so query a bit wider than the obstacle
	// to still catch boids that moved into it this frame.
	constexpr float QueryMargin = 200.f;

	for (const FBoidConvexInstance& Instance : ConvexObstacles)
	{
		const int32 GeoIndex = ConvexGeometrySlots.Resolve(Instance.Geometry);
		if (GeoIndex == INDEX_NONE)
		{
			continue;
		}

		const FBoidConvexGeometry& Geometry = ConvexGeometries[GeoIndex];
		const FQuat Orientation = Instance.Rotation.Quaternion();
		const FVector Center(Instance.Center);

		Grid.ForEachBoidInCellRange(Instance.Center, Geometry.BoundingRadius + QueryMargin, [&](int32 Index)
		{
			const float BoidRadius = ParamsFor(Index).CollisionRadius;
			const FVector LocalPoint = Orientation.UnrotateVector(FVector(Positions[Index]) - Center);

			FVector LocalNormal;
			const float Distance = ConvexLocalSignedDistance(Geometry, LocalPoint, LocalNormal);
			if (Distance >= BoidRadius)
			{
				return;
			}

			const FVector3f Normal = FVector3f(Orientation.RotateVector(LocalNormal.GetSafeNormal()));
			Positions[Index] += Normal * (BoidRadius - Distance);

			const float NormalVelocity = Velocities[Index] | Normal;
			if (NormalVelocity < 0.f)
			{
				Velocities[Index] -= Normal * NormalVelocity;
			}
		});
	}
}

void UBoidFlockComponent::ResolveObstacles()
{
	if (!Grid.IsBuilt())
	{
		return;
	}

	// The grid was built from pre-Integrate positions, so query a bit wider than the obstacle
	// to still catch boids that moved into it this frame.
	constexpr float QueryMargin = 200.f;

	for (const FBoidObstacle& Obstacle : Obstacles)
	{
		Grid.ForEachBoidInCellRange(Obstacle.Center, Obstacle.BoundingRadius() + QueryMargin, [&](int32 Index)
		{
			const float BoidRadius = ParamsFor(Index).CollisionRadius;

			FVector3f Normal;
			const float Distance = Obstacle.SignedDistance(Positions[Index], Normal);
			if (Distance >= BoidRadius)
			{
				return;
			}

			Positions[Index] += Normal * (BoidRadius - Distance);

			const float NormalVelocity = Velocities[Index] | Normal;
			if (NormalVelocity < 0.f)
			{
				Velocities[Index] -= Normal * NormalVelocity;
			}
		});
	}
}

void UBoidFlockComponent::CreateSpeciesRenderers()
{
	SpeciesRenderers.Reserve(Species.Num());

	for (const FBoidSpeciesEntry& Entry : Species)
	{
		UInstancedStaticMeshComponent* Renderer = nullptr;

		if (Entry.Asset && Entry.Asset->Mesh)
		{
			Renderer = NewObject<UInstancedStaticMeshComponent>(GetOwner());
			Renderer->SetupAttachment(this);
			Renderer->SetStaticMesh(Entry.Asset->Mesh);
			if (Entry.Asset->Material)
			{
				Renderer->SetMaterial(0, Entry.Asset->Material);
			}
			Renderer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Renderer->SetCanEverAffectNavigation(false);
			Renderer->RegisterComponent();
		}

		SpeciesRenderers.Add(Renderer);
	}
}

void UBoidFlockComponent::SpawnInitialBoids()
{
	int32 Total = 0;
	for (const FBoidSpeciesEntry& Entry : Species)
	{
		if (Entry.Asset && Entry.Asset->Mesh)
		{
			Total += Entry.Count;
		}
	}

	Positions.Reserve(Total);
	Velocities.Reserve(Total);
	SpeciesIds.Reserve(Total);
	BoidSlots.Reserve(Total);

	const FVector3f Center = FVector3f(GetComponentLocation());

	for (int32 s = 0; s < Species.Num(); ++s)
	{
		const FBoidSpeciesEntry& Entry = Species[s];
		if (!Entry.Asset || !Entry.Asset->Mesh)
		{
			continue;
		}

		const FBoidSimParams& Params = Entry.Asset->Params;

		for (int32 i = 0; i < Entry.Count; ++i)
		{
			const FVector3f Position = Center + FVector3f(
				FMath::FRandRange(-SpawnExtent.X, SpawnExtent.X),
				FMath::FRandRange(-SpawnExtent.Y, SpawnExtent.Y),
				FMath::FRandRange(-SpawnExtent.Z, SpawnExtent.Z));

			FVector3f Direction = FVector3f(FMath::VRand());
			Direction.Z *= 0.2f;
			const FVector3f Velocity = Direction.GetSafeNormal() * FMath::FRandRange(Params.MinSpeed, Params.MaxSpeed);

			SpawnBoid(Position, Velocity, static_cast<uint8>(s));
		}
	}
}

const FBoidSimParams& UBoidFlockComponent::ParamsFor(int32 Index) const
{
	return Species[SpeciesIds[Index]].Asset->Params;
}

FVector3f UBoidFlockComponent::SteerTowards(const FVector3f& Direction, const FVector3f& Velocity, const FBoidSimParams& Params) const
{
	if (Direction.IsNearlyZero())
	{
		return FVector3f::ZeroVector;
	}

	const FVector3f DesiredVelocity = Direction.GetSafeNormal() * Params.MaxSpeed;
	return (DesiredVelocity - Velocity).GetClampedToMaxSize(Params.MaxSteerForce);
}

FVector3f UBoidFlockComponent::ComputeBoundsForce(int32 Index) const
{
	const FBoidSimParams& Params = ParamsFor(Index);

	const FVector3f RelativePos = Positions[Index] - FVector3f(GetComponentLocation());

	auto AxisPush = [&Params](float Pos, float Extent)
	{
		const float SoftEdge = Extent - Params.BoundsMargin;
		if (FMath::Abs(Pos) <= SoftEdge)
		{
			return 0.f;
		}

		const float Penetration = (FMath::Abs(Pos) - SoftEdge) / Params.BoundsMargin;
		return -FMath::Sign(Pos) * FMath::Min(Penetration, 2.f);
	};

	const FVector3f Push(
		AxisPush(RelativePos.X, SpawnExtent.X),
		AxisPush(RelativePos.Y, SpawnExtent.Y),
		AxisPush(RelativePos.Z, SpawnExtent.Z));

	if (Push.IsNearlyZero())
	{
		return FVector3f::ZeroVector;
	}

	return SteerTowards(Push, Velocities[Index], Params) * FMath::Min(Push.Size(), 1.f);
}

FVector3f UBoidFlockComponent::ComputeSteeringForce(int32 Index) const
{
	const FBoidSimParams& Params = ParamsFor(Index);

	const FVector3f Position = Positions[Index];
	const FVector3f Velocity = Velocities[Index];
	const FVector3f Forward = Velocity.GetSafeNormal();

	const float PerceptionSq = FMath::Square(Params.PerceptionRadius);
	const float SeparationSq = FMath::Square(Params.SeparationRadius);
	const float CosHalfFOV = FMath::Cos(FMath::DegreesToRadians(Params.FieldOfViewDegrees * 0.5f));

	FVector3f PositionSum = FVector3f::ZeroVector;
	FVector3f VelocitySum = FVector3f::ZeroVector;
	FVector3f SeparationSum = FVector3f::ZeroVector;

	int32 NumPerceivedBoids = 0;

	Grid.ForEachBoidInCellRange(Position, Params.PerceptionRadius, [&](int32 Other)
	{
		if (Other == Index || NumPerceivedBoids >= Params.MaxNeighbors)
		{
			return;
		}

		const FVector3f Offset = Positions[Other] - Position;
		const float DistSq = Offset.SizeSquared();

		if (DistSq > PerceptionSq || DistSq < UE_KINDA_SMALL_NUMBER)
		{
			return;
		}

		if ((Offset | Forward) < CosHalfFOV * FMath::Sqrt(DistSq))
		{
			return;
		}

		++NumPerceivedBoids;
		VelocitySum += Velocities[Other];
		PositionSum += Positions[Other];

		if (DistSq < SeparationSq)
		{
			SeparationSum -= Offset / DistSq;
		}
	});

	FVector3f Force = ComputeBoundsForce(Index) * Params.BoundsWeight;

	if (NumPerceivedBoids > 0)
	{
		Force += SteerTowards(SeparationSum, Velocity, Params) * Params.SeparationWeight;
		Force += SteerTowards(VelocitySum, Velocity, Params) * Params.AlignmentWeight;
		Force += SteerTowards(PositionSum / static_cast<float>(NumPerceivedBoids) - Position, Velocity, Params) * Params.CohesionWeight;
	}

	return Force;
}

void UBoidFlockComponent::Integrate(float DeltaTime)
{
	const FVector3f Center = FVector3f(GetComponentLocation());
	const FVector3f Min = Center - SpawnExtent;
	const FVector3f Max = Center + SpawnExtent;

	ParallelFor(Positions.Num(), [this, DeltaTime, Min, Max](int32 Index)
	{
		const FBoidSimParams& Params = ParamsFor(Index);

		const FVector3f Acceleration = Forces[Index] / Params.Mass;
		FVector3f Velocity = Velocities[Index] + Acceleration * DeltaTime;

		const float Speed = Velocity.Size();
		if (Speed > UE_KINDA_SMALL_NUMBER)
		{
			float NewSpeed = FMath::Max(Speed, Params.MinSpeed);
			if (NewSpeed > Params.MaxSpeed)
			{
				NewSpeed = Params.MaxSpeed + (NewSpeed - Params.MaxSpeed) * FMath::Exp(-Params.OverSpeedDamping * DeltaTime);
			}
			Velocity *= NewSpeed / Speed;
		}

		FVector3f NewPosition = Positions[Index] + Velocity * DeltaTime;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (NewPosition[Axis] < Min[Axis])
			{
				NewPosition[Axis] = Min[Axis];
				Velocity[Axis] = FMath::Max(Velocity[Axis], 0.f);
			}
			else if (NewPosition[Axis] > Max[Axis])
			{
				NewPosition[Axis] = Max[Axis];
				Velocity[Axis] = FMath::Min(Velocity[Axis], 0.f);
			}
		}

		Velocities[Index] = Velocity;
		Positions[Index] = NewPosition;
	});
}

void UBoidFlockComponent::UpdateRenderInstances()
{
	SpeciesTransforms.SetNum(Species.Num());
	for (TArray<FTransform>& Bucket : SpeciesTransforms)
	{
		Bucket.Reset();
	}

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		const int32 SpeciesId = SpeciesIds[i];
		const float Scale = Species[SpeciesId].Asset->MeshScale;

		const FVector Velocity(Velocities[i]);
		const FQuat Rotation = Velocity.IsNearlyZero()
			? FQuat::Identity
			: FRotationMatrix::MakeFromX(Velocity).ToQuat();

		SpeciesTransforms[SpeciesId].Emplace(Rotation, FVector(Positions[i]), FVector(Scale));
	}

	for (int32 s = 0; s < SpeciesRenderers.Num(); ++s)
	{
		UInstancedStaticMeshComponent* Renderer = SpeciesRenderers[s];
		if (!Renderer)
		{
			continue;
		}

		const TArray<FTransform>& Transforms = SpeciesTransforms[s];
		if (Renderer->GetInstanceCount() != Transforms.Num())
		{
			Renderer->ClearInstances();
			Renderer->AddInstances(Transforms, false, true);
		}
		else
		{
			Renderer->BatchUpdateInstancesTransforms(0, Transforms, true, true,  true);
		}
	}
}

void UBoidFlockComponent::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bDrawBoids)
	{
		for (const FVector3f& Position : Positions)
		{
			DrawDebugPoint(World, FVector(Position), 4.f, FColor::Cyan, false, -1.f);
		}
	}

	if (bDrawGrid)
	{
		constexpr float HeatmapMax = 16.f;
		Grid.ForEachCell([World](const FBox& CellBounds, int32 Count)
		{
			if (Count == 0)
			{
				return;
			}

			const float Heat = FMath::Clamp(Count / HeatmapMax, 0.f, 1.f);
			const FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Heat).ToFColor(true);
			DrawDebugBox(World, CellBounds.GetCenter(), CellBounds.GetExtent(), Color, false, -1.f, 0, 1.f);
		});
	}

	const FVector Center = GetComponentLocation();

	if (bDrawSpawnBounds)
	{
		DrawDebugBox(World, Center, FVector(SpawnExtent), FColor::Green, false, -1.f, 0, 2.f);
	}

	if (bDrawBounds)
	{
		DrawDebugBox(World, Center, FVector(SpawnExtent), FColor::Orange, false, -1.f, 0, 2.f);
	}

	if (bDrawObstacles)
	{
		for (const FBoidObstacle& Obstacle : Obstacles)
		{
			if (Obstacle.Shape == EBoidObstacleShape::Box)
			{
				DrawDebugBox(World, FVector(Obstacle.Center), FVector(Obstacle.Extent), Obstacle.Rotation.Quaternion(), FColor::Yellow, false, -1.f);
			}
			else if (Obstacle.Shape == EBoidObstacleShape::Capsule)
			{
				DrawDebugCapsule(World, FVector(Obstacle.Center), Obstacle.HalfHeight + Obstacle.Radius, Obstacle.Radius, Obstacle.Rotation.Quaternion(), FColor::Yellow, false, -1.f);
			}
			else
			{
				DrawDebugSphere(World, FVector(Obstacle.Center), Obstacle.Radius, 16, FColor::Yellow, false, -1.f);
			}
		}

		// Convex hulls aren't drawn exactly (only planes are stored); show the bounding sphere.
		for (const FBoidConvexInstance& Instance : ConvexObstacles)
		{
			const int32 GeoIndex = ConvexGeometrySlots.Resolve(Instance.Geometry);
			const float Radius = GeoIndex != INDEX_NONE ? ConvexGeometries[GeoIndex].BoundingRadius : 50.f;
			DrawDebugSphere(World, FVector(Instance.Center), Radius, 12, FColor::Purple, false, -1.f);
		}
	}

	const bool bAnyPerBoid = bDrawBoidVelocity || bDrawBoidForce || bDrawBoidPerception || bDrawBoidSeparation || bDrawBoidCollision || bDrawBoidFOV;
	if (bAnyPerBoid)
	{
		const int32 SampleCount = FMath::Min(DebugSampleCount, Positions.Num());
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const FVector Position(Positions[Index]);
			const FBoidSimParams& Params = ParamsFor(Index);

			if (bDrawBoidVelocity)
			{
				DrawDebugDirectionalArrow(World, Position, Position + FVector(Velocities[Index]) * 0.25f, 30.f, FColor::White, false, -1.f);
			}

			if (bDrawBoidForce && Forces.IsValidIndex(Index))
			{
				DrawDebugDirectionalArrow(World, Position, Position + FVector(Forces[Index]) * 0.25f, 30.f, FColor::Magenta, false, -1.f);
			}

			if (bDrawBoidPerception)
			{
				DrawDebugSphere(World, Position, Params.PerceptionRadius, 12, FColor(64, 64, 255), false, -1.f);
			}

			if (bDrawBoidSeparation)
			{
				DrawDebugSphere(World, Position, Params.SeparationRadius, 10, FColor::Orange, false, -1.f);
			}

			if (bDrawBoidCollision)
			{
				DrawDebugSphere(World, Position, Params.CollisionRadius, 8, FColor(255, 64, 64), false, -1.f);
			}

			if (bDrawBoidFOV && !Velocities[Index].IsNearlyZero())
			{
				const FVector Direction(-Velocities[Index].GetSafeNormal());
				const float HalfAngle = FMath::DegreesToRadians(Params.FieldOfViewDegrees * 0.5f);
				DrawDebugCone(World, Position, Direction, Params.PerceptionRadius, HalfAngle, HalfAngle, 16, FColor::Cyan, false, -1.f);
			}
		}
	}

	if (GEngine)
	{
		auto Legend = [](int32 Key, bool bEnabled, const FColor& Color, const TCHAR* Label)
		{
			if (bEnabled)
			{
				GEngine->AddOnScreenDebugMessage(Key, 0.f, Color, Label);
			}
		};

		Legend(1710, bDrawBoidFOV,        FColor::Cyan,        TEXT("Boid FOV"));
		Legend(1709, bDrawBoidCollision,  FColor(255, 64, 64), TEXT("Boid collision"));
		Legend(1708, bDrawBoidSeparation, FColor::Orange,      TEXT("Boid separation"));
		Legend(1707, bDrawBoidPerception, FColor(64, 64, 255), TEXT("Boid perception"));
		Legend(1706, bDrawBoidForce,      FColor::Magenta,     TEXT("Boid force"));
		Legend(1705, bDrawBoidVelocity,   FColor::White,       TEXT("Boid velocity"));
		Legend(1704, bDrawObstacles,      FColor::Yellow,      TEXT("Obstacles"));
		Legend(1703, bDrawBounds,         FColor::Orange,      TEXT("Bounds"));
		Legend(1702, bDrawSpawnBounds,    FColor::Green,       TEXT("Spawn bounds"));
		Legend(1701, bDrawGrid,           FColor::White,       TEXT("Grid (occupancy heatmap)"));
		Legend(1700, bDrawBoids,          FColor::Cyan,        TEXT("Boids"));
	}
}
