#include "AIityObserverPawn.h"

#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawnMovement.h"

namespace
{
const FVector InitialViewLocation(-2200.0f, -2600.0f, 2100.0f);
const FRotator InitialViewRotation(-28.0f, 40.0f, 0.0f);
}

AAIityObserverPawn::AAIityObserverPawn()
{
	bAddDefaultMovementBindings = false;
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	if (USpectatorPawnMovement* Movement = Cast<USpectatorPawnMovement>(GetMovementComponent()))
	{
		Movement->MaxSpeed = 1800.0f;
		Movement->Acceleration = 5000.0f;
		Movement->Deceleration = 6000.0f;
	}
}

void AAIityObserverPawn::Tick(float DeltaSeconds)
{
	// GameMode finishes restart by resetting control rotation after possession.
	// Apply the overview once after that sequence, not inside PossessedBy.
	if (bNeedsInitialView && Controller)
	{
		SetActorLocation(InitialViewLocation);
		SetActorRotation(InitialViewRotation);
		Controller->SetControlRotation(InitialViewRotation);
		bNeedsInitialView = false;
	}
	Super::Tick(DeltaSeconds);
}

void AAIityObserverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AAIityObserverPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AAIityObserverPawn::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("MoveUp"), this, &AAIityObserverPawn::MoveUp_World);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AAIityObserverPawn::LookYaw);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AAIityObserverPawn::LookPitch);
}

void AAIityObserverPawn::LookYaw(float Value)
{
	const APlayerController* Player = Cast<APlayerController>(Controller);
	if (Player && Player->IsInputKeyDown(EKeys::RightMouseButton))
	{
		AddControllerYawInput(Value);
	}
}

void AAIityObserverPawn::LookPitch(float Value)
{
	const APlayerController* Player = Cast<APlayerController>(Controller);
	if (Player && Player->IsInputKeyDown(EKeys::RightMouseButton))
	{
		AddControllerPitchInput(Value);
	}
}
