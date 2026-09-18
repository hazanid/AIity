#pragma once

#include "AIityWorldState.h"

#include <utility>
#include <vector>

namespace AIity
{
class FBridgeReceiptQueue
{
public:
	void Add(const FMovementReceipt& Receipt) { Pending.push_back(Receipt); }
	bool IsEmpty() const { return Pending.empty(); }
	std::size_t Size() const { return Pending.size(); }
	void Clear() { Pending.clear(); }

	std::vector<FMovementReceipt> Take()
	{
		std::vector<FMovementReceipt> Taken;
		Taken.swap(Pending);
		return Taken;
	}

private:
	std::vector<FMovementReceipt> Pending;
};

template <typename TPawn, typename TMovement>
void FreezeQueuedMovement(TPawn& Pawn, TMovement& Movement)
{
	Pawn.ConsumeMovementInputVector();
	Movement.StopMovementImmediately();
	Movement.ClearAccumulatedForces();
}
}
