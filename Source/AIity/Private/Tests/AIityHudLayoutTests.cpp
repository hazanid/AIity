#if WITH_DEV_AUTOMATION_TESTS

#include "AIityHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Presentation/AIityPresentationHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityHudLayoutTest, "AIity.Presentation.HudLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityHudLayoutTest::RunTest(const FString& Parameters)
{
	// Font measurement only: never begin play, open a save, or create a graphical session.
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	if (!TestNotNull(TEXT("Transient world"), World)) return false;
	World->AddToRoot();
	GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(World);
	AAIityHUD* HUD = World->SpawnActor<AAIityHUD>();
	if (TestNotNull(TEXT("HUD"), HUD))
	{
		HUD->Canvas = NewObject<UCanvas>(HUD);
		UFont* Font = GEngine->GetSmallFont();
		if (TestNotNull(TEXT("Engine body font"), Font))
		{
			for (const float Height : {720.0f, 1080.0f})
			{
				const float Width = Height * 16.0f / 9.0f;
				HUD->Canvas->Init(static_cast<int32>(Width), static_cast<int32>(Height), nullptr, nullptr);
				const AIity::FHudLayout Layout = AIity::BuildHudLayout(Width, Height);
				float TextWidth = 0.0f;
				float TextHeight = 0.0f;
				const float Scale = HUD->GetFontScale(Font, Layout.BodyTextHeight);
				HUD->GetTextSize(TEXT("Ag"), TextWidth, TextHeight, Font, Scale);
				TestTrue(TEXT("Measured body text is at least 16 pixels"), TextHeight >= 16.0f);
				const auto Measure = [&](const FString& Text, float Bottom = MAX_flt)
				{
					return HUD->DrawWrappedText(Text, 0, 0, Layout.ContentWidth,
						FLinearColor::White, Font, Scale, Bottom, false);
				};
				const float Line = Measure(TEXT("Saved"));
				const FString LongError = TEXT("Save failed: the world remains paused at the last committed tick. ")
					TEXT("The storage reserve is exhausted. Free disk space and press R to retry; ")
					TEXT("the original save is preserved and no offline time will be applied.");
				const float ErrorHeight = Measure(LongError);
				TestTrue(TEXT("Long status wraps into several measured lines"), ErrorHeight > Line * 2.0f);
				const float StatusContent = Line + 3.0f * Layout.Scale + ErrorHeight;
				const AIity::FHudLayout ErrorLayout = AIity::BuildHudLayout(Width, Height, StatusContent);
				TestTrue(TEXT("Long failure fits the reserved status region"),
					StatusContent + 2.0f * Layout.Padding <= ErrorLayout.StatusHeight);
				TestTrue(TEXT("Overlong name tokens wrap"), Measure(FString::ChrN(180, TCHAR('W'))) > Line);
				TestTrue(TEXT("Overflow stays inside its measured vertical bound"),
					Measure(LongError, Line * 2.0f + 1.0f) <= Line * 2.0f + 1.0f);
				TestEqual(TEXT("No room does not advance into adjacent content"), Measure(LongError, Line - 1.0f), 0.0f);
			}
		}
	}
	World->DestroyWorld(true);
	World->SetPhysicsScene(nullptr);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

#endif
