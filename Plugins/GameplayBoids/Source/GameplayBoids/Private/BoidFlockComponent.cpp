#include "BoidFlockComponent.h"

#include "Async/ParallelFor.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarBoidDrawDebug(
	TEXT("GameplayBoids.DrawDebug"),
	0,
	TEXT("Draw boid debug points and the grid occupancy heatmap. 0 = off, 1 = on."),
	ECVF_Cheat);

UBoidFlockComponent::UBoidFlockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBoidFlockComponent::BeginPlay()
{
	Super::BeginPlay();

	SpawnInitialBoids();
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

	int32 Slot;
	if (FreeSlots.Num() > 0)
	{
		Slot = FreeSlots.Pop(EAllowShrinking::No);
		SlotToIndex[Slot] = Index;
	}
	else
	{
		Slot = SlotToIndex.Add(Index);
		SlotGeneration.Add(0);
	}
	IndexToSlot.Add(Slot);

	FBoidHandle Handle;
	Handle.Slot = Slot;
	Handle.Generation = SlotGeneration[Slot];
	return Handle;
}

void UBoidFlockComponent::DespawnBoid(int32 Index)
{
	if (!Positions.IsValidIndex(Index))
	{
		return;
	}

	const int32 Slot = IndexToSlot[Index];
	++SlotGeneration[Slot];
	SlotToIndex[Slot] = INDEX_NONE;
	FreeSlots.Add(Slot);

	const int32 Last = Positions.Num() - 1;
	if (Index != Last)
	{
		Positions[Index] = Positions[Last];
		Velocities[Index] = Velocities[Last];
		SpeciesIds[Index] = SpeciesIds[Last];

		const int32 MovedSlot = IndexToSlot[Last];
		IndexToSlot[Index] = MovedSlot;
		SlotToIndex[MovedSlot] = Index;
	}

	Positions.Pop(EAllowShrinking::No);
	Velocities.Pop(EAllowShrinking::No);
	SpeciesIds.Pop(EAllowShrinking::No);
	IndexToSlot.Pop(EAllowShrinking::No);
}

int32 UBoidFlockComponent::ResolveHandle(const FBoidHandle& Handle) const
{
	if (Handle.IsSet() && SlotGeneration.IsValidIndex(Handle.Slot) && SlotGeneration[Handle.Slot] == Handle.Generation)
	{
		return SlotToIndex[Handle.Slot];
	}
	return INDEX_NONE;
}

FBoidHandle UBoidFlockComponent::MakeHandle(int32 Index) const
{
	FBoidHandle Handle;
	if (IndexToSlot.IsValidIndex(Index))
	{
		Handle.Slot = IndexToSlot[Index];
		Handle.Generation = SlotGeneration[Handle.Slot];
	}
	return Handle;
}

void UBoidFlockComponent::SpawnInitialBoids()
{
	Positions.Reserve(MaxBoids);
	Velocities.Reserve(MaxBoids);
	SpeciesIds.Reserve(MaxBoids);
	IndexToSlot.Reserve(MaxBoids);
	SlotToIndex.Reserve(MaxBoids);
	SlotGeneration.Reserve(MaxBoids);

	const FVector3f Center = FVector3f(GetComponentLocation());

	for (int32 i = 0; i < MaxBoids; ++i)
	{
		const FVector3f Position = Center + FVector3f(
			FMath::FRandRange(-SpawnExtent.X, SpawnExtent.X),
			FMath::FRandRange(-SpawnExtent.Y, SpawnExtent.Y),
			FMath::FRandRange(-SpawnExtent.Z, SpawnExtent.Z));

		FVector3f Direction = FVector3f(FMath::VRand());
		Direction.Z *= 0.2f;
		const FVector3f Velocity = Direction.GetSafeNormal() * FMath::FRandRange(Params.MinSpeed, Params.MaxSpeed);

		SpawnBoid(Position, Velocity);
	}
}

FVector3f UBoidFlockComponent::SteerTowards(const FVector3f& Direction, const FVector3f& Velocity) const
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
	const FVector3f RelativePos = Positions[Index] - FVector3f(GetComponentLocation());

	auto AxisPush = [this](float Pos, float Extent)
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

	return SteerTowards(Push, Velocities[Index]) * FMath::Min(Push.Size(), 1.f);
}

FVector3f UBoidFlockComponent::ComputeSteeringForce(int32 Index) const
{
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
		Force += SteerTowards(SeparationSum, Velocity) * Params.SeparationWeight;
		Force += SteerTowards(VelocitySum, Velocity) * Params.AlignmentWeight;
		Force += SteerTowards(PositionSum / static_cast<float>(NumPerceivedBoids) - Position, Velocity) * Params.CohesionWeight;
	}

	return Force;
}

void UBoidFlockComponent::Integrate(float DeltaTime)
{
	ParallelFor(Positions.Num(), [this, DeltaTime](int32 Index)
	{
		const FVector3f Acceleration = Forces[Index] / Params.Mass;
		FVector3f Velocity = Velocities[Index] + Acceleration * DeltaTime;

		const float Speed = Velocity.Size();
		if (Speed > UE_KINDA_SMALL_NUMBER)
		{
			Velocity *= FMath::Clamp(Speed, Params.MinSpeed, Params.MaxSpeed) / Speed;
		}

		Velocities[Index] = Velocity;
		Positions[Index] += Velocity * DeltaTime;
	});
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

		const FVector InnerExtent = FVector(SpawnExtent) - FVector(Params.BoundsMargin);
		if (InnerExtent.GetMin() > 0.f)
		{
			DrawDebugBox(World, Center, InnerExtent, FColor::Yellow, false, -1.f, 0, 1.f);
		}
	}
}
