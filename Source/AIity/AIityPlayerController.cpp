#include "AIityPlayerController.h"

#include "AIityFounderCharacter.h"
#include "AIityWorldSubsystem.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "GameFramework/Pawn.h"
#include "Presentation/AIityPresentationHelpers.h"

AAIityPlayerController::AAIityPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AAIityPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction(TEXT("NextFounder"), IE_Pressed, this, &AAIityPlayerController::SelectNextFounder);
	InputComponent->BindAction(TEXT("PreviousFounder"), IE_Pressed, this, &AAIityPlayerController::SelectPreviousFounder);
	InputComponent->BindAction(TEXT("Follow"), IE_Pressed, this, &AAIityPlayerController::ToggleFollow);
	InputComponent->BindAction(TEXT("PauseWorld"), IE_Pressed, this, &AAIityPlayerController::TogglePause);
	InputComponent->BindAction(TEXT("RetryPersistence"), IE_Pressed, this, &AAIityPlayerController::RetryPersistence);
	InputComponent->BindAction(TEXT("Speed1"), IE_Pressed, this, &AAIityPlayerController::SpeedOne);
	InputComponent->BindAction(TEXT("Speed2"), IE_Pressed, this, &AAIityPlayerController::SpeedTwo);
	InputComponent->BindAction(TEXT("Speed4"), IE_Pressed, this, &AAIityPlayerController::SpeedFour);
}

bool AAIityPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bHandled = Super::InputKey(Params);
	if (Params.Viewport && Params.Key == EKeys::LeftMouseButton &&
		(Params.Event == IE_Pressed || Params.Event == IE_DoubleClick))
	{
		// The viewport has just cached this event's local position. A deferred action
		// can run after MouseLeave invalidates that position or another event moves it.
		FIntPoint Position;
		Params.Viewport->GetMousePos(Position);
		const FIntPoint Size = Params.Viewport->GetSizeXY();
		FHitResult Hit;
		if (Position.X >= 0 && Position.Y >= 0 && Position.X < Size.X && Position.Y < Size.Y &&
			GetHitResultAtScreenPosition(FVector2D(Position), ECC_Pawn, false, Hit))
		{
			if (const AAIityFounderCharacter* Founder = Cast<AAIityFounderCharacter>(Hit.GetActor()))
			{
				SelectedFounderId = Founder->GetFounderId();
				return true;
			}
		}
	}
	return bHandled;
}

void AAIityPlayerController::SelectNextFounder()
{
	SelectRelativeFounder(1);
}

void AAIityPlayerController::SelectPreviousFounder()
{
	SelectRelativeFounder(-1);
}

void AAIityPlayerController::SelectRelativeFounder(int32 Direction)
{
	if (const UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		SelectedFounderId = AIity::SelectRelativeFounderId(
			Simulation->GetCommittedState().Founders, SelectedFounderId, Direction);
	}
}

void AAIityPlayerController::ToggleFollow()
{
	bFollowing = !bFollowing;
}

void AAIityPlayerController::TogglePause()
{
	if (UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		Simulation->TogglePause();
	}
}

void AAIityPlayerController::RetryPersistence()
{
	if (UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		Simulation->RetryPersistence();
	}
}

void AAIityPlayerController::SpeedOne()
{
	if (UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		Simulation->SetSpeed(1);
	}
}

void AAIityPlayerController::SpeedTwo()
{
	if (UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		Simulation->SetSpeed(2);
	}
}

void AAIityPlayerController::SpeedFour()
{
	if (UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>())
	{
		Simulation->SetSpeed(4);
	}
}

void AAIityPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!bFollowing || !GetPawn())
	{
		return;
	}
	for (TActorIterator<AAIityFounderCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFounderId() == SelectedFounderId)
		{
			const FVector Target = It->GetActorLocation();
			const FVector CameraLocation = Target + FVector(-650.0f, -650.0f, 520.0f);
			GetPawn()->SetActorLocation(FMath::VInterpTo(GetPawn()->GetActorLocation(), CameraLocation, DeltaTime, 3.0f));
			SetControlRotation((Target - GetPawn()->GetActorLocation()).Rotation());
			break;
		}
	}
}
