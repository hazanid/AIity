#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Persistence/AIityWorldStore.h"
#include "Simulation/AIityRules.h"
#include "Simulation/AIitySerialization.h"
#include "SQLitePreparedStatement.h"

#if PLATFORM_MAC
#include <cerrno>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
constexpr double ParentMarkerTimeoutSeconds = 30.0;
constexpr double ChildBarrierTimeoutSeconds = 60.0;
constexpr double ChildExitTimeoutSeconds = 10.0;

struct FCrashPhase
{
	FAIityWorldStore::ECommitBarrierPhase StorePhase;
	const TCHAR* Name;
	bool bCandidateMustCommit;
};

const FCrashPhase CrashPhases[] = {
	{FAIityWorldStore::ECommitBarrierPhase::BeforeBegin, TEXT("before-begin"), false},
	{FAIityWorldStore::ECommitBarrierPhase::BeforeCommit, TEXT("before-commit"), false},
	{FAIityWorldStore::ECommitBarrierPhase::AfterCommit, TEXT("after-commit"), true}
};

bool IsHexToken(const FString& Token)
{
	if (Token.Len() != 32)
	{
		return false;
	}
	for (int32 Index = 0; Index < Token.Len(); ++Index)
	{
		if (!FChar::IsHexDigit(Token[Index]))
		{
			return false;
		}
	}
	return true;
}

const FCrashPhase* FindPhase(const FString& Name)
{
	for (const FCrashPhase& Phase : CrashPhases)
	{
		if (Name == Phase.Name)
		{
			return &Phase;
		}
	}
	return nullptr;
}

FString FixtureDirectory(const FString& FixtureToken)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests/ProcessCrash"), FixtureToken);
}

bool BuildArrivalCandidate(const AIity::FWorldState& Baseline,
	AIity::FCandidateTick& Candidate, FString& Error)
{
	if (Baseline.Founders.empty() || !Baseline.Founders[0].Action.AwaitingMovement ||
		(Baseline.Founders[0].Action.Kind != AIity::EActionKind::GatherFood &&
		 Baseline.Founders[0].Action.Kind != AIity::EActionKind::GatherWater))
	{
		Error = TEXT("Process-crash baseline has no active gathering movement.");
		return false;
	}
	const AIity::FFounder& Founder = Baseline.Founders[0];
	const AIity::FMovementReceipt Arrival{Founder.Action.Id, Founder.Id, Founder.Action.Id,
		Founder.Action.Epoch, AIity::EMovementOutcome::Arrived,
		Founder.Action.TargetX, Founder.Action.TargetY};
	Candidate = AIity::FRules::BuildCandidate(Baseline, {Arrival});
	if (Candidate.ConsumedReceiptIds.size() != 1)
	{
		Error = TEXT("Process-crash candidate did not consume exactly one movement receipt.");
		return false;
	}
	return true;
}

bool ReadIdsAtTick(const FString& DatabasePath, const TCHAR* Sql, uint64 Tick,
	TArray<int64>& Ids, FString& Error)
{
	if (Tick > static_cast<uint64>(MAX_int64))
	{
		Error = TEXT("Process-crash Tick exceeds SQLite signed storage range.");
		return false;
	}
	FScopedSqliteDatabase Session;
	if (!Session.OpenExclusive(*DatabasePath, ESQLiteDatabaseOpenMode::ReadOnly, Error))
	{
		return false;
	}
	FSQLitePreparedStatement Query = Session.Database.PrepareStatement(
		Sql, ESQLitePreparedStatementFlags::Persistent);
	if (!Query.IsValid() || !Query.SetBindingValueByIndex(1, static_cast<int64>(Tick)))
	{
		Error = Session.Database.GetLastError();
		return false;
	}
	for (;;)
	{
		const ESQLitePreparedStatementStepResult Result = Query.Step();
		if (Result == ESQLitePreparedStatementStepResult::Done)
		{
			return true;
		}
		int64 Id = 0;
		if (Result != ESQLitePreparedStatementStepResult::Row ||
			!Query.GetColumnValueByIndex(0, Id))
		{
			Error = Session.Database.GetLastError();
			return false;
		}
		Ids.Add(Id);
	}
}

bool WaitUntilOwnedChildStops(FProcHandle& Child, double TimeoutSeconds)
{
	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformProcess::IsProcRunning(Child) && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(0.01f);
	}
	return !FPlatformProcess::IsProcRunning(Child);
}

