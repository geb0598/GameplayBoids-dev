#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoidTypes.h"
#include "BoidSpeciesAsset.generated.h"

class UStaticMesh;
class UMaterialInterface;

/**
 * @brief Defines one boid species: how it looks (mesh/material) and how it behaves (sim params).
 *
 * A flock references several of these; each gets its own instanced-mesh renderer, and each
 * boid's SpeciesId selects which species' mesh and params it uses.
 */
UCLASS()
class GAMEPLAYBOIDS_API UBoidSpeciesAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Mesh rendered for each boid of this species (one instanced renderer per species). */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	TObjectPtr<UStaticMesh> Mesh;

	/** Optional material override; the mesh's default material is used when unset. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	TObjectPtr<UMaterialInterface> Material;

	/** Uniform render scale of the mesh. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids", meta = (ClampMin = "0"))
	float MeshScale = 1.f;

	/** Steering and locomotion parameters for this species. */
	UPROPERTY(EditAnywhere, Category = "GameplayBoids")
	FBoidSimParams Params;
};
