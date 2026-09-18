#include "AIityObserverPawn.h"

#include "GameFramework/Controller.h"
#include "GameFramework/SpectatorPawnMovement.h"

namespace
{
const FVector InitialViewLocation(-2200.0f, -2600.0f, 2100.0f);
const FRotator InitialViewRotation(-28.0f, 40.0f, 0.0f);
}

AAIityObserverPawn::AAIityObserverPawn()
{
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	if (USpectatorPawnMovement* Movement = Cast<USpectatorPawnMovement>(GetMovementComponent()))
	{
		Movement->MaxSpeed = 1800.0f;
		Movement->Acceleration = 5000.0f;
		Movement->Deceleration = 6000.0f;
	}
}

void AAIityObserverPawn::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(InitialViewLocation);
	SetActorRotation(InitialViewRotation);
	if (Controller)
	{
		Controller->SetControlRotation(InitialViewRotation);
	}
}

void AAIityObserverPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SetActorLocation(InitialViewLocation);
	SetActorRotation(InitialViewRotation);
	if (NewController)
	{
		NewController->SetControlRotation(InitialViewRotation);
	}
}
