#if WITH_DEV_AUTOMATION_TESTS

#include "AIityObserverPawn.h"
#include "AIityPlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
		Controller->Possess(Pawn);
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
	}
	World->DestroyWorld(true);
	World->SetPhysicsScene(nullptr);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

#endif
