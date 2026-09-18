#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Presentation/AIityPresentationHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityPresentationHelperTest,
	"AIity.Bridge.PresentationHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityPresentationHelperTest::RunTest(const FString& Parameters)
{
	const AIity::FHudLayout Small = AIity::BuildHudLayout(1280.0f, 720.0f);
	const AIity::FHudLayout Large = AIity::BuildHudLayout(1920.0f, 1080.0f);
	TestTrue(TEXT("720p HUD keeps practical content width"), Small.ContentWidth >= 360.0f);
	TestTrue(TEXT("720p HUD reserves long-status space"), Small.StatusHeight >= 110.0f);
	TestTrue(TEXT("HUD remains inside the viewport"), Small.PanelWidth < 1280.0f);
	TestTrue(TEXT("1080p HUD gains content width"), Large.ContentWidth > Small.ContentWidth);
	TestTrue(TEXT("1080p HUD gains status height"), Large.StatusHeight > Small.StatusHeight);

	std::vector<AIity::FFounder> Founders(3);
	Founders[0].Id = 10;
	Founders[1].Id = 20;
	Founders[2].Id = 30;
	TestEqual(TEXT("Next founder advances"), AIity::SelectRelativeFounderId(Founders, 10, 1),
		static_cast<uint64>(20));
	TestEqual(TEXT("Previous founder wraps"), AIity::SelectRelativeFounderId(Founders, 10, -1),
		static_cast<uint64>(30));
	TestEqual(TEXT("Next founder wraps"), AIity::SelectRelativeFounderId(Founders, 30, 1),
		static_cast<uint64>(10));
	return true;
}

#endif
