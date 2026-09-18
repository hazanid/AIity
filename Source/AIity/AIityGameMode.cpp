#include "AIityGameMode.h"

#include "AIityFounderCharacter.h"
#include "AIityHUD.h"
#include "AIityObserverPawn.h"
#include "AIityPlayerController.h"
#include "AIityWorldSubsystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "EngineDefines.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Simulation/AIityWorldState.h"
#include "UObject/ConstructorHelpers.h"

#include <algorithm>

namespace
{
constexpr float FounderSpawnZ = 110.0f;
constexpr int64 FounderPlacementLimit = static_cast<int64>(UE_OLD_HALF_WORLD_MAX1);
// ponytail: fixed nearby search is capped at 25; widen it only from measured placement failures.
const FIntPoint FounderPlacementOffsets[] = {
	FIntPoint(0, 0),
	FIntPoint(100, 0), FIntPoint(-100, 0), FIntPoint(0, 100), FIntPoint(0, -100),
	FIntPoint(100, 100), FIntPoint(100, -100), FIntPoint(-100, 100), FIntPoint(-100, -100),
	FIntPoint(200, 0), FIntPoint(-200, 0), FIntPoint(0, 200), FIntPoint(0, -200),
	FIntPoint(200, 100), FIntPoint(200, -100), FIntPoint(-200, 100), FIntPoint(-200, -100),
	FIntPoint(100, 200), FIntPoint(100, -200), FIntPoint(-100, 200), FIntPoint(-100, -200),
	FIntPoint(200, 200), FIntPoint(200, -200), FIntPoint(-200, 200), FIntPoint(-200, -200)
};
}

AAIityGameMode::AAIityGameMode()
{
	DefaultPawnClass = AAIityObserverPawn::StaticClass();
	PlayerControllerClass = AAIityPlayerController::StaticClass();
	HUDClass = AAIityHUD::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
}

void AAIityGameMode::BeginPlay()
{
	Super::BeginPlay();
	const UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>();
	if (!Simulation || !Simulation->IsReady())
	{
		return;
	}
	if (!CubeMesh || !CylinderMesh || !SphereMesh)
	{
		FailStartup(TEXT("STARTUP FAILED: required procedural geometry assets were not loaded."));
		return;
	}
	if (!BuildRiverValley())
	{
		FailStartup(TEXT("STARTUP FAILED: required river-valley actors could not be spawned."));
		return;
	}
	if (!SpawnFounders())
	{
		FailStartup(TEXT("STARTUP FAILED: all ten required founders could not be spawned."));
	}
}

bool AAIityGameMode::SpawnShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
	const FLinearColor& Color, bool bCollision)
{
	if (!Mesh)
	{
		return false;
	}
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
	if (!Actor || !Actor->GetStaticMeshComponent())
	{
		return false;
	}
	Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
	Actor->GetStaticMeshComponent()->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Actor->SetActorScale3D(Scale);
	if (UMaterialInstanceDynamic* Material = Actor->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return true;
}

bool AAIityGameMode::BuildRiverValley()
{
	bool bSuccess = SpawnShape(CubeMesh.Get(), FVector(0, 0, -70), FVector(60, 60, 1),
		FLinearColor(0.18f, 0.29f, 0.12f));
	bSuccess &= SpawnShape(CubeMesh.Get(), FVector(0, -650, -10), FVector(60, 5, 0.35f),
		FLinearColor(0.04f, 0.28f, 0.42f), false);

	for (int32 Side : {-1, 1})
	{
		for (int32 Step = -2; Step <= 2; ++Step)
		{
			bSuccess &= SpawnShape(CubeMesh.Get(), FVector(Side * 2900.0f, Step * 1100.0f, 250.0f),
				FVector(4.0f, 12.0f, 6.0f + FMath::Abs(Step)), FLinearColor(0.23f, 0.20f, 0.15f));
		}
	}
	for (int32 Index = 0; Index < 18; ++Index)
	{
		const float X = -2400.0f + static_cast<float>((Index * 733) % 4800);
		const float Y = Index % 2 == 0 ? 1500.0f + (Index % 3) * 180.0f : -1800.0f - (Index % 3) * 130.0f;
		bSuccess &= SpawnShape(CylinderMesh.Get(), FVector(X, Y, 115.0f),
			FVector(0.28f, 0.28f, 2.3f), FLinearColor(0.20f, 0.10f, 0.04f));
		bSuccess &= SpawnShape(SphereMesh.Get(), FVector(X, Y, 310.0f),
			FVector(1.15f, 1.15f, 0.9f), FLinearColor(0.08f, 0.24f, 0.07f), false);
	}
	bSuccess &= SpawnShape(SphereMesh.Get(), FVector(-1200, 500, 65), FVector(2.2f, 2.2f, 0.7f),
		FLinearColor(0.38f, 0.16f, 0.08f), false);
	bSuccess &= SpawnShape(CylinderMesh.Get(), FVector(1200, -450, 20), FVector(1.8f, 1.8f, 0.25f),
		FLinearColor(0.07f, 0.42f, 0.58f), false);

	ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-45, -25, 0));
	if (Sun && Sun->GetLightComponent())
	{
		Sun->GetLightComponent()->SetIntensity(6.0f);
	}
	else
	{
		bSuccess = false;
	}
	bSuccess &= GetWorld()->SpawnActor<ASkyLight>() != nullptr;
	return bSuccess;
}

