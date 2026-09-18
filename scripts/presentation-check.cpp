#include "../Source/AIity/Presentation/AIityPresentationHelpers.h"

#include <cassert>
#include <iostream>

int main()
{
	const AIity::FHudLayout Small = AIity::BuildHudLayout(1280.0f, 720.0f);
	const AIity::FHudLayout Large = AIity::BuildHudLayout(1920.0f, 1080.0f);
	assert(Small.BodyTextHeight >= 16.0f);
	assert(Small.HeadingTextHeight > Small.BodyTextHeight);
	assert(Large.BodyTextHeight >= Small.BodyTextHeight);
	assert(Small.ContentWidth >= 400.0f);
	assert(Large.ContentWidth > Small.ContentWidth);
	for (const float Height : {720.0f, 1080.0f})
	{
		const float Width = Height * 16.0f / 9.0f;
		for (const float StatusTextHeight : {40.0f, 180.0f, 10000.0f})
		{
			const AIity::FHudLayout Layout = AIity::BuildHudLayout(Width, Height, StatusTextHeight);
			assert(Layout.PanelWidth + 2.0f * Layout.Margin <= Width);
			assert(Layout.ContentWidth + 2.0f * Layout.Padding <= Layout.PanelWidth);
			assert(Layout.StatusHeight <= Height * 0.4f);
			assert(Height - Layout.Margin - Layout.StatusHeight - Layout.Padding > Height * 0.5f);
			if (StatusTextHeight < 200.0f)
			{
				assert(Layout.StatusHeight >= StatusTextHeight + 2.0f * Layout.Padding);
			}
		}
	}

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
