#pragma once

#include <cstdint>

namespace AIity
{
constexpr uint64_t CheckpointInterval = 300;
constexpr uint64_t RetainedCheckpointCount = 8;

constexpr bool ShouldCheckpoint(uint64_t Tick)
{
	return Tick % CheckpointInterval == 0;
}
}
