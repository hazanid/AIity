#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AIityGameMode.generated.h"

class UStaticMesh;
class UMaterialInterface;
namespace AIity
{
struct FWorldState;
}

UCLASS()
class AIITY_API AAIityGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAIityGameMode();
	virtual void BeginPlay() override;
	const FString& GetStartupFailure() const { return StartupFailure; }
	static bool ReconcileFounderActors(UWorld& World, const AIity::FWorldState& State);

private:
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	FString StartupFailure;

	bool BuildRiverValley();
	bool SpawnFounders();
	bool SpawnShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
		const FLinearColor& Color, bool bCollision = true);
	void FailStartup(const FString& Message);
};
