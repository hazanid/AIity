#if WITH_DEV_AUTOMATION_TESTS

#include "AIityObserverPawn.h"
#include "AIityPlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityObserverInputTest, "AIity.Presentation.ObserverInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityObserverInputTest::RunTest(const FString& Parameters)
{
	// Never begin play: this transient world cannot open a production save.
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	if (!TestNotNull(TEXT("Transient world"), World)) return false;
	World->AddToRoot();
	GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	AAIityPlayerController* Controller = World->SpawnActor<AAIityPlayerController>();
	AAIityObserverPawn* Pawn = World->SpawnActor<AAIityObserverPawn>();
	if (TestNotNull(TEXT("Controller"), Controller) && TestNotNull(TEXT("Observer"), Pawn))
	{
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
		Controller->Possess(Pawn);
		TestTrue(TEXT("Input fixture is locally controlled"), Controller->IsLocalPlayerController());
		// Reproduce GameMode's post-possession overwrite, then first gameplay tick.
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Pawn->Tick(0.0f);
		TestTrue(TEXT("Overview rotation survives restart"),
			Controller->GetControlRotation().Equals(FRotator(-28.0f, 40.0f, 0.0f)));
		TestTrue(TEXT("Overview location"),
			Pawn->GetActorLocation().Equals(FVector(-2200.0f, -2600.0f, 2100.0f)));
		Controller->SetControlRotation(FRotator(-15.0f, 80.0f, 0.0f));
		Pawn->Tick(0.0f);
		TestTrue(TEXT("Later camera input is not reset"),
			Controller->GetControlRotation().Equals(FRotator(-15.0f, 80.0f, 0.0f)));

		UInputComponent* Input = NewObject<UInputComponent>(Pawn);
		Pawn->SetupPlayerInputComponent(Input);
		TestEqual(TEXT("Only explicit project axes are bound"), Input->AxisBindings.Num(), 5);
		Controller->RotationInput = FRotator::ZeroRotator;
		for (FInputAxisBinding& Binding : Input->AxisBindings)
		{
			if (Binding.AxisName == TEXT("Turn") || Binding.AxisName == TEXT("LookUp"))
			{
				Binding.AxisDelegate.Execute(100.0f);
			}
			TestFalse(TEXT("No always-on inherited spectator axis"),
				Binding.AxisName.ToString().StartsWith(TEXT("DefaultPawn_")));
		}
		TestTrue(TEXT("Pointer motion without RMB cannot rotate"), Controller->RotationInput.IsZero());

		// Exercise configured key mappings through native input processing, not just delegates.
		const TArray<UInputComponent*> Stack { Input };
		auto Key = [&](FKey Value, EInputEvent Event)
		{
			Controller->PlayerInput->InputKey(FInputKeyEventArgs(
				nullptr, INPUTDEVICEID_NONE, Value, Event, 0));
		};
		auto Process = [&]()
		{
			Controller->PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
		};
		auto Mouse = [&]()
		{
			for (FKey Axis : { EKeys::MouseX, EKeys::MouseY })
			{
				Controller->PlayerInput->InputKey(FInputKeyEventArgs(
					nullptr, INPUTDEVICEID_NONE, Axis, 10.0f, 1.0f / 60.0f, 1, 0));
			}
			Process();
		};
		Key(EKeys::RightMouseButton, IE_Pressed);
		Mouse();
		TestTrue(TEXT("Held RMB MouseX maps to yaw"), !FMath::IsNearlyZero(Controller->RotationInput.Yaw));
		TestTrue(TEXT("Held RMB MouseY maps to pitch"), !FMath::IsNearlyZero(Controller->RotationInput.Pitch));
		Key(EKeys::RightMouseButton, IE_Released);
		Controller->RotationInput = FRotator::ZeroRotator;
		Mouse();
		TestTrue(TEXT("RMB release stops look"), Controller->RotationInput.IsZero());
		Key(EKeys::LeftMouseButton, IE_Pressed);
		Mouse();
		TestTrue(TEXT("Selection click does not enable look"), Controller->RotationInput.IsZero());
		Key(EKeys::LeftMouseButton, IE_Released);
		Process();

		Controller->SetControlRotation(FRotator::ZeroRotator);
		const FKey MoveKeys[] = { EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::Q, EKeys::E };
		const FVector Directions[] = {
			FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, -1, 0),
			FVector(0, 1, 0), FVector(0, 0, -1), FVector(0, 0, 1) };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(MoveKeys); ++Index)
		{
			Pawn->ConsumeMovementInputVector();
			Key(MoveKeys[Index], IE_Pressed);
			Process();
			TestTrue(*FString::Printf(TEXT("%s maps to expected movement"), *MoveKeys[Index].ToString()),
				Pawn->ConsumeMovementInputVector().Equals(Directions[Index]));
			Key(MoveKeys[Index], IE_Released);
			Process();
		}
		Pawn->ConsumeMovementInputVector();
		Key(EKeys::SpaceBar, IE_Pressed);
		Process();
		TestTrue(TEXT("Pause key supplies no observer movement"), Pawn->ConsumeMovementInputVector().IsZero());
		Key(EKeys::SpaceBar, IE_Released);
		Process();
	}
	World->DestroyWorld(true);
	World->SetPhysicsScene(nullptr);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

#endif
