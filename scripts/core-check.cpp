#include "../Source/AIity/Simulation/AIityRules.h"
#include "../Source/AIity/Simulation/AIitySerialization.h"
#include "../Source/AIity/Simulation/AIityBridgeProtocol.h"
#include "../Source/AIity/Simulation/AIityPersistencePolicy.h"
#include "../Source/AIity/Simulation/AIitySimulationClock.h"

#include <cassert>
#include <iostream>
#include <limits>

using namespace AIity;

static int TotalFood(const FWorldState& State)
{
	int Total = 0;
	for (const FFounder& Founder : State.Founders)
	{
		Total += Founder.Food;
	}
	for (const FResourceNode& Resource : State.Resources)
	{
		if (Resource.Kind == EResourceKind::Food)
		{
			Total += Resource.Amount;
		}
	}
	return Total;
}

struct FFakePawn
{
	bool bConsumed = false;
	void ConsumeMovementInputVector() { bConsumed = true; }
};

struct FFakeMovement
{
	bool bStopped = false;
	bool bForcesCleared = false;
	void StopMovementImmediately() { bStopped = true; }
	void ClearAccumulatedForces() { bForcesCleared = true; }
};

int main()
{
	const FWorldState Initial = FRules::CreateFoundationWorld(0xA117ULL);
	std::string Error;
	assert(FRules::Validate(Initial, Error));
	assert(Initial.Founders.size() == 10);
	assert(Initial.Founders.front().Name == "Aru");
	assert(Initial.Founders.back().Name == "Jora");

	const FCandidateTick First = FRules::BuildCandidate(Initial, {});
	const FCandidateTick Repeat = FRules::BuildCandidate(Initial, {});
	assert(SerializeWorld(First.State) == SerializeWorld(Repeat.State));

	FWorldState ExactResource = Initial;
	for (FResourceNode& Resource : ExactResource.Resources)
	{
		Resource.X = ExactResource.Founders.front().X;
		Resource.Y = ExactResource.Founders.front().Y;
	}
	const FCandidateTick ExactStarted = FRules::BuildCandidate(ExactResource, {});
	const FFounder& ExactFounder = ExactStarted.State.Founders.front();
	const FAction ExactAction = ExactFounder.Action;
	assert(ExactAction.Kind == EActionKind::GatherFood || ExactAction.Kind == EActionKind::GatherWater);
	assert(ExactAction.AwaitingMovement);
	const auto ResourceAmount = [](const FWorldState& State, uint64_t ResourceId)
	{
		for (const FResourceNode& Resource : State.Resources)
		{
			if (Resource.Id == ResourceId)
			{
				return Resource.Amount;
			}
		}
		return -1;
	};
	const int ExactAmountBefore = ResourceAmount(ExactStarted.State, ExactAction.TargetId);
	const int ExactFoodBefore = ExactFounder.Food;
	const int ExactWaterBefore = ExactFounder.Water;
	const FCandidateTick DisplacedWithoutReceipt = FRules::BuildCandidate(ExactStarted.State, {});
	assert(ResourceAmount(DisplacedWithoutReceipt.State, ExactAction.TargetId) == ExactAmountBefore);
	assert(DisplacedWithoutReceipt.State.Founders.front().Food == ExactFoodBefore);
	assert(DisplacedWithoutReceipt.State.Founders.front().Water == ExactWaterBefore);
	assert(DisplacedWithoutReceipt.State.Founders.front().Action.Id == ExactAction.Id);
	FMovementReceipt ExactArrival{ExactAction.Id, ExactFounder.Id, ExactAction.Id, ExactAction.Epoch,
		EMovementOutcome::Arrived, ExactAction.TargetX, ExactAction.TargetY};
	FMovementReceipt OutsideArrival = ExactArrival;
	OutsideArrival.X += 151;
	const FCandidateTick Outside = FRules::BuildCandidate(ExactStarted.State, {OutsideArrival});
	assert(Outside.ConsumedReceiptIds.empty());
	assert(Outside.State.Founders.front().Action.AwaitingMovement);
	assert(ResourceAmount(Outside.State, ExactAction.TargetId) == ExactAmountBefore);
	assert(Outside.State.Founders.front().Food == ExactFoodBefore);
	assert(Outside.State.Founders.front().Water == ExactWaterBefore);
	const FCandidateTick ExactCompleted = FRules::BuildCandidate(ExactStarted.State, {ExactArrival});
	assert(ResourceAmount(ExactCompleted.State, ExactAction.TargetId) == ExactAmountBefore - 1);
	assert(ExactCompleted.State.Founders.front().Food + ExactCompleted.State.Founders.front().Water ==
		ExactFoodBefore + ExactWaterBefore + 1);
	const FCandidateTick ExactDuplicate = FRules::BuildCandidate(ExactCompleted.State, {ExactArrival});
	assert(ResourceAmount(ExactDuplicate.State, ExactAction.TargetId) == ExactAmountBefore - 1);

	const int FoodBefore = TotalFood(First.State);
	const FFounder& Moving = First.State.Founders.front();
	FMovementReceipt Arrival{Moving.Action.Id, Moving.Id, Moving.Action.Id, Moving.Action.Epoch,
		EMovementOutcome::Arrived, Moving.Action.TargetX, Moving.Action.TargetY};
	const FCandidateTick Arrived = FRules::BuildCandidate(First.State, {Arrival});
	assert(TotalFood(Arrived.State) == FoodBefore);

	const int FoodAfterArrival = TotalFood(Arrived.State);
	const FCandidateTick Duplicate = FRules::BuildCandidate(Arrived.State, {Arrival});
	assert(TotalFood(Duplicate.State) == FoodAfterArrival);
	assert(Duplicate.ConsumedReceiptIds.empty());

	const FFounder& SecondMoving = First.State.Founders[1];
	FMovementReceipt Blocked{SecondMoving.Action.Id, SecondMoving.Id, SecondMoving.Action.Id, SecondMoving.Action.Epoch,
		EMovementOutcome::Blocked, SecondMoving.X, SecondMoving.Y};
	const FCandidateTick Failed = FRules::BuildCandidate(First.State, {Blocked});
	assert(Failed.State.Founders[1].Food == First.State.Founders[1].Food);
	assert(Failed.State.Founders[1].Water == First.State.Founders[1].Water);

	FWorldState Starving = Initial;
	Starving.Founders.front().Needs.Hunger = 1000;
	Starving.Founders.front().Food = 0;
	const int HealthBefore = Starving.Founders.front().Needs.Health;
	const FCandidateTick Starved = FRules::BuildCandidate(Starving, {});
	assert(Starved.State.Founders.front().Needs.Health < HealthBefore);

	const std::string Saved = SerializeWorld(Arrived.State);
	FWorldState Reopened;
	assert(DeserializeWorld(Saved, Reopened, Error));
	assert(SerializeWorld(Reopened) == Saved);
	assert(Reopened.LogicalSeconds == Arrived.State.LogicalSeconds);

	const FCandidateTick Resume = FRules::BuildResumeCandidate(First.State);
	assert(Resume.State.ExecutionEpoch == First.State.ExecutionEpoch + 1);
	assert(Resume.State.LogicalSeconds == First.State.LogicalSeconds);
	assert(Resume.State.Founders.front().Action.Epoch == Resume.State.ExecutionEpoch);
	FMovementReceipt Stale = Arrival;
	FMovementReceipt Fresh = Arrival;
	Fresh.Epoch = Resume.State.ExecutionEpoch;
	const FCandidateTick StaleThenFresh = FRules::BuildCandidate(Resume.State, {Stale, Fresh});
	assert(StaleThenFresh.ConsumedReceiptIds.size() == 1);
	assert(StaleThenFresh.ConsumedReceiptIds.front() == Fresh.ReceiptId);

	const FCandidateTick Retry = FRules::BuildRetryCandidate(First.State);
	assert(Retry.State.ExecutionEpoch == First.State.ExecutionEpoch + 1);
	assert(Retry.State.LogicalSeconds == First.State.LogicalSeconds);
	const FCandidateTick RejectedPreFailure = FRules::BuildCandidate(Retry.State, {Arrival});
	assert(RejectedPreFailure.ConsumedReceiptIds.empty());
	FMovementReceipt RetryFresh = Arrival;
	RetryFresh.Epoch = Retry.State.ExecutionEpoch;
	const FCandidateTick AcceptedAfterRetry = FRules::BuildCandidate(Retry.State, {RetryFresh});
	assert(AcceptedAfterRetry.ConsumedReceiptIds.size() == 1);

	FWorldState LegacyGather = First.State;
	LegacyGather.Founders.front().Action.AwaitingMovement = false;
	LegacyGather.Founders.front().X = LegacyGather.Founders.front().Action.TargetX;
	LegacyGather.Founders.front().Y = LegacyGather.Founders.front().Action.TargetY;
	const uint64_t LegacyEpoch = LegacyGather.Founders.front().Action.Epoch;
	const auto CheckLegacyGatherTransition =
		[&](const FCandidateTick& Transition)
		{
			const FFounder& TransitionFounder = Transition.State.Founders.front();
			const FAction TransitionAction = TransitionFounder.Action;
			assert(Transition.State.Tick == LegacyGather.Tick + 1);
			assert(Transition.State.LogicalSeconds == LegacyGather.LogicalSeconds);
			assert(TransitionAction.Id == LegacyGather.Founders.front().Action.Id);
			assert(TransitionAction.AwaitingMovement);
			assert(TransitionAction.Epoch == LegacyGather.ExecutionEpoch + 1);
			const int AmountBefore =
				ResourceAmount(Transition.State, TransitionAction.TargetId);
			const int GoodsBefore = TransitionFounder.Food + TransitionFounder.Water;
			const FCandidateTick NoReceipt = FRules::BuildCandidate(Transition.State, {});
			assert(ResourceAmount(NoReceipt.State, TransitionAction.TargetId) == AmountBefore);
			assert(NoReceipt.State.Founders.front().Food +
				NoReceipt.State.Founders.front().Water == GoodsBefore);
			FMovementReceipt StaleLegacy{
				TransitionAction.Id, TransitionFounder.Id, TransitionAction.Id, LegacyEpoch,
				EMovementOutcome::Arrived, TransitionAction.TargetX, TransitionAction.TargetY};
			const FCandidateTick StaleResult =
				FRules::BuildCandidate(Transition.State, {StaleLegacy});
			assert(ResourceAmount(StaleResult.State, TransitionAction.TargetId) == AmountBefore);
			assert(StaleResult.State.Founders.front().Food +
				StaleResult.State.Founders.front().Water == GoodsBefore);
			StaleLegacy.Epoch = TransitionAction.Epoch;
			const FCandidateTick FreshResult =
				FRules::BuildCandidate(Transition.State, {StaleLegacy});
			assert(ResourceAmount(FreshResult.State, TransitionAction.TargetId) == AmountBefore - 1);
			assert(FreshResult.State.Founders.front().Food +
				FreshResult.State.Founders.front().Water == GoodsBefore + 1);
			const FCandidateTick DuplicateResult =
				FRules::BuildCandidate(FreshResult.State, {StaleLegacy});
			assert(ResourceAmount(DuplicateResult.State, TransitionAction.TargetId) == AmountBefore - 1);
		};
	CheckLegacyGatherTransition(FRules::BuildResumeCandidate(LegacyGather));
	CheckLegacyGatherTransition(FRules::BuildRetryCandidate(LegacyGather));

	const uint64_t UnsignedMaximum = std::numeric_limits<uint64_t>::max();
	FWorldState FullRangeText = Initial;
	FullRangeText.Seed = UnsignedMaximum;
	FullRangeText.RandomState = UnsignedMaximum;
	assert(FRules::Validate(FullRangeText, Error));
	FWorldState InvalidSignedState = Initial;
	InvalidSignedState.Tick = UnsignedMaximum;
	assert(!FRules::Validate(InvalidSignedState, Error));
	InvalidSignedState = Initial;
	InvalidSignedState.LogicalSeconds = UnsignedMaximum;
	assert(!FRules::Validate(InvalidSignedState, Error));
	InvalidSignedState = Initial;
	InvalidSignedState.History.push_back(
		FEvent{UnsignedMaximum, 0, 0, "invalid", "out of signed range"});
	assert(!FRules::Validate(InvalidSignedState, Error));
	InvalidSignedState = Initial;
	InvalidSignedState.History.push_back(
		FEvent{1, UnsignedMaximum, 0, "invalid", "out of signed range"});
	assert(!FRules::Validate(InvalidSignedState, Error));
	InvalidSignedState = Initial;
	InvalidSignedState.History.push_back(
		FEvent{1, 0, UnsignedMaximum, "invalid", "out of signed range"});
	assert(!FRules::Validate(InvalidSignedState, Error));
	FWorldState RejectedLoadedRange;
	assert(!DeserializeWorld(
		SerializeWorld(InvalidSignedState), RejectedLoadedRange, Error));
	FCandidateTick InvalidEnvelope = First;
	InvalidEnvelope.Events.front().Id = UnsignedMaximum;
	assert(!FRules::ValidatePersistenceEnvelope(InvalidEnvelope, Error));
	InvalidEnvelope = First;
	InvalidEnvelope.Events.front().Tick = UnsignedMaximum;
	assert(!FRules::ValidatePersistenceEnvelope(InvalidEnvelope, Error));
	InvalidEnvelope = First;
	InvalidEnvelope.Events.front().AgentId = UnsignedMaximum;
	assert(!FRules::ValidatePersistenceEnvelope(InvalidEnvelope, Error));
	InvalidEnvelope = First;
	InvalidEnvelope.ConsumedReceiptIds.push_back(UnsignedMaximum);
	assert(!FRules::ValidatePersistenceEnvelope(InvalidEnvelope, Error));

	FBridgeReceiptQueue ReceiptQueue;
	ReceiptQueue.Add(Arrival);
	assert(ReceiptQueue.Size() == 1);
	const std::vector<FMovementReceipt> DiscardedReceipts = ReceiptQueue.Take();
	assert(DiscardedReceipts.size() == 1 && ReceiptQueue.IsEmpty());
	ReceiptQueue.Add(Arrival);
	ReceiptQueue.Clear();
	assert(ReceiptQueue.IsEmpty());

	FFakePawn BufferedPawn;
	FFakeMovement BufferedMovement;
	FreezeQueuedMovement(BufferedPawn, BufferedMovement);
	assert(BufferedPawn.bConsumed);
	assert(BufferedMovement.bStopped);
	assert(BufferedMovement.bForcesCleared);

	FWorldState Depleted = Initial;
	for (FResourceNode& Resource : Depleted.Resources)
	{
		Resource.Amount = 0;
	}
	for (int Tick = 0; Tick < 1000; ++Tick)
	{
		Depleted = FRules::BuildCandidate(Depleted, {}).State;
	}
	assert(Depleted.History.size() <= 64);
	assert(SerializeWorld(Depleted).size() < 32000);

	FSimulationClock Clock;
	assert(Clock.ScaleDelta(1.0f) == 0.0f);
	Clock.MarkReady();
	assert(Clock.IsPaused());
	assert(Clock.TogglePause());
	assert(Clock.ScaleDelta(1.0f) == 1.0f);
	assert(Clock.SetSpeed(4));
	assert(Clock.ScaleDelta(0.25f) == 1.0f);
	assert(Clock.TogglePause());
	assert(Clock.ScaleDelta(1.0f) == 0.0f);
	Clock.MarkFailed();
	assert(!Clock.TogglePause());
	assert(Clock.ScaleDelta(1.0f) == 0.0f);
	assert(Clock.MarkRetryReady());
	assert(Clock.IsPaused() && !Clock.HasFailure());

	FSimulationClock HitchClock;
	HitchClock.MarkReady();
	assert(HitchClock.TogglePause());
	assert(HitchClock.SetSpeed(4));
	float HitchAccumulator = 0.0f;
	const int32_t HitchTicks = HitchClock.AccumulateWholeTicks(10.0f, HitchAccumulator);
	assert(HitchTicks == 4);
	assert(HitchClock.ScaleDelta(10.0f) == 4.0f);
	assert(HitchClock.MovementSpeedScale(10.0f) == 0.4f);
	FWorldState Hitched = Initial;
	for (int32_t Tick = 0; Tick < HitchTicks; ++Tick)
	{
		Hitched = FRules::BuildCandidate(Hitched, {}).State;
	}
	assert(Hitched.LogicalSeconds == Initial.LogicalSeconds + 4);

	HitchClock.MarkStartupFailed();
	assert(HitchClock.AccumulateWholeTicks(10.0f, HitchAccumulator) == 0);
	assert(HitchClock.ScaleDelta(10.0f) == 0.0f);
	assert(!HitchClock.CanRetry());
	assert(!HitchClock.MarkRetryReady());

	assert(ShouldCheckpoint(0));
	assert(!ShouldCheckpoint(299));
	assert(ShouldCheckpoint(300));
	FWorldState NearCheckpoint = Initial;
	NearCheckpoint.Tick = 298;
	const FCandidateTick Reopen299 = FRules::BuildResumeCandidate(NearCheckpoint);
	const FCandidateTick Reopen300 = FRules::BuildResumeCandidate(Reopen299.State);
	assert(Reopen299.State.Tick == 299 && !ShouldCheckpoint(Reopen299.State.Tick));
	assert(Reopen300.State.Tick == 300 && ShouldCheckpoint(Reopen300.State.Tick));
	assert(Reopen300.State.LogicalSeconds == NearCheckpoint.LogicalSeconds);

	FWorldState Unsupported;
	assert(!DeserializeWorld("AIITY 999\n", Unsupported, Error));
	assert(!DeserializeWorld(Saved.substr(0, Saved.size() / 2), Unsupported, Error));

	std::cout << "AIity portable core checks passed\n";
	return 0;
}
