#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/AIityBridgeProtocol.h"
#include "Simulation/AIityRules.h"
#include "Simulation/AIitySimulationClock.h"

namespace
{
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityBridgeRetryEpochTest,
	"AIity.Bridge.PersistenceRetryEpoch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityBridgeRetryEpochTest::RunTest(const FString& Parameters)
{
	const AIity::FCandidateTick Active = AIity::FRules::BuildCandidate(
		AIity::FRules::CreateFoundationWorld(411), {});
	const AIity::FFounder& Founder = Active.State.Founders[0];
	const AIity::FMovementReceipt BeforeFailure{Founder.Action.Id, Founder.Id,
		Founder.Action.Id, Founder.Action.Epoch, AIity::EMovementOutcome::Arrived,
		Founder.Action.TargetX, Founder.Action.TargetY};

	AIity::FBridgeReceiptQueue Queue;
	Queue.Add(BeforeFailure);
	Queue.Clear();
	TestTrue(TEXT("Failure clears queued bridge receipts"), Queue.IsEmpty());

	const AIity::FCandidateTick Retry = AIity::FRules::BuildRetryCandidate(Active.State);
	TestEqual(TEXT("Retry advances execution epoch"), Retry.State.ExecutionEpoch,
		Active.State.ExecutionEpoch + 1);
	TestEqual(TEXT("Retry preserves LogicalSeconds"), Retry.State.LogicalSeconds,
		Active.State.LogicalSeconds);
	const AIity::FCandidateTick Stale = AIity::FRules::BuildCandidate(
		Retry.State, {BeforeFailure});
	TestTrue(TEXT("Pre-failure receipt is stale after retry"), Stale.ConsumedReceiptIds.empty());

	AIity::FMovementReceipt Fresh = BeforeFailure;
	Fresh.Epoch = Retry.State.ExecutionEpoch;
	const AIity::FCandidateTick Accepted = AIity::FRules::BuildCandidate(Retry.State, {Fresh});
	TestEqual(TEXT("Fresh retry receipt is accepted"),
		static_cast<int32>(Accepted.ConsumedReceiptIds.size()), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityBridgeFreezeProtocolTest,
	"AIity.Bridge.QueuedInputFreeze",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityBridgeFreezeProtocolTest::RunTest(const FString& Parameters)
{
	FFakePawn Pawn;
	FFakeMovement Movement;
	AIity::FreezeQueuedMovement(Pawn, Movement);
	TestTrue(TEXT("Freeze consumes queued pawn input"), Pawn.bConsumed);
	TestTrue(TEXT("Freeze stops current movement"), Movement.bStopped);
	TestTrue(TEXT("Freeze clears accumulated movement forces"), Movement.bForcesCleared);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityBridgeHitchBudgetTest,
	"AIity.Bridge.LargeFrameBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityBridgeHitchBudgetTest::RunTest(const FString& Parameters)
{
	AIity::FSimulationClock Clock;
	Clock.MarkReady();
	TestTrue(TEXT("Ready clock resumes"), Clock.TogglePause());
	TestTrue(TEXT("Four-times speed is accepted"), Clock.SetSpeed(4));
	float Accumulator = 0.0f;
	const int32 WholeTicks = Clock.AccumulateWholeTicks(10.0f, Accumulator);
	TestEqual(TEXT("Large frame is capped and drained as fixed ticks"), WholeTicks, 4);
	TestEqual(TEXT("Timeout receives capped simulation seconds"), Clock.ScaleDelta(10.0f), 4.0f);
	TestEqual(TEXT("Physical speed scale receives the same budget"),
		Clock.MovementSpeedScale(10.0f), 0.4f);

	AIity::FWorldState State = AIity::FRules::CreateFoundationWorld(412);
	const uint64 StartSeconds = State.LogicalSeconds;
	for (int32 Tick = 0; Tick < WholeTicks; ++Tick)
	{
		State = AIity::FRules::BuildCandidate(State, {}).State;
	}
	TestEqual(TEXT("Rules drain exactly the shared whole-tick budget"),
		State.LogicalSeconds, StartSeconds + 4);

	Clock.MarkStartupFailed();
	TestEqual(TEXT("Permanent startup failure advances no ticks"),
		Clock.AccumulateWholeTicks(10.0f, Accumulator), 0);
	TestEqual(TEXT("Permanent startup failure advances no physical time"),
		Clock.ScaleDelta(10.0f), 0.0f);
	TestFalse(TEXT("Writable retry cannot clear startup failure"), Clock.MarkRetryReady());
	return true;
}

#endif
