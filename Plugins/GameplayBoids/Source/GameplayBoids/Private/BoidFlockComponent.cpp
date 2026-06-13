#include "BoidFlockComponent.h"

#include "BoidSpeciesAsset.h"
#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
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

UBoidFlockComponent::UBoidFlockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBoidFlockComponent::BeginPlay()
{
	Super::BeginPlay();

	CreateSpeciesRenderers();
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
	IndexToSlot.Reserve(Total);
	SlotToIndex.Reserve(Total);
	SlotGeneration.Reserve(Total);

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
}
