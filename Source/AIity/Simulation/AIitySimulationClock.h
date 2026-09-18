#pragma once

#include <algorithm>
#include <cstdint>

namespace AIity
{
class FSimulationClock
{
public:
	bool IsReady() const { return bReady; }
	bool IsPaused() const { return bPaused; }
	bool HasFailure() const { return bFailed; }
	bool HasPermanentFailure() const { return bPermanentFailure; }
	bool CanRetry() const { return bReady && bFailed && !bPermanentFailure; }
	int32_t GetSpeed() const { return Speed; }
	bool CanAdvance() const { return bReady && !bPaused && !bFailed; }

	void MarkReady()
	{
		bReady = true;
		bPaused = true;
		bFailed = false;
		bPermanentFailure = false;
	}

	void MarkFailed()
	{
		bPaused = true;
		bFailed = true;
	}

	void MarkStartupFailed()
	{
		bPaused = true;
		bFailed = true;
		bPermanentFailure = true;
	}

	bool MarkRetryReady()
	{
		if (CanRetry())
		{
			bPaused = true;
			bFailed = false;
			return true;
		}
		return false;
	}

	bool TogglePause()
	{
		if (!bReady || bFailed)
		{
			return false;
		}
		bPaused = !bPaused;
		return true;
	}

	bool SetSpeed(int32_t NewSpeed)
	{
		if (!bReady || (NewSpeed != 1 && NewSpeed != 2 && NewSpeed != 4))
		{
			return false;
		}
		Speed = NewSpeed;
		return true;
	}

	float ScaleDelta(float RealSeconds) const
	{
		if (!CanAdvance() || RealSeconds <= 0.0f)
		{
			return 0.0f;
		}
		return std::min(MaxSimulationSecondsPerFrame,
			RealSeconds * static_cast<float>(Speed));
	}

	float MovementSpeedScale(float RealSeconds) const
	{
		return RealSeconds > 0.0f ? ScaleDelta(RealSeconds) / RealSeconds : 0.0f;
	}

	int32_t AccumulateWholeTicks(float RealSeconds, float& Accumulator) const
	{
		Accumulator += ScaleDelta(RealSeconds);
		const int32_t WholeTicks = static_cast<int32_t>(Accumulator);
		Accumulator -= static_cast<float>(WholeTicks);
		return WholeTicks;
	}

private:
	static constexpr float MaxSimulationSecondsPerFrame = 4.0f;
	int32_t Speed = 1;
	bool bReady = false;
	bool bPaused = true;
	bool bFailed = false;
	bool bPermanentFailure = false;
};
}
