#pragma once

#include "CoreMinimal.h"
#include "Simulation/AIityWorldState.h"
#include "SQLiteDatabase.h"

// Prepared statements declared after this object finalize before Close.
struct FScopedSqliteDatabase
{
	FSQLiteDatabase Database;

	FScopedSqliteDatabase() = default;
	~FScopedSqliteDatabase()
	{
		if (Database.IsValid())
		{
			Database.Close();
		}
	}

	FScopedSqliteDatabase(const FScopedSqliteDatabase&) = delete;
	FScopedSqliteDatabase& operator=(const FScopedSqliteDatabase&) = delete;

	// Unreal's default SQLite VFS has no shared memory. WAL access needs this first.
	bool OpenExclusive(const TCHAR* Filename, ESQLiteDatabaseOpenMode Mode, FString& Error)
	{
		Error.Reset();
		if (Database.Open(Filename, Mode) &&
			Database.Execute(TEXT("PRAGMA locking_mode=EXCLUSIVE;")))
		{
			return true;
		}
		Error = Database.GetLastError();
		return false;
	}
};

class FAIityWorldStore
{
public:
	~FAIityWorldStore();

	bool Open(const FString& DatabasePath, FString& Error);
	bool LoadLatest(AIity::FWorldState& State, bool& bFound, FString& Error);
	bool Commit(const AIity::FCandidateTick& Candidate, FString& Error);
	bool CheckpointAndBackup(FString& Error);
	bool CheckWritable(FString& Error);
	void Close();
	void SetMinimumFreeBytesForTesting(uint64 Bytes) { MinimumFreeBytes = Bytes; }
	void SetFailCommitForTesting(bool bFail) { bFailCommitForTesting = bFail; }
	void SetFailBackupPublishForTesting(bool bFail) { bFailBackupPublishForTesting = bFail; }
	void SetFailBackupAfterPreviousForTesting(bool bFail) { bFailBackupAfterPreviousForTesting = bFail; }
	void SetFailRecoveryAfterMarkerForTesting(bool bFail) { bFailRecoveryAfterMarkerForTesting = bFail; }

#if WITH_DEV_AUTOMATION_TESTS
	enum class ECommitBarrierPhase
	{
		BeforeBegin,
		BeforeCommit,
		AfterCommit
	};

	void SetCommitBarrierForTesting(ECommitBarrierPhase Phase, const FString& MarkerPath,
		const FString& Nonce, double TimeoutSeconds);
#endif

private:
	enum class EInspection
	{
		Compatible,
		Unsupported,
		RecoverableCorrupt,
		Unknown
	};

	FSQLiteDatabase Database;
	FString Path;
	int32 WorldLockFd = -1;
	uint64 MinimumFreeBytes = 256ULL * 1024ULL * 1024ULL;
	bool bFailCommitForTesting = false;
	bool bFailBackupPublishForTesting = false;
	bool bFailBackupAfterPreviousForTesting = false;
	bool bFailRecoveryAfterMarkerForTesting = false;

#if WITH_DEV_AUTOMATION_TESTS
	ECommitBarrierPhase CommitBarrierPhase = ECommitBarrierPhase::BeforeBegin;
	FString CommitBarrierMarkerPath;
	FString CommitBarrierNonce;
	double CommitBarrierTimeoutSeconds = 0.0;

	bool WaitAtCommitBarrierForTesting(ECommitBarrierPhase Phase, FString& Error);
#endif

	bool Execute(const TCHAR* Sql, FString& Error);
	EInspection InspectFile(const FString& CandidatePath, FString& Error,
		bool bRequireSelfContained = false) const;
	bool RecoverFromBackup(FString& Error);
	bool FinishInterruptedRecovery(FString& Error);
	bool OpenWritable(bool bNewDatabase, FString& Error);
	bool AcquireWorldLock(FString& Error);
	bool HasSaveFamilyEvidence() const;
	bool HasDiskReserve(FString& Error) const;
	bool AtomicReplace(const FString& Source, const FString& Destination, FString& Error) const;
	bool DeleteExact(const FString& FilePath, FString& Error) const;
};