void AAIityGameMode::FailStartup(const FString& Message)
{
	StartupFailure = Message;
	UE_LOG(LogTemp, Error, TEXT("%s"), *StartupFailure);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, StartupFailure);
	}
	if (UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		Simulation->MarkStartupFailure(StartupFailure);
	}
}

bool AAIityGameMode::SpawnFounders()
{
	const UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>();
	if (!Simulation)
	{
		return false;
	}
	return ReconcileFounderActors(*GetWorld(), Simulation->GetCommittedState());
}

bool AAIityGameMode::ReconcileFounderActors(UWorld& World, const AIity::FWorldState& State)
{
	if (State.Founders.size() != 10)
	{
		return false;
	}

	std::vector<const AIity::FFounder*> Founders;
	Founders.reserve(State.Founders.size());
	for (const AIity::FFounder& Founder : State.Founders)
	{
		Founders.push_back(&Founder);
	}
	std::sort(Founders.begin(), Founders.end(),
		[](const AIity::FFounder* Left, const AIity::FFounder* Right)
		{
			return Left->Id < Right->Id;
		});
	for (size_t Index = 0; Index < Founders.size(); ++Index)
	{
		if (Founders[Index]->Id == 0 ||
			(Index > 0 && Founders[Index - 1]->Id == Founders[Index]->Id))
		{
			return false;
		}
		for (const FIntPoint& Offset : FounderPlacementOffsets)
		{
			const int64 PlacementX =
				static_cast<int64>(Founders[Index]->X) + static_cast<int64>(Offset.X);
			const int64 PlacementY =
				static_cast<int64>(Founders[Index]->Y) + static_cast<int64>(Offset.Y);
			if (PlacementX < -FounderPlacementLimit || PlacementX > FounderPlacementLimit ||
				PlacementY < -FounderPlacementLimit || PlacementY > FounderPlacementLimit)
			{
				return false;
			}
		}
	}

	TArray<AAIityFounderCharacter*> Existing;
	for (TActorIterator<AAIityFounderCharacter> It(&World); It; ++It)
	{
		It->ResetBridgeExecution();
		It->SetActorEnableCollision(false);
		Existing.Add(*It);
	}

	TArray<AAIityFounderCharacter*> Created;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	for (int32 FounderIndex = 0; FounderIndex < static_cast<int32>(Founders.size()); ++FounderIndex)
	{
		const AIity::FFounder& Founder = *Founders[FounderIndex];
		AAIityFounderCharacter* Character = nullptr;
		for (const FIntPoint& Offset : FounderPlacementOffsets)
		{
			const int64 PlacementX =
				static_cast<int64>(Founder.X) + static_cast<int64>(Offset.X);
			const int64 PlacementY =
				static_cast<int64>(Founder.Y) + static_cast<int64>(Offset.Y);
			const FVector Location(
				static_cast<double>(PlacementX),
				static_cast<double>(PlacementY),
				FounderSpawnZ);
			Character = World.SpawnActor<AAIityFounderCharacter>(
				Location, FRotator::ZeroRotator, SpawnParameters);
			if (Character)
			{
				Character->InitializeFounder(
					Founder.Id, UTF8_TO_TCHAR(Founder.Name.c_str()), FounderIndex);
				Created.Add(Character);
				if (Offset != FIntPoint::ZeroValue)
				{
					UE_LOG(LogTemp, Log,
						TEXT("Founder %llu presentation placement offset by (%d, %d) from durable XY."),
						static_cast<unsigned long long>(Founder.Id), Offset.X, Offset.Y);
				}
				break;
			}
		}
		if (!Character)
		{
			for (AAIityFounderCharacter* CreatedCharacter : Created)
			{
				CreatedCharacter->ResetBridgeExecution();
				CreatedCharacter->SetActorEnableCollision(false);
				CreatedCharacter->Destroy();
			}
			for (AAIityFounderCharacter* ExistingCharacter : Existing)
			{
				ExistingCharacter->SetActorEnableCollision(true);
			}
			return false;
		}
	}

	for (AAIityFounderCharacter* ExistingCharacter : Existing)
	{
		ExistingCharacter->Destroy();
	}
	return Created.Num() == static_cast<int32>(Founders.size());
}
