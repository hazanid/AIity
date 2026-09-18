#include "AIityWorldSubsystem.h"

#include "AIityGameMode.h"
#include "Misc/Paths.h"
#include "Persistence/AIityWorldStore.h"
#include "Simulation/AIityRules.h"

void UAIityWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bShuttingDown = false;
}

void UAIityWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Store = MakeUnique<FAIityWorldStore>();
	FString Error;
	const FString SavePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Worlds/Foundation.sqlite"));
	if (!Store->Open(SavePath, Error))
	{
		Status = TEXT("Save failed: ") + Error;
		Store.Reset();
		return;
	}

	bool bFound = false;
	AIity::FWorldState Loaded;
	if (!Store->LoadLatest(Loaded, bFound, Error))
	{
		Status = TEXT("Recovery failed: ") + Error;
		Store->Close();
		Store.Reset();
		return;
	}
	if (!bFound)
	{
		AIity::FCandidateTick Initial;
		Initial.State = AIity::FRules::CreateFoundationWorld(0xA117ULL);
		if (!CommitAndPublish(Initial))
		{
			return;
		}
		Clock.MarkReady();
		Status = TEXT("New world saved — paused");
	}
	else
	{
		const AIity::FCandidateTick Reopen = AIity::FRules::BuildResumeCandidate(Loaded);
		CommittedState = Loaded;
		if (!CommitAndPublish(Reopen))
		{
			return;
		}
		Clock.MarkReady();
		Status = TEXT("World recovered — paused");
	}
}

void UAIityWorldSubsystem::Deinitialize()
{
	bShuttingDown = true;
	if (Store)
	{
		SaveNow();
		Store->Close();
		Store.Reset();
	}
	Super::Deinitialize();
}

bool UAIityWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAIityWorldSubsystem::Tick(float DeltaTime)
{
	if (!Store)
	{
		return;
	}
	const int32 WholeTicks = Clock.AccumulateWholeTicks(DeltaTime, Accumulator);
	for (int32 TickIndex = 0; TickIndex < WholeTicks; ++TickIndex)
	{
		const std::vector<AIity::FMovementReceipt> Receipts = PendingReceipts.Take();
		const AIity::FCandidateTick Candidate = AIity::FRules::BuildCandidate(CommittedState, Receipts);
		if (!CommitAndPublish(Candidate))
		{
			return;
		}
		Status = TEXT("Running — saved");
	}
}

TStatId UAIityWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAIityWorldSubsystem, STATGROUP_Tickables);
}

void UAIityWorldSubsystem::SubmitMovementReceipt(const AIity::FMovementReceipt& Receipt)
{
	if (Clock.CanAdvance())
	{
		PendingReceipts.Add(Receipt);
	}
}

bool UAIityWorldSubsystem::TogglePause()
{
	if (Clock.TogglePause())
	{
		Status = Clock.IsPaused() ? TEXT("Paused") : TEXT("Running");
		return true;
	}
	if (!Clock.IsReady())
	{
		Status = TEXT("World initialization failed and cannot run");
	}
	else if (Clock.HasPermanentFailure())
	{
		Status = TEXT("Startup failed — restart after fixing the required world; simulation cannot resume");
	}
	else if (Clock.HasFailure())
	{
		Status = TEXT("Persistence failed — press R to retry; world remains paused");
	}
	else
	{
		Status = TEXT("World is not ready and cannot run");
	}
	return false;
}

bool UAIityWorldSubsystem::RetryPersistence()
{
	if (Clock.HasPermanentFailure())
	{
		Status = TEXT("Startup failure is non-resumable; restart after fixing the required world");
		return false;
	}
	if (!Store || !Clock.CanRetry())
	{
		return false;
	}
	FString Error;
	if (!Store->CheckWritable(Error))
	{
		Status = TEXT("Persistence retry failed: ") + Error;
		return false;
	}
	PendingReceipts.Clear();
	const AIity::FCandidateTick Reissue = AIity::FRules::BuildRetryCandidate(CommittedState);
	if (!CommitAndPublish(Reissue))
	{
		return false;
	}
	if (!Clock.MarkRetryReady())
	{
		EnterPersistenceFailure(TEXT("Persistence retry could not reopen the simulation clock."));
		return false;
	}
	Status = TEXT("Persistence retry ready — paused; press Space to resume");
	return true;
}

void UAIityWorldSubsystem::MarkStartupFailure(const FString& Error)
{
	PendingReceipts.Clear();
	Accumulator = 0.0f;
	Clock.MarkStartupFailed();
	Status = Error;
}

void UAIityWorldSubsystem::SetSpeed(int32 NewSpeed)
{
	Clock.SetSpeed(NewSpeed);
}

bool UAIityWorldSubsystem::CommitAndPublish(const AIity::FCandidateTick& Candidate)
{
	FString Error;
	if (!Store->Commit(Candidate, Error))
	{
		EnterPersistenceFailure(TEXT("Persistence paused: ") + Error);
		return false;
	}
	CommittedState = Candidate.State;
	return true;
}

bool UAIityWorldSubsystem::SaveNow()
{
	if (!Store)
	{
		return false;
	}
	FString Error;
	Status = TEXT("Saving");
	if (!Store->CheckpointAndBackup(Error))
	{
		EnterPersistenceFailure(TEXT("Save failed: ") + Error);
		return false;
	}
	Status = TEXT("Saved");
	return true;
}

void UAIityWorldSubsystem::EnterPersistenceFailure(const FString& Error)
{
	const bool bReconcileActors =
		!bShuttingDown && Clock.IsReady() && !Clock.HasFailure();
	PendingReceipts.Clear();
	Accumulator = 0.0f;
	Clock.MarkFailed();
	Status = Error;
	if (bReconcileActors && GetWorld() &&
		!AAIityGameMode::ReconcileFounderActors(*GetWorld(), CommittedState))
	{
		Clock.MarkStartupFailed();
		Status += TEXT(" Founder rollback placement failed; restart is required.");
		UE_LOG(LogTemp, Error, TEXT("%s"), *Status);
	}
}
