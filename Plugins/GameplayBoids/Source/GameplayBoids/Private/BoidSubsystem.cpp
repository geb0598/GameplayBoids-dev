#include "BoidSubsystem.h"

#include "Async/ParallelFor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarBoidDrawDebug(
	TEXT("GameplayBoids.DrawDebug"),
	0,
	TEXT("Draw boid debug points and the grid occupancy heatmap. 0 = off, 1 = on."),
	ECVF_Cheat);

void UBoidSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SpawnInitialBoids();
}

void UBoidSubsystem::Tick(float DeltaTime)
{
	if (Positions.Num() == 0)
	{
		return;
	}

	DeltaTime = FMath::Min(DeltaTime, 1.f / 20.f);

	Grid.Build(Positions, SimCenter);
	
	Integrate(DeltaTime);

	if (CVarBoidDrawDebug.GetValueOnGameThread() > 0)
	{
		DrawDebug();
	}
}

FBoidHandle UBoidSubsystem::SpawnBoid(const FVector3f& Position, const FVector3f& Velocity, uint8 SpeciesId)
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

void UBoidSubsystem::DespawnBoid(int32 Index)
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

int32 UBoidSubsystem::ResolveHandle(const FBoidHandle& Handle) const
{
	if (Handle.IsSet() && SlotGeneration.IsValidIndex(Handle.Slot) && SlotGeneration[Handle.Slot] == Handle.Generation)
	{
		return SlotToIndex[Handle.Slot];
	}
	return INDEX_NONE;
}

FBoidHandle UBoidSubsystem::MakeHandle(int32 Index) const
{
	FBoidHandle Handle;
	if (IndexToSlot.IsValidIndex(Index))
	{
		Handle.Slot = IndexToSlot[Index];
		Handle.Generation = SlotGeneration[Handle.Slot];
	}
	return Handle;
}

void UBoidSubsystem::SpawnInitialBoids()
{
	Positions.Reserve(MaxBoids);
	Velocities.Reserve(MaxBoids);
	SpeciesIds.Reserve(MaxBoids);
	IndexToSlot.Reserve(MaxBoids);
	SlotToIndex.Reserve(MaxBoids);
	SlotGeneration.Reserve(MaxBoids);

	for (int32 i = 0; i < MaxBoids; ++i)
	{
		const FVector3f Position = SimCenter + FVector3f(
			FMath::FRandRange(-SpawnExtent.X, SpawnExtent.X),
			FMath::FRandRange(-SpawnExtent.Y, SpawnExtent.Y),
			FMath::FRandRange(-SpawnExtent.Z, SpawnExtent.Z));

		FVector3f Direction = FVector3f(FMath::VRand());
		Direction.Z *= 0.2f;
		const FVector3f Velocity = Direction.GetSafeNormal() * FMath::FRandRange(Params.MinSpeed, Params.MaxSpeed);

		SpawnBoid(Position, Velocity);
	}
}

void UBoidSubsystem::Integrate(float DeltaTime)
{
	ParallelFor(Positions.Num(), [this, DeltaTime](int32 Index)
	{
		FVector3f Velocity = Velocities[Index];

		const float Speed = Velocity.Size();
		if (Speed > UE_KINDA_SMALL_NUMBER)
		{
			Velocity *= FMath::Clamp(Speed, Params.MinSpeed, Params.MaxSpeed) / Speed;
		}

		Velocities[Index] = Velocity;
		Positions[Index] += Velocity * DeltaTime;
	});
}

void UBoidSubsystem::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FVector3f& Position : Positions)
	{
		DrawDebugPoint(World, FVector(Position), 4.f, FColor::Cyan, false, -1.f);
	}

	constexpr float HeatmapMax = 16.f;
	Grid.ForEachCell([World](const FBox& Bounds, int32 Count)
	{
		if (Count == 0)
		{
			return;
		}

		const float Heat = FMath::Clamp(Count / HeatmapMax, 0.f, 1.f);
		const FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Heat).ToFColor(true);
		DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), Color, false, -1.f, 0, 1.f);
	});
}