void CloseOwnedChild(FProcHandle& Child)
{
	if (Child.IsValid())
	{
		FPlatformProcess::WaitForProc(Child);
		FPlatformProcess::CloseProc(Child);
	}
}

// Timeout/failure cleanup only. Success here is not forced-termination evidence.
bool CleanupOwnedChild(FProcHandle& Child, uint32 ChildPid, FString& Error)
{
	if (!Child.IsValid())
	{
		return true;
	}
#if PLATFORM_MAC
	if (FPlatformProcess::IsProcRunning(Child))
	{
		const int KillResult = ::kill(static_cast<pid_t>(ChildPid), SIGKILL);
		if (KillResult != 0 && errno != ESRCH)
		{
			Error = FString::Printf(
				TEXT("Cleanup SIGKILL failed for owned child %u with errno %d."),
				ChildPid, errno);
		}
	}
	if (!WaitUntilOwnedChildStops(Child, ChildExitTimeoutSeconds) &&
		FPlatformProcess::IsProcRunning(Child))
	{
		::kill(static_cast<pid_t>(ChildPid), SIGKILL);
		WaitUntilOwnedChildStops(Child, 5.0);
	}
#else
	Error = TEXT("Process-crash gate requires macOS SIGKILL.");
	return false;
#endif
	if (FPlatformProcess::IsProcRunning(Child))
	{
		Error = FString::Printf(TEXT("Owned child %u remained running after cleanup."), ChildPid);
		return false;
	}
	CloseOwnedChild(Child);
	return true;
}

// Crash-phase proof: SIGKILL must be issued to the live CreateProc PID (kill returns 0).
// Unreal WaitForProc does not expose waitpid/WIFSIGNALED; native SIGKILL-death remains engine-unverified.
bool ForceKillOwnedLiveChild(FProcHandle& Child, uint32 ChildPid, FString& Error)
{
	if (!Child.IsValid() || ChildPid == 0)
	{
		Error = TEXT("No owned live child handle/PID for forced termination.");
		return false;
	}
#if !PLATFORM_MAC
	Error = TEXT("Process-crash gate requires macOS SIGKILL.");
	CleanupOwnedChild(Child, ChildPid, Error);
	return false;
#else
	if (!FPlatformProcess::IsProcRunning(Child))
	{
		Error = FString::Printf(
			TEXT("Owned child %u already exited; cleanup is not confirmed SIGKILL."),
			ChildPid);
		CloseOwnedChild(Child);
		return false;
	}
	if (::kill(static_cast<pid_t>(ChildPid), SIGKILL) != 0)
	{
		Error = FString::Printf(
			TEXT("SIGKILL was not issued to live owned child %u (errno %d)."),
			ChildPid, errno);
		FString CleanupError;
		CleanupOwnedChild(Child, ChildPid, CleanupError);
		if (!CleanupError.IsEmpty())
		{
			Error += TEXT(" ") + CleanupError;
		}
		return false;
	}
	if (!WaitUntilOwnedChildStops(Child, ChildExitTimeoutSeconds))
	{
		Error = FString::Printf(
			TEXT("Owned child %u did not exit after confirmed SIGKILL."), ChildPid);
		FString CleanupError;
		CleanupOwnedChild(Child, ChildPid, CleanupError);
		if (!CleanupError.IsEmpty())
		{
			Error += TEXT(" ") + CleanupError;
		}
		return false;
	}
	CloseOwnedChild(Child);
	return true;
#endif
}

