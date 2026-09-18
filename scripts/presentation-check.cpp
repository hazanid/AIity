#include "../Source/AIity/Presentation/AIityPresentationHelpers.h"

#include <cassert>
#include <iostream>

int main()
{
	const AIity::FHudLayout Small = AIity::BuildHudLayout(1280.0f, 720.0f);
	const AIity::FHudLayout Large = AIity::BuildHudLayout(1920.0f, 1080.0f);
	assert(Small.ContentWidth >= 360.0f);
	assert(Small.StatusHeight >= 110.0f);
	assert(Small.PanelWidth < 1280.0f);
	assert(Large.ContentWidth > Small.ContentWidth);
	assert(Large.StatusHeight > Small.StatusHeight);

	std::vector<AIity::FFounder> Founders(3);
	Founders[0].Id = 10;
	Founders[1].Id = 20;
	Founders[2].Id = 30;
	assert(AIity::SelectRelativeFounderId(Founders, 10, 1) == 20);
	assert(AIity::SelectRelativeFounderId(Founders, 10, -1) == 30);
	assert(AIity::SelectRelativeFounderId(Founders, 30, 1) == 10);
	assert(AIity::SelectRelativeFounderId(Founders, 999, 1) == 10);
	assert(AIity::SelectRelativeFounderId(Founders, 999, -1) == 30);

	std::cout << "AIity portable presentation checks passed\n";
	return 0;
}
