#include "AIityFounderCharacter.h"

#include "AIityWorldSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Simulation/AIityBridgeProtocol.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float BaseWalkSpeed = 230.0f;
}

AAIityFounderCharacter::AAIityFounderCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(36.0f, 88.0f);
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	// UMovementComponent normally ticks before its owner. Reverse that default before
	// registration so the owner can clear or queue input before CharacterMovement runs.
	GetCharacterMovement()->bTickBeforeOwner = false;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -22.0f));
	Body->SetRelativeScale3D(FVector(0.48f, 0.48f, 1.15f));
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetMaterial(0, Material.Object);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(GetCapsuleComponent());
	Head->SetMobility(EComponentMobility::Movable);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	Head->SetRelativeScale3D(FVector(0.34f));
	Head->SetStaticMesh(Sphere.Object);
	Head->SetMaterial(0, Material.Object);
}

void AAIityFounderCharacter::InitializeFounder(uint64 InFounderId, const FString& InName, int32 ColorIndex)
{
	FounderId = InFounderId;
	ResetBridgeExecution();
#if WITH_EDITOR
	SetActorLabel(InName);
#else
	(void)InName;
#endif
	static const FLinearColor Colors[] = {
		FLinearColor(0.64f, 0.24f, 0.12f), FLinearColor(0.16f, 0.45f, 0.55f),
		FLinearColor(0.54f, 0.48f, 0.18f), FLinearColor(0.28f, 0.54f, 0.24f),
		FLinearColor(0.52f, 0.22f, 0.38f), FLinearColor(0.74f, 0.38f, 0.13f),
		FLinearColor(0.18f, 0.37f, 0.30f), FLinearColor(0.45f, 0.25f, 0.58f),
		FLinearColor(0.63f, 0.55f, 0.32f), FLinearColor(0.22f, 0.40f, 0.62f)
	};
	if (UMaterialInstanceDynamic* Material = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Colors[ColorIndex % UE_ARRAY_COUNT(Colors)]);
	}
	if (UMaterialInstanceDynamic* Material = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.65f, 0.42f, 0.28f));
	}
}

void AAIityFounderCharacter::ResetBridgeExecution()
{
	AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
	ActiveActionId = 0;
	ActiveEpoch = 0;
	MovementSeconds = 0.0f;
	bTerminalSent = false;
}

void AAIityFounderCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && Controller == nullptr)
	{
		SpawnDefaultController();
	}
	// bTickBeforeOwner is disabled above; adding only this direction avoids a tick cycle.
	GetCharacterMovement()->AddTickPrerequisiteActor(this);
}

void AAIityFounderCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>();
	if (!Simulation || FounderId == 0)
	{
		AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
		return;
	}
	const AIity::FFounder* Founder = nullptr;
	for (const AIity::FFounder& Item : Simulation->GetCommittedState().Founders)
	{
		if (Item.Id == FounderId)
		{
			Founder = &Item;
			break;
		}
	}
	const float SimulationDelta = Simulation->GetSimulationDelta(DeltaSeconds);
	if (SimulationDelta <= 0.0f)
	{
		AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = BaseWalkSpeed * Simulation->GetMovementSpeedScale(DeltaSeconds);
	const FVector BoundedHorizontalVelocity =
		FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.0f).GetClampedToMaxSize(Movement->MaxWalkSpeed);
	Movement->Velocity.X = BoundedHorizontalVelocity.X;
	Movement->Velocity.Y = BoundedHorizontalVelocity.Y;
	if (!Founder || !Founder->Action.AwaitingMovement)
	{
		AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
		ActiveActionId = 0;
		return;
	}
	if (ActiveActionId != Founder->Action.Id || ActiveEpoch != Founder->Action.Epoch)
	{
		ActiveActionId = Founder->Action.Id;
		ActiveEpoch = Founder->Action.Epoch;
		MovementSeconds = 0.0f;
		bTerminalSent = false;
	}
	if (bTerminalSent)
	{
		AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
		return;
	}

	const FVector Target(static_cast<float>(Founder->Action.TargetX), static_cast<float>(Founder->Action.TargetY), GetActorLocation().Z);
	const FVector Offset = Target - GetActorLocation();
	if (Offset.SizeSquared2D() <= FMath::Square(90.0f))
	{
		AIity::FMovementReceipt Receipt{ActiveActionId, FounderId, ActiveActionId, ActiveEpoch,
			AIity::EMovementOutcome::Arrived, FMath::RoundToInt(GetActorLocation().X), FMath::RoundToInt(GetActorLocation().Y)};
		Simulation->SubmitMovementReceipt(Receipt);
		bTerminalSent = true;
		AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
	}
	else if (MovementSeconds >= 20.0f)
	{
		AIity::FMovementReceipt Receipt{ActiveActionId, FounderId, ActiveActionId, ActiveEpoch,
			AIity::EMovementOutcome::Blocked, FMath::RoundToInt(GetActorLocation().X), FMath::RoundToInt(GetActorLocation().Y)};
		Simulation->SubmitMovementReceipt(Receipt);
		bTerminalSent = true;
		AIity::FreezeQueuedMovement(*this, *GetCharacterMovement());
	}
	else
	{
		MovementSeconds += SimulationDelta;
		AddMovementInput(Offset.GetSafeNormal2D());
	}
}
