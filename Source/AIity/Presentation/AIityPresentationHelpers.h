#pragma once

#include "../Simulation/AIityWorldState.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace AIity
{
struct FHudLayout
{
	float Scale = 1.0f;
	float Margin = 16.0f;
	float PanelWidth = 500.0f;
	float Padding = 16.0f;
	float ContentWidth = 468.0f;
	float StatusHeight = 120.0f;
};

inline FHudLayout BuildHudLayout(float ViewportWidth, float ViewportHeight)
{
	FHudLayout Layout;
	Layout.Scale = std::max(0.85f, std::min(1.1f,
		std::min(ViewportWidth / 1920.0f, ViewportHeight / 1080.0f)));
	Layout.Margin = 16.0f * Layout.Scale;
	const float AvailableWidth = std::max(1.0f, ViewportWidth - Layout.Margin * 2.0f);
	Layout.PanelWidth = std::min(AvailableWidth,
		std::max(360.0f, std::min(500.0f * Layout.Scale, ViewportWidth * 0.38f)));
	Layout.Padding = 16.0f * Layout.Scale;
	Layout.ContentWidth = std::max(1.0f, Layout.PanelWidth - Layout.Padding * 2.0f);
	Layout.StatusHeight = std::max(96.0f * Layout.Scale, ViewportHeight * 0.16f);
	return Layout;
}

inline uint64_t SelectRelativeFounderId(const std::vector<FFounder>& Founders,
	uint64_t CurrentId, int32_t Direction)
{
	if (Founders.empty())
	{
		return 0;
	}
	auto Current = std::find_if(Founders.begin(), Founders.end(),
		[CurrentId](const FFounder& Founder) { return Founder.Id == CurrentId; });
	if (Current == Founders.end())
	{
		return Direction < 0 ? Founders.back().Id : Founders.front().Id;
	}
	const size_t Index = static_cast<size_t>(Current - Founders.begin());
	if (Direction < 0)
	{
		return Founders[(Index + Founders.size() - 1) % Founders.size()].Id;
	}
	return Founders[(Index + 1) % Founders.size()].Id;
}
}
