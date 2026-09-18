#if WITH_DEV_AUTOMATION_TESTS

#include "AIityObserverPawn.h"
#include "AIityFounderCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "AIityPlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"

namespace
{
// No OS window or global cursor: model the viewport position at delivery and after leave.
class FObserverClickViewport final : public FSceneViewport
{
public:
	FObserverClickViewport() : FSceneViewport(TSharedPtr<SViewport>()) { SizeX = 1280; SizeY = 720; }
	FIntPoint Cursor { 640, 360 };
	virtual int32 GetMouseX() const override { return Cursor.X; }
	virtual int32 GetMouseY() const override { return Cursor.Y; }
	virtual void GetMousePos(FIntPoint& Position, bool = true) override { Position = Cursor; }
};
}

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

		FObserverClickViewport Viewport;
		ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		LocalPlayer->PlayerController = Controller;
		LocalPlayer->ViewportClient = NewObject<UGameViewportClient>(GEngine);
		LocalPlayer->ViewportClient->Viewport = &Viewport;
		Controller->SetViewTarget(Pawn);
		Controller->PlayerCameraManager->UpdateCamera(0.0f);
		// At time zero GetPlayerViewPoint can use the pawn instead of the camera cache.
		// Place the target on the actual projection ray used by screen-position traces.
		FVector RayOrigin = FVector::ZeroVector;
		FVector RayDirection = FVector::ZeroVector;
		TestTrue(TEXT("Viewport center deprojects"), Controller->DeprojectScreenPositionToWorld(
			640.0f, 360.0f, RayOrigin, RayDirection));
		TestTrue(TEXT("Projection ray is normalized"), RayDirection.IsNormalized());
		const FVector FounderLocation = RayOrigin + RayDirection * 1000.0f;
		AAIityFounderCharacter* Founder = World->SpawnActor<AAIityFounderCharacter>(
			FounderLocation, FRotator::ZeroRotator);
		if (TestNotNull(TEXT("Click target founder"), Founder))
		{
			Founder->InitializeFounder(2, TEXT("Click target"), 0);
			TestTrue(TEXT("Founder capsule has query collision"), Founder->GetCapsuleComponent()->IsQueryCollisionEnabled());
			TestEqual(TEXT("Founder capsule blocks selection channel"),
				Founder->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
			FHitResult CenterHit;
			TestTrue(TEXT("Center screen trace hits fixture"), Controller->GetHitResultAtScreenPosition(
				FVector2D(640, 360), ECC_Pawn, false, CenterHit));
			TestTrue(TEXT("Center hit is the intended founder"), CenterHit.GetActor() == Founder);
			Controller->InputKey(FInputKeyEventArgs(&Viewport, INPUTDEVICEID_NONE,
				EKeys::LeftMouseButton, IE_Pressed, 0));
			TestEqual(TEXT("Founder selected at event delivery before processing frame"),
				Controller->GetSelectedFounderId(), uint64(2));
			Viewport.Cursor = FIntPoint(-1, -1);
			Process();
			TestEqual(TEXT("Pointer leaving before frame cannot erase delivered selection"),
				Controller->GetSelectedFounderId(), uint64(2));
			Founder->InitializeFounder(3, TEXT("Changed target"), 0);
			Controller->InputKey(FInputKeyEventArgs(&Viewport, INPUTDEVICEID_NONE,
				EKeys::LeftMouseButton, IE_Pressed, 0));
			TestEqual(TEXT("Invalid event position cannot reuse stale coordinates"),
				Controller->GetSelectedFounderId(), uint64(2));
		}
		LocalPlayer->ViewportClient->Viewport = nullptr;
	}
	World->DestroyWorld(true);
	World->SetPhysicsScene(nullptr);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

#endif