bool RunChildBranch(FString& Error)
{
	FString FixtureToken;
	FString Nonce;
	FString PhaseName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("AIityCrashFixture="), FixtureToken) ||
		!FParse::Value(FCommandLine::Get(), TEXT("AIityCrashNonce="), Nonce) ||
		!FParse::Value(FCommandLine::Get(), TEXT("AIityCrashPhase="), PhaseName) ||
		!IsHexToken(FixtureToken) || !IsHexToken(Nonce))
	{
		Error = TEXT("Child process-crash arguments are missing or invalid.");
		return false;
	}
	const FCrashPhase* Phase = FindPhase(PhaseName);
	if (!Phase)
	{
		Error = TEXT("Child process-crash phase is invalid.");
		return false;
	}

	const FString Directory = FixtureDirectory(FixtureToken);
	const FString DatabasePath = FPaths::Combine(Directory, TEXT("World.sqlite"));
	const FString MarkerPath = FPaths::Combine(Directory, TEXT("commit.marker"));
	FAIityWorldStore Store;
	if (!Store.Open(DatabasePath, Error))
	{
		return false;
	}
	bool bFound = false;
	AIity::FWorldState Baseline;
	if (!Store.LoadLatest(Baseline, bFound, Error) || !bFound)
	{
		return false;
	}
	AIity::FCandidateTick Candidate;
	if (!BuildArrivalCandidate(Baseline, Candidate, Error))
	{
		return false;
	}
	Store.SetCommitBarrierForTesting(
		Phase->StorePhase, MarkerPath, Nonce, ChildBarrierTimeoutSeconds);
	return Store.Commit(Candidate, Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityProcessCrashTest,
	"AIity.Persistence.ProcessCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityProcessCrashTest::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("AIityProcessCrashChild")))
	{
		FString Error;
		const bool bResult = RunChildBranch(Error);
		if (!bResult)
		{
			AddError(Error);
		}
		return bResult;
	}

#if !PLATFORM_MAC
	AddWarning(TEXT("AIity.Persistence.ProcessCrash is a macOS-only SIGKILL gate."));
	return true;
#else
	for (const FCrashPhase& Phase : CrashPhases)
	{
		const FString FixtureToken = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Directory = FixtureDirectory(FixtureToken);
		const FString DatabasePath = FPaths::Combine(Directory, TEXT("World.sqlite"));
		const FString MarkerPath = FPaths::Combine(Directory, TEXT("commit.marker"));
		IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
		if (!TestFalse(TEXT("GUID-scoped fixture did not already exist"),
			Files.DirectoryExists(*Directory)))
		{
			return false;
		}
		if (!TestTrue(TEXT("GUID-scoped process-crash fixture directory is created"),
			Files.CreateDirectoryTree(*Directory)))
		{
			return false;
		}

		FString Error;
		FAIityWorldStore Store;
		if (!TestTrue(TEXT("Process-crash baseline store opens"), Store.Open(DatabasePath, Error)))
		{
			AddError(Error);
			return false;
		}
		AIity::FCandidateTick Baseline = AIity::FRules::BuildCandidate(
			AIity::FRules::CreateFoundationWorld(0xC2A5ULL), {});
		Baseline.State.Tick = 299;
		if (!TestTrue(TEXT("Process-crash baseline commits"), Store.Commit(Baseline, Error)))
		{
			AddError(Error);
			return false;
		}
		AIity::FCandidateTick Candidate;
		if (!TestTrue(TEXT("Arrival candidate is valid"),
			BuildArrivalCandidate(Baseline.State, Candidate, Error)))
		{
			AddError(Error);
			return false;
		}
		Store.Close();

		const FString Executable = FPlatformProcess::ExecutablePath();
		if (!TestTrue(TEXT("Parent is the command-line Unreal editor"),
			FPaths::GetCleanFilename(Executable).Contains(TEXT("UnrealEditor-Cmd"))))
		{
			return false;
		}
		const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		if (!TestTrue(TEXT("Child uses the current project file"),
			Project.EndsWith(TEXT(".uproject")) && Files.FileExists(*Project)))
		{
			return false;
		}
		const FString Arguments = FString::Printf(
			TEXT("\"%s\" -unattended -NullRHI -nosplash -AIityProcessCrashChild ")
			TEXT("-AIityCrashFixture=%s -AIityCrashNonce=%s -AIityCrashPhase=%s ")
			TEXT("-ExecCmds=\"Automation RunTests AIity.Persistence.ProcessCrash; Quit\""),
			*Project, *FixtureToken, *Nonce, Phase.Name);
		uint32 ChildPid = 0;
		FProcHandle Child = FPlatformProcess::CreateProc(*Executable, *Arguments,
			false, true, true, &ChildPid, 0, *FPaths::ProjectDir(), nullptr);
		if (!TestTrue(TEXT("Owned non-detached child starts"), Child.IsValid() && ChildPid != 0))
		{
			return false;
		}

		const FString ExpectedMarker = FString::Printf(TEXT("nonce=%s\nphase=%s\npid=%u\n"),
			*Nonce, Phase.Name, ChildPid);
		bool bMarkerMatched = false;
		const double MarkerDeadline = FPlatformTime::Seconds() + ParentMarkerTimeoutSeconds;
		while (FPlatformTime::Seconds() < MarkerDeadline)
		{
			FString Marker;
			if (FFileHelper::LoadFileToString(Marker, *MarkerPath))
			{
				bMarkerMatched = Marker == ExpectedMarker;
				break;
			}
			if (!FPlatformProcess::IsProcRunning(Child))
			{
				break;
			}
			FPlatformProcess::Sleep(0.01f);
		}

		if (!bMarkerMatched || !FPlatformProcess::IsProcRunning(Child))
		{
			FString CleanupError;
			if (!CleanupOwnedChild(Child, ChildPid, CleanupError) && !CleanupError.IsEmpty())
			{
				AddError(CleanupError);
			}
			AddError(FString::Printf(
				TEXT("Child marker/live-process validation failed for phase %s. Fixture kept: %s"),
				Phase.Name, *FixtureToken));
			return false;
		}
		if (!TestTrue(TEXT("SIGKILL was issued to the owned live child"),
			ForceKillOwnedLiveChild(Child, ChildPid, Error)))
		{
			AddError(Error);
			return false;
		}

		FAIityWorldStore Reopened;
		if (!TestTrue(TEXT("Killed child fixture reopens through WorldStore"),
			Reopened.Open(DatabasePath, Error)))
		{
			AddError(Error);
			return false;
		}
		bool bFound = false;
		AIity::FWorldState Recovered;
		if (!TestTrue(TEXT("Recovered state loads"),
			Reopened.LoadLatest(Recovered, bFound, Error)) || !TestTrue(TEXT("Recovered state exists"), bFound))
		{
			AddError(Error);
			return false;
		}
		const AIity::FWorldState& ExpectedState =
			Phase.bCandidateMustCommit ? Candidate.State : Baseline.State;
		const FString RecoveredText =
			UTF8_TO_TCHAR(AIity::SerializeWorld(Recovered).c_str());
		const FString ExpectedText =
			UTF8_TO_TCHAR(AIity::SerializeWorld(ExpectedState).c_str());
		TestEqual(TEXT("Recovered serialized WorldState is exact"),
			RecoveredText, ExpectedText);
		TestEqual(TEXT("Recovered Tick is exact"), Recovered.Tick, ExpectedState.Tick);
		TestEqual(TEXT("Recovered LogicalSeconds is exact"),
			Recovered.LogicalSeconds, ExpectedState.LogicalSeconds);
		Reopened.Close();

		TArray<int64> EventIds;
		TArray<int64> ReceiptIds;
		TArray<int64> CheckpointTicks;
		if (!TestTrue(TEXT("Candidate Event IDs can be queried after close"),
			ReadIdsAtTick(DatabasePath,
				TEXT("SELECT event_id FROM world_events WHERE tick=?1 ORDER BY event_id;"),
				Candidate.State.Tick, EventIds, Error)) ||
			!TestTrue(TEXT("Candidate receipt IDs can be queried after close"),
				ReadIdsAtTick(DatabasePath,
					TEXT("SELECT receipt_id FROM consumed_receipts WHERE tick=?1 ORDER BY receipt_id;"),
					Candidate.State.Tick, ReceiptIds, Error)) ||
			!TestTrue(TEXT("Candidate Checkpoint can be queried after close"),
				ReadIdsAtTick(DatabasePath,
					TEXT("SELECT tick FROM world_checkpoints WHERE tick=?1;"),
					Candidate.State.Tick, CheckpointTicks, Error)))
		{
			AddError(Error);
			return false;
		}
		const int32 ExpectedEventCount =
			Phase.bCandidateMustCommit ? static_cast<int32>(Candidate.Events.size()) : 0;
		const int32 ExpectedReceiptCount = Phase.bCandidateMustCommit ? 1 : 0;
		TestEqual(TEXT("Candidate Event count matches commit boundary"),
			EventIds.Num(), ExpectedEventCount);
		TestEqual(TEXT("Candidate receipt count matches commit boundary"),
			ReceiptIds.Num(), ExpectedReceiptCount);
		TestEqual(TEXT("Candidate Checkpoint count matches commit boundary"),
			CheckpointTicks.Num(), ExpectedReceiptCount);
		if (Phase.bCandidateMustCommit)
		{
			const int32 ComparableEvents = FMath::Min(
				EventIds.Num(), static_cast<int32>(Candidate.Events.size()));
			for (int32 Index = 0; Index < ComparableEvents; ++Index)
			{
				TestEqual(TEXT("Candidate Event identity is exact"), EventIds[Index],
					static_cast<int64>(Candidate.Events[Index].Id));
			}
			if (ReceiptIds.Num() == 1)
			{
				TestEqual(TEXT("Consumed receipt identity is exact"), ReceiptIds[0],
					static_cast<int64>(Candidate.ConsumedReceiptIds[0]));
			}
		}
	}
	return true;
#endif
}

#endif
