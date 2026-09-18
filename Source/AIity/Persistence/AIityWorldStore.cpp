#include "Persistence/AIityWorldStore.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "SQLitePreparedStatement.h"
#include "Simulation/AIityPersistencePolicy.h"
#include "Simulation/AIityRules.h"
#include "Simulation/AIitySerialization.h"

#if PLATFORM_MAC
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
constexpr int32 SupportedSchemaVersion = 2;
constexpr const TCHAR* InspectionFilename = TEXT("World.sqlite");

bool SyncParentDirectory(const FString& FilePath)
{
#if PLATFORM_MAC
	const FTCHARToUTF8 DirectoryUtf8(*FPaths::GetPath(FilePath));
	const int DirectoryFd = open(DirectoryUtf8.Get(), O_RDONLY);
	if (DirectoryFd < 0)
	{
		return false;
	}
	const bool bSynced = fsync(DirectoryFd) == 0;
	const bool bClosed = close(DirectoryFd) == 0;
	return bSynced && bClosed;
#else
	return true;
#endif
}

bool SyncFileAndParent(const FString& FilePath)
{
#if PLATFORM_MAC
	const FTCHARToUTF8 PathUtf8(*FilePath);
	const int Fd = open(PathUtf8.Get(), O_RDONLY);
	if (Fd < 0)
	{
		return false;
	}
	const bool bSynced = fsync(Fd) == 0;
	const bool bClosed = close(Fd) == 0;
	return bSynced && bClosed && SyncParentDirectory(FilePath);
#else
	return true;
#endif
}

bool WriteDurableTextFile(const FString& FilePath, const FString& Text)
{
#if PLATFORM_MAC
	const FTCHARToUTF8 PathUtf8(*FilePath);
	const FTCHARToUTF8 TextUtf8(*Text);
	const int Fd = open(PathUtf8.Get(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (Fd < 0)
	{
		return false;
	}
	const int32 Length = TextUtf8.Length();
	const bool bWritten = write(Fd, TextUtf8.Get(), Length) == static_cast<ssize_t>(Length);
	const bool bSynced = bWritten && fsync(Fd) == 0;
	const bool bClosed = close(Fd) == 0;
	return bSynced && bClosed && SyncParentDirectory(FilePath);
#else
	return FFileHelper::SaveStringToFile(Text, *FilePath);
#endif
}

bool CreatePrivateInspectionDirectory(const FString& Directory, FString& Error)
{
#if PLATFORM_MAC
	const FTCHARToUTF8 DirectoryUtf8(*Directory);
	if (mkdir(DirectoryUtf8.Get(), 0700) == 0)
	{
		return true;
	}
	Error = FString::Printf(TEXT("Private inspection directory creation failed with errno %d."), errno);
	return false;
#else
	Error = TEXT("Private inspection directories are not implemented for this platform.");
	return false;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
const TCHAR* CommitBarrierPhaseName(FAIityWorldStore::ECommitBarrierPhase Phase)
{
	switch (Phase)
	{
	case FAIityWorldStore::ECommitBarrierPhase::BeforeBegin: return TEXT("before-begin");
	case FAIityWorldStore::ECommitBarrierPhase::BeforeCommit: return TEXT("before-commit");
	case FAIityWorldStore::ECommitBarrierPhase::AfterCommit: return TEXT("after-commit");
	default: return TEXT("unknown");
	}
}

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
#endif
}

FAIityWorldStore::~FAIityWorldStore()
{
	Close();
}

bool FAIityWorldStore::Execute(const TCHAR* Sql, FString& Error)
{
	if (Database.Execute(Sql))
	{
		return true;
	}
	Error = Database.GetLastError();
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
void FAIityWorldStore::SetCommitBarrierForTesting(ECommitBarrierPhase Phase,
	const FString& MarkerPath, const FString& Nonce, double TimeoutSeconds)
{
	CommitBarrierPhase = Phase;
	CommitBarrierMarkerPath = MarkerPath;
	CommitBarrierNonce = Nonce;
	CommitBarrierTimeoutSeconds = TimeoutSeconds;
}

bool FAIityWorldStore::WaitAtCommitBarrierForTesting(ECommitBarrierPhase Phase, FString& Error)
{
	if (CommitBarrierMarkerPath.IsEmpty() || Phase != CommitBarrierPhase)
	{
		return true;
	}
	const FString MarkerDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::GetPath(CommitBarrierMarkerPath));
	const FString DatabaseDirectory = FPaths::ConvertRelativePathToFull(FPaths::GetPath(Path));
	const FString FixtureRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests/ProcessCrash")));
	if (!MarkerDirectory.Equals(DatabaseDirectory, ESearchCase::CaseSensitive) ||
		!FPaths::ConvertRelativePathToFull(FPaths::GetPath(DatabaseDirectory)).Equals(
			FixtureRoot, ESearchCase::CaseSensitive) ||
		!IsHexToken(FPaths::GetCleanFilename(DatabaseDirectory)) ||
		FPaths::GetCleanFilename(Path) != TEXT("World.sqlite") ||
		FPaths::GetCleanFilename(CommitBarrierMarkerPath) != TEXT("commit.marker") ||
		!IsHexToken(CommitBarrierNonce) ||
		CommitBarrierTimeoutSeconds <= 0.0 || CommitBarrierTimeoutSeconds > 120.0)
	{
		Error = TEXT("Test commit barrier configuration is invalid or outside the fixture.");
		return false;
	}

	const uint32 ProcessId = FPlatformProcess::GetCurrentProcessId();
	const FString MarkerText = FString::Printf(TEXT("nonce=%s\nphase=%s\npid=%u\n"),
		*CommitBarrierNonce, CommitBarrierPhaseName(Phase), ProcessId);
	const FString TemporaryMarker = CommitBarrierMarkerPath +
		FString::Printf(TEXT(".tmp.%u"), ProcessId);
	if (!DeleteExact(TemporaryMarker, Error) ||
		!WriteDurableTextFile(TemporaryMarker, MarkerText) ||
		!AtomicReplace(TemporaryMarker, CommitBarrierMarkerPath, Error))
	{
		if (Error.IsEmpty())
		{
			Error = TEXT("Test commit barrier marker could not be published durably.");
		}
		return false;
	}

	const double Deadline = FPlatformTime::Seconds() + CommitBarrierTimeoutSeconds;
	const FString ReleasePath = CommitBarrierMarkerPath + TEXT(".release");
	while (FPlatformTime::Seconds() < Deadline)
	{
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ReleasePath))
		{
			return true;
		}
		FPlatformProcess::Sleep(0.01f);
	}
	Error = TEXT("Test commit barrier timed out.");
	return false;
}
#endif

bool FAIityWorldStore::Open(const FString& DatabasePath, FString& Error)
{
	Error.Reset();
	Close();
	Path = DatabasePath;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(*FPaths::GetPath(Path)))
	{
		Error = TEXT("Could not create the save directory.");
		Path.Reset();
		return false;
	}
	if (!AcquireWorldLock(Error))
	{
		Path.Reset();
		return false;
	}
	if (PlatformFile.FileExists(*(Path + TEXT("-journal"))))
	{
		Error = TEXT("World rollback journal exists; source inspection and recovery are forbidden.");
		Close();
		return false;
	}

	if (PlatformFile.FileExists(*(Path + TEXT(".recovery"))))
	{
		if (!FinishInterruptedRecovery(Error))
		{
			Close();
			return false;
		}
	}

	const bool bPrimaryExists = PlatformFile.FileExists(*Path);
	if (!bPrimaryExists)
	{
		if (HasSaveFamilyEvidence())
		{
			Error = TEXT("World primary is missing but save-family evidence exists; no new world was created.");
			Close();
			return false;
		}
		return OpenWritable(true, Error);
	}

	const EInspection Inspection = InspectFile(Path, Error);
	if (Inspection == EInspection::Compatible)
	{
		return OpenWritable(false, Error);
	}
	if (Inspection != EInspection::RecoverableCorrupt)
	{
		Close();
		return false;
	}
	if (!RecoverFromBackup(Error))
	{
		Close();
		return false;
	}
	return OpenWritable(false, Error);
}

bool FAIityWorldStore::AcquireWorldLock(FString& Error)
{
#if PLATFORM_MAC
	const FString LockPath = Path + TEXT(".lock");
	const FTCHARToUTF8 LockPathUtf8(*LockPath);
	WorldLockFd = open(LockPathUtf8.Get(), O_RDWR | O_CREAT, 0600);
	if (WorldLockFd < 0 || flock(WorldLockFd, LOCK_EX | LOCK_NB) != 0)
	{
		if (WorldLockFd >= 0)
		{
			close(WorldLockFd);
			WorldLockFd = -1;
		}
		Error = TEXT("World is already owned by another process or its lock cannot be acquired.");
		return false;
	}
	return true;
#else
	Error = TEXT("Writable world locking is not implemented for this platform.");
	return false;
#endif
}

bool FAIityWorldStore::HasSaveFamilyEvidence() const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	for (const FString& Candidate : {
		Path + TEXT("-wal"), Path + TEXT("-shm"), Path + TEXT("-journal"), Path + TEXT(".backup"),
		Path + TEXT(".backup.previous"), Path + TEXT(".recovery"), Path + TEXT(".recovering")})
	{
		if (PlatformFile.FileExists(*Candidate))
		{
			return true;
		}
	}
	for (const FString& Pattern : {
		Path + TEXT(".damaged*"), Path + TEXT(".recovering*"),
		Path + TEXT(".backup.tmp*"), Path + TEXT(".backup.swap*"),
		Path + TEXT(".inspect.*")})
	{
		TArray<FString> Matches;
		IFileManager::Get().FindFiles(Matches, *Pattern, true, true);
		if (Matches.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

bool FAIityWorldStore::AtomicReplace(const FString& Source, const FString& Destination, FString& Error) const
{
#if PLATFORM_MAC
	const FTCHARToUTF8 SourceUtf8(*Source);
	const FTCHARToUTF8 DestinationUtf8(*Destination);
	if (rename(SourceUtf8.Get(), DestinationUtf8.Get()) == 0)
	{
		if (SyncParentDirectory(Destination))
		{
			return true;
		}
		Error = TEXT("Atomic replacement completed but its directory could not be synced.");
		return false;
	}
	Error = FString::Printf(TEXT("Atomic file replacement failed with errno %d."), errno);
	return false;
#else
	Error = TEXT("Atomic file replacement is not implemented for this platform.");
	return false;
#endif
}

bool FAIityWorldStore::DeleteExact(const FString& FilePath, FString& Error) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*FilePath))
	{
		return true;
	}
	if (PlatformFile.DeleteFile(*FilePath) && SyncParentDirectory(FilePath))
	{
		return true;
	}
	Error = TEXT("Could not remove a scoped world recovery file: ") + FilePath;
	return false;
}

bool FAIityWorldStore::HasDiskReserve(FString& Error) const
{
	uint64 TotalBytes = 0;
	uint64 FreeBytes = 0;
	const bool bHasStats = FPlatformMisc::GetDiskTotalAndFreeSpace(FPaths::GetPath(Path), TotalBytes, FreeBytes);
	if (!bHasStats)
	{
		Error = TEXT("World paused: free disk space could not be determined.");
		return false;
	}
	if (MinimumFreeBytes != MAX_uint64 && FreeBytes >= MinimumFreeBytes)
	{
		return true;
	}
	Error = FString::Printf(TEXT("World paused: free space %llu bytes is below reserve %llu bytes."),
		FreeBytes, MinimumFreeBytes);
	return false;
}

FAIityWorldStore::EInspection FAIityWorldStore::InspectFile(
	const FString& CandidatePath, FString& Error, bool bRequireSelfContained) const
{
	Error.Reset();
	if (WorldLockFd < 0)
	{
		Error = TEXT("World lock must be held before isolated inspection.");
		return EInspection::Unknown;
	}
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString SourceWal = CandidatePath + TEXT("-wal");
	const FString SourceJournal = CandidatePath + TEXT("-journal");
	if (PlatformFile.FileExists(*SourceJournal))
	{
		Error = TEXT("SQLite rollback journal is untrusted; source inspection is forbidden.");
		return EInspection::Unknown;
	}
	if (bRequireSelfContained && PlatformFile.FileExists(*SourceWal))
	{
		Error = TEXT("Primary-only publication input depends on a WAL file.");
		return EInspection::Unknown;
	}

	const int64 PrimaryBytes = PlatformFile.FileSize(*CandidatePath);
	const bool bHasWal = PlatformFile.FileExists(*SourceWal);
	const int64 WalBytes = bHasWal ? PlatformFile.FileSize(*SourceWal) : 0;
	if (PrimaryBytes < 0 || WalBytes < 0)
	{
		Error = TEXT("Inspection source size could not be read.");
		return EInspection::Unknown;
	}
	uint64 TotalBytes = 0;
	uint64 FreeBytes = 0;
	const uint64 CopyBytes = static_cast<uint64>(PrimaryBytes) + static_cast<uint64>(WalBytes);
	if (!FPlatformMisc::GetDiskTotalAndFreeSpace(
			FPaths::GetPath(CandidatePath), TotalBytes, FreeBytes) ||
		MinimumFreeBytes == MAX_uint64 || FreeBytes < MinimumFreeBytes ||
		CopyBytes > FreeBytes - MinimumFreeBytes)
	{
		Error = TEXT("Inspection scratch copy would violate the disk reserve.");
		return EInspection::Unknown;
	}

	const FString ScratchDirectory = CandidatePath + TEXT(".inspect.") +
		FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!CreatePrivateInspectionDirectory(ScratchDirectory, Error))
	{
		return EInspection::Unknown;
	}
	const FString ScratchPath = FPaths::Combine(ScratchDirectory, InspectionFilename);
	if (!PlatformFile.CopyFile(*ScratchPath, *CandidatePath) ||
		(bHasWal && !PlatformFile.CopyFile(*(ScratchPath + TEXT("-wal")), *SourceWal)))
	{
		Error = TEXT("Inspection scratch family copy failed; source was not opened. Scratch kept: ") +
			ScratchDirectory;
		return EInspection::Unknown;
	}

	FScopedSqliteDatabase Inspection;
	EInspection Result = EInspection::Unknown;
	if (!Inspection.OpenExclusive(*ScratchPath, ESQLiteDatabaseOpenMode::ReadOnly, Error))
	{
		Error = TEXT("World database identity is unreadable; automatic replacement is forbidden: ") +
			Error;
	}
	else
	{
		Result = [&]()
		{
			FSQLitePreparedStatement Schema = Inspection.Database.PrepareStatement(
				TEXT("SELECT value FROM world_meta WHERE key='schema_version';"),
				ESQLitePreparedStatementFlags::Persistent);
			int64 Version = 0;
			if (!Schema.IsValid() || Schema.Step() != ESQLitePreparedStatementStepResult::Row ||
				!Schema.GetColumnValueByIndex(0, Version))
			{
				Error = TEXT("Existing world identity is unknown or missing; the file was left unchanged.");
				return EInspection::Unknown;
			}
			if (Version != SupportedSchemaVersion)
			{
				Error = FString::Printf(
					TEXT("World schema %lld is unsupported; expected %d. The file was left unchanged."),
					Version, SupportedSchemaVersion);
				return EInspection::Unsupported;
			}

			FSQLitePreparedStatement Current = Inspection.Database.PrepareStatement(
				TEXT("SELECT tick,logical_seconds,state_text FROM current_state WHERE id=1;"),
				ESQLitePreparedStatementFlags::Persistent);
			int64 StoredTick = -1;
			int64 StoredLogicalSeconds = -1;
			FString Serialized;
			if (!Current.IsValid() || Current.Step() != ESQLitePreparedStatementStepResult::Row ||
				!Current.GetColumnValueByIndex(0, StoredTick) ||
				!Current.GetColumnValueByIndex(1, StoredLogicalSeconds) ||
				!Current.GetColumnValueByIndex(2, Serialized))
			{
				Error = TEXT("Supported world has no readable current WorldState.");
				return EInspection::RecoverableCorrupt;
			}
			AIity::FWorldState State;
			std::string ParseError;
			if (!AIity::DeserializeWorld(TCHAR_TO_UTF8(*Serialized), State, ParseError) ||
				StoredTick < 0 || StoredLogicalSeconds < 0 ||
				State.Tick != static_cast<uint64>(StoredTick) ||
				State.LogicalSeconds != static_cast<uint64>(StoredLogicalSeconds))
			{
				Error = TEXT("Stored WorldState is malformed or disagrees with Tick metadata.");
				return EInspection::RecoverableCorrupt;
			}

			FSQLitePreparedStatement Integrity = Inspection.Database.PrepareStatement(
				TEXT("PRAGMA quick_check;"), ESQLitePreparedStatementFlags::Persistent);
			FString IntegrityResult;
			if (!Integrity.IsValid() || Integrity.Step() != ESQLitePreparedStatementStepResult::Row ||
				!Integrity.GetColumnValueByIndex(0, IntegrityResult) || IntegrityResult != TEXT("ok"))
			{
				Error = TEXT("Supported world failed SQLite integrity validation.");
				return EInspection::RecoverableCorrupt;
			}
			return EInspection::Compatible;
		}();
	}

	if (Inspection.Database.IsValid() && !Inspection.Database.Close())
	{
		Error = TEXT("Inspection scratch database could not close; source was not opened. Scratch kept: ") +
			ScratchDirectory;
		return EInspection::Unknown;
	}
	for (const TCHAR* Suffix : {TEXT(""), TEXT("-wal"), TEXT("-shm"), TEXT("-journal")})
	{
		const FString ScratchFile = ScratchPath + Suffix;
		if (PlatformFile.FileExists(*ScratchFile) && !PlatformFile.DeleteFile(*ScratchFile))
		{
			Error = TEXT("Inspection scratch cleanup failed; source was not opened. Scratch kept: ") +
				ScratchDirectory;
			return EInspection::Unknown;
		}
	}
	if (!PlatformFile.DeleteDirectory(*ScratchDirectory))
	{
		Error = TEXT("Inspection scratch directory cleanup failed; source was not opened. Scratch kept: ") +
			ScratchDirectory;
		return EInspection::Unknown;
	}
	return Result;
}

bool FAIityWorldStore::RecoverFromBackup(FString& Error)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString Backup = Path + TEXT(".backup");
	const FString Previous = Backup + TEXT(".previous");
	FString RecoverySource;
	FString InspectionError;
	for (const FString& Candidate : {Backup, Previous})
	{
		if (PlatformFile.FileExists(*Candidate) &&
			InspectFile(Candidate, InspectionError, true) == EInspection::Compatible)
		{
			RecoverySource = Candidate;
			break;
		}
	}
	if (RecoverySource.IsEmpty())
	{
		Error = TEXT("World database is damaged and no validated backup is available. The original was preserved.");
		return false;
	}

	const FString Suffix = FString::Printf(TEXT(".%lld"), FDateTime::UtcNow().GetTicks());
	const FString Recovering = Path + TEXT(".recovering");
	if (PlatformFile.FileExists(*Recovering))
	{
		Error = TEXT("Unrecognized recovery staging exists; automatic replacement is forbidden.");
		return false;
	}
	if (!PlatformFile.CopyFile(*Recovering, *RecoverySource) ||
		!SyncFileAndParent(Recovering) ||
		InspectFile(Recovering, InspectionError, true) != EInspection::Compatible)
	{
		FString DeleteError;
		const bool bCleaned = DeleteExact(Recovering, DeleteError);
		Error = bCleaned
			? TEXT("Validated backup could not be staged; the damaged original was preserved.")
			: TEXT("Backup staging failed and cleanup also failed: ") + DeleteError;
		return false;
	}

	const FString Damaged = Path + TEXT(".damaged") + Suffix;
	if (!PlatformFile.CopyFile(*Damaged, *Path) || !SyncFileAndParent(Damaged))
	{
		FString DeleteError;
		const bool bCleaned = DeleteExact(Recovering, DeleteError);
		Error = bCleaned ? TEXT("Could not preserve the damaged world before recovery.")
			: TEXT("Could not preserve damaged world or clean staging: ") + DeleteError;
		return false;
	}
	for (const TCHAR* Sidecar : {TEXT("-wal"), TEXT("-shm")})
	{
		const FString Source = Path + Sidecar;
		if (PlatformFile.FileExists(*Source) &&
			(!PlatformFile.CopyFile(*(Damaged + Sidecar), *Source) ||
			 !SyncFileAndParent(Damaged + Sidecar)))
		{
			FString DeleteError;
			const bool bCleaned = DeleteExact(Recovering, DeleteError);
			Error = bCleaned ? TEXT("Could not preserve the complete damaged SQLite journal family.")
				: TEXT("Journal preservation and staging cleanup both failed: ") + DeleteError;
			return false;
		}
	}
	if (!WriteDurableTextFile(Path + TEXT(".recovery"), Damaged))
	{
		FString DeleteError;
		const bool bCleaned = DeleteExact(Recovering, DeleteError);
		Error = bCleaned ? TEXT("Could not write durable recovery intent; the primary was left unchanged.")
			: TEXT("Recovery intent and staging cleanup both failed: ") + DeleteError;
		return false;
	}
	if (bFailRecoveryAfterMarkerForTesting)
	{
		bFailRecoveryAfterMarkerForTesting = false;
		Error = TEXT("Injected interruption after durable recovery intent.");
		return false;
	}
	return FinishInterruptedRecovery(Error);
}

bool FAIityWorldStore::FinishInterruptedRecovery(FString& Error)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString Marker = Path + TEXT(".recovery");
	const FString Recovering = Path + TEXT(".recovering");
	FString Damaged;
	if (!FFileHelper::LoadFileToString(Damaged, *Marker) || Damaged.IsEmpty())
	{
		Error = TEXT("Recovery marker is unreadable; automatic replacement is forbidden.");
		return false;
	}
	if (!PlatformFile.FileExists(*Recovering))
	{
		if (PlatformFile.FileExists(*Path) && InspectFile(Path, Error, true) == EInspection::Compatible)
		{
			return DeleteExact(Marker, Error);
		}
		Error = TEXT("Recovery staging is missing and the primary is invalid; damaged family was preserved.");
		return false;
	}
	if (InspectFile(Recovering, Error, true) != EInspection::Compatible)
	{
		Error = TEXT("Recovery staging is not a valid AIity WorldState; damaged family was preserved.");
		return false;
	}
	if (!PlatformFile.FileExists(*Damaged))
	{
		Error = TEXT("Recovery marker does not name a preserved damaged primary.");
		return false;
	}
	for (const TCHAR* Sidecar : {TEXT("-wal"), TEXT("-shm")})
	{
		if (!DeleteExact(Path + Sidecar, Error))
		{
			return false;
		}
	}
	if (!AtomicReplace(Recovering, Path, Error))
	{
		return false;
	}
	if (InspectFile(Path, Error, true) != EInspection::Compatible)
	{
		Error = TEXT("Published recovery failed application-state validation; damaged family remains preserved.");
		return false;
	}
	return DeleteExact(Marker, Error);
}

bool FAIityWorldStore::OpenWritable(bool bNewDatabase, FString& Error)
{
	Error.Reset();
	if (!HasDiskReserve(Error))
	{
		Close();
		return false;
	}
	if (!Database.Open(*Path, bNewDatabase ? ESQLiteDatabaseOpenMode::ReadWriteCreate : ESQLiteDatabaseOpenMode::ReadWrite))
	{
		Error = Database.GetLastError();
		Close();
		return false;
	}
	if (!Execute(TEXT("PRAGMA locking_mode=EXCLUSIVE;"), Error) ||
		!Execute(TEXT("PRAGMA busy_timeout=0;"), Error) ||
		!Execute(TEXT("BEGIN EXCLUSIVE TRANSACTION;"), Error))
	{
		Close();
		Error = TEXT("World is already open by another writer or cannot be locked: ") + Error;
		return false;
	}

	if (bNewDatabase &&
		(!Execute(TEXT("CREATE TABLE world_meta (key TEXT PRIMARY KEY, value INTEGER NOT NULL);"), Error) ||
		 !Execute(TEXT("CREATE TABLE current_state (id INTEGER PRIMARY KEY CHECK(id=1), tick INTEGER NOT NULL, logical_seconds INTEGER NOT NULL, state_text TEXT NOT NULL);"), Error) ||
		 !Execute(TEXT("CREATE TABLE world_checkpoints (tick INTEGER PRIMARY KEY, logical_seconds INTEGER NOT NULL, state_text TEXT NOT NULL);"), Error) ||
		 !Execute(TEXT("CREATE TABLE world_events (event_id INTEGER PRIMARY KEY, tick INTEGER NOT NULL, agent_id INTEGER NOT NULL, type TEXT NOT NULL, message TEXT NOT NULL);"), Error) ||
		 !Execute(TEXT("CREATE INDEX world_events_tick_idx ON world_events(tick,event_id);"), Error) ||
		 !Execute(TEXT("CREATE TABLE consumed_receipts (receipt_id INTEGER PRIMARY KEY, tick INTEGER NOT NULL);"), Error) ||
		 !Execute(TEXT("INSERT INTO world_meta(key,value) VALUES('schema_version',2);"), Error)))
	{
		FString RollbackError;
		Execute(TEXT("ROLLBACK;"), RollbackError);
		Close();
		return false;
	}
	if (!Execute(TEXT("COMMIT;"), Error) ||
		!Execute(TEXT("PRAGMA journal_mode=WAL;"), Error) ||
		!Execute(TEXT("PRAGMA synchronous=FULL;"), Error))
	{
		Close();
		return false;
	}
	return true;
}

bool FAIityWorldStore::LoadLatest(AIity::FWorldState& State, bool& bFound, FString& Error)
{
	bFound = false;
	FSQLitePreparedStatement Query = Database.PrepareStatement(
		TEXT("SELECT state_text FROM current_state WHERE id=1;"),
		ESQLitePreparedStatementFlags::Persistent);
	if (!Query.IsValid())
	{
		Error = Database.GetLastError();
		return false;
	}
	const ESQLitePreparedStatementStepResult Result = Query.Step();
	if (Result == ESQLitePreparedStatementStepResult::Done)
	{
		return true;
	}
	FString Serialized;
	if (Result != ESQLitePreparedStatementStepResult::Row || !Query.GetColumnValueByIndex(0, Serialized))
	{
		Error = TEXT("Latest committed world state could not be read.");
		return false;
	}
	std::string ParseError;
	if (!AIity::DeserializeWorld(TCHAR_TO_UTF8(*Serialized), State, ParseError))
	{
		Error = UTF8_TO_TCHAR(ParseError.c_str());
		return false;
	}
	bFound = true;
	return true;
}

bool FAIityWorldStore::Commit(const AIity::FCandidateTick& Candidate, FString& Error)
{
	Error.Reset();
	std::string ValidationError;
	if (!AIity::FRules::ValidatePersistenceEnvelope(Candidate, ValidationError))
	{
		Error = UTF8_TO_TCHAR(ValidationError.c_str());
		return false;
	}
	if (!HasDiskReserve(Error))
	{
		return false;
	}
	FSQLitePreparedStatement CurrentTick = Database.PrepareStatement(
		TEXT("SELECT tick FROM current_state WHERE id=1;"), ESQLitePreparedStatementFlags::Persistent);
	if (!CurrentTick.IsValid())
	{
		Error = Database.GetLastError();
		return false;
	}
	int64 DurableTick = -1;
	const ESQLitePreparedStatementStepResult CurrentResult = CurrentTick.Step();
	if (CurrentResult == ESQLitePreparedStatementStepResult::Row)
	{
		if (!CurrentTick.GetColumnValueByIndex(0, DurableTick) ||
			Candidate.State.Tick <= static_cast<uint64>(DurableTick))
		{
			Error = TEXT("Candidate tick is not newer than the committed WorldState.");
			return false;
		}
	}
	else if (CurrentResult != ESQLitePreparedStatementStepResult::Done)
	{
		Error = Database.GetLastError();
		return false;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (!WaitAtCommitBarrierForTesting(ECommitBarrierPhase::BeforeBegin, Error))
	{
		return false;
	}
#endif
	if (!Execute(TEXT("BEGIN IMMEDIATE TRANSACTION;"), Error))
	{
		return false;
	}

	bool bCommitted = false;
	bool bBarrierSuccess = true;
	do
	{
		const FString StateText = UTF8_TO_TCHAR(AIity::SerializeWorld(Candidate.State).c_str());
		FSQLitePreparedStatement Tick = Database.PrepareStatement(
			TEXT("INSERT INTO current_state(id,tick,logical_seconds,state_text) VALUES(1,?1,?2,?3) "
				 "ON CONFLICT(id) DO UPDATE SET tick=excluded.tick,logical_seconds=excluded.logical_seconds,state_text=excluded.state_text;"),
			ESQLitePreparedStatementFlags::Persistent);
		if (!Tick.IsValid() ||
			!Tick.SetBindingValueByIndex(1, static_cast<int64>(Candidate.State.Tick)) ||
			!Tick.SetBindingValueByIndex(2, static_cast<int64>(Candidate.State.LogicalSeconds)) ||
			!Tick.SetBindingValueByIndex(3, StateText) ||
			Tick.Step() != ESQLitePreparedStatementStepResult::Done)
		{
			Error = Database.GetLastError();
			break;
		}

		if (bFailCommitForTesting)
		{
			bFailCommitForTesting = false;
			Error = TEXT("Injected failure after candidate state write.");
			break;
		}

		if (AIity::ShouldCheckpoint(Candidate.State.Tick))
		{
			FSQLitePreparedStatement Checkpoint = Database.PrepareStatement(
				TEXT("INSERT INTO world_checkpoints(tick,logical_seconds,state_text) VALUES(?1,?2,?3);"),
				ESQLitePreparedStatementFlags::Persistent);
			if (!Checkpoint.IsValid() ||
				!Checkpoint.SetBindingValueByIndex(1, static_cast<int64>(Candidate.State.Tick)) ||
				!Checkpoint.SetBindingValueByIndex(2, static_cast<int64>(Candidate.State.LogicalSeconds)) ||
				!Checkpoint.SetBindingValueByIndex(3, StateText) ||
				Checkpoint.Step() != ESQLitePreparedStatementStepResult::Done ||
				!Execute(*FString::Printf(TEXT("DELETE FROM world_checkpoints WHERE tick NOT IN "
					"(SELECT tick FROM world_checkpoints ORDER BY tick DESC LIMIT %llu);"),
					static_cast<unsigned long long>(AIity::RetainedCheckpointCount)), Error))
			{
				if (Error.IsEmpty())
				{
					Error = Database.GetLastError();
				}
				break;
			}
		}

		FSQLitePreparedStatement Event = Database.PrepareStatement(
			TEXT("INSERT INTO world_events(event_id,tick,agent_id,type,message) VALUES(?1,?2,?3,?4,?5);"),
			ESQLitePreparedStatementFlags::Persistent);
		for (const AIity::FEvent& Item : Candidate.Events)
		{
			Event.Reset();
			if (!Event.SetBindingValueByIndex(1, static_cast<int64>(Item.Id)) ||
				!Event.SetBindingValueByIndex(2, static_cast<int64>(Item.Tick)) ||
				!Event.SetBindingValueByIndex(3, static_cast<int64>(Item.AgentId)) ||
				!Event.SetBindingValueByIndex(4, UTF8_TO_TCHAR(Item.Type.c_str())) ||
				!Event.SetBindingValueByIndex(5, UTF8_TO_TCHAR(Item.Message.c_str())) ||
				Event.Step() != ESQLitePreparedStatementStepResult::Done)
			{
				Error = Database.GetLastError();
				break;
			}
		}
		if (!Error.IsEmpty())
		{
			break;
		}

		FSQLitePreparedStatement Receipt = Database.PrepareStatement(
			TEXT("INSERT INTO consumed_receipts(receipt_id,tick) VALUES(?1,?2);"),
			ESQLitePreparedStatementFlags::Persistent);
		for (uint64_t ReceiptId : Candidate.ConsumedReceiptIds)
		{
			Receipt.Reset();
			if (!Receipt.SetBindingValueByIndex(1, static_cast<int64>(ReceiptId)) ||
				!Receipt.SetBindingValueByIndex(2, static_cast<int64>(Candidate.State.Tick)) ||
				Receipt.Step() != ESQLitePreparedStatementStepResult::Done)
			{
				Error = Database.GetLastError();
				break;
			}
		}
		if (!Error.IsEmpty())
		{
			break;
		}
#if WITH_DEV_AUTOMATION_TESTS
		if (!WaitAtCommitBarrierForTesting(ECommitBarrierPhase::BeforeCommit, Error))
		{
			break;
		}
#endif
		if (!Execute(TEXT("COMMIT;"), Error))
		{
			break;
		}
		bCommitted = true;
#if WITH_DEV_AUTOMATION_TESTS
		if (!WaitAtCommitBarrierForTesting(ECommitBarrierPhase::AfterCommit, Error))
		{
			bBarrierSuccess = false;
			break;
		}
#endif
	} while (false);

	if (!bCommitted)
	{
		FString RollbackError;
		Execute(TEXT("ROLLBACK;"), RollbackError);
	}
	return bCommitted && bBarrierSuccess;
}

bool FAIityWorldStore::CheckpointAndBackup(FString& Error)
{
	Error.Reset();
	if (!HasDiskReserve(Error) || !Execute(TEXT("PRAGMA wal_checkpoint(TRUNCATE);"), Error))
	{
		return false;
	}
	const FString BackupPath = Path + TEXT(".backup");
	const FString PreviousBackupPath = BackupPath + TEXT(".previous");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	auto CanPublishPrimaryOnly = [&PlatformFile, &Error](const FString& Destination)
	{
		if (PlatformFile.FileExists(*(Destination + TEXT("-wal"))) ||
			PlatformFile.FileExists(*(Destination + TEXT("-journal"))))
		{
			Error = TEXT("Backup destination has WAL or rollback-journal evidence; publication is forbidden.");
			return false;
		}
		return true;
	};
	if (!CanPublishPrimaryOnly(BackupPath) || !CanPublishPrimaryOnly(PreviousBackupPath))
	{
		return false;
	}
	const FString Suffix = FString::Printf(TEXT(".%lld"), FDateTime::UtcNow().GetTicks());
	const FString TemporaryBackupPath = BackupPath + TEXT(".tmp") + Suffix;
	const FString TemporaryPreviousPath = PreviousBackupPath + TEXT(".tmp") + Suffix;
	const FString SafeTemporaryPath = TemporaryBackupPath.Replace(TEXT("'"), TEXT("''"));
	if (!Execute(*FString::Printf(TEXT("VACUUM INTO '%s';"), *SafeTemporaryPath), Error))
	{
		FString CleanupError;
		if (!DeleteExact(TemporaryBackupPath, CleanupError))
		{
			Error += TEXT(" Backup staging cleanup failed: ") + CleanupError;
		}
		return false;
	}
	FString InspectionError;
	if (!SyncFileAndParent(TemporaryBackupPath) ||
		InspectFile(TemporaryBackupPath, InspectionError, true) != EInspection::Compatible)
	{
		Error = TEXT("New backup failed application-state validation; existing backups were preserved.");
		FString CleanupError;
		if (!DeleteExact(TemporaryBackupPath, CleanupError))
		{
			Error += TEXT(" Cleanup failed: ") + CleanupError;
		}
		return false;
	}
	if (bFailBackupPublishForTesting)
	{
		bFailBackupPublishForTesting = false;
		Error = TEXT("Injected backup publication failure; existing backups were preserved.");
		FString CleanupError;
		if (!DeleteExact(TemporaryBackupPath, CleanupError))
		{
			Error += TEXT(" Cleanup failed: ") + CleanupError;
		}
		return false;
	}

	if (PlatformFile.FileExists(*BackupPath) &&
		InspectFile(BackupPath, InspectionError, true) == EInspection::Compatible)
	{
		if (!PlatformFile.CopyFile(*TemporaryPreviousPath, *BackupPath) ||
			!SyncFileAndParent(TemporaryPreviousPath) ||
			InspectFile(TemporaryPreviousPath, InspectionError, true) != EInspection::Compatible ||
			!CanPublishPrimaryOnly(PreviousBackupPath) ||
			!AtomicReplace(TemporaryPreviousPath, PreviousBackupPath, Error))
		{
			FString CleanupError;
			if (Error.IsEmpty())
			{
				Error = TEXT("Could not publish a recognized previous validated backup.");
			}
			const bool bPreviousCleaned = DeleteExact(TemporaryPreviousPath, CleanupError);
			const bool bBackupCleaned = DeleteExact(TemporaryBackupPath, CleanupError);
			if (!bPreviousCleaned || !bBackupCleaned)
			{
				Error += TEXT(" Cleanup failed: ") + CleanupError;
			}
			return false;
		}
	}
	if (bFailBackupAfterPreviousForTesting)
	{
		bFailBackupAfterPreviousForTesting = false;
		Error = TEXT("Injected interruption after previous backup publication; current backup remains recognized.");
		FString CleanupError;
		if (!DeleteExact(TemporaryBackupPath, CleanupError))
		{
			Error += TEXT(" Cleanup failed: ") + CleanupError;
		}
		return false;
	}
	if (!CanPublishPrimaryOnly(BackupPath))
	{
		FString CleanupError;
		if (!DeleteExact(TemporaryBackupPath, CleanupError))
		{
			Error += TEXT(" Temporary backup cleanup failed: ") + CleanupError;
		}
		return false;
	}
	if (!AtomicReplace(TemporaryBackupPath, BackupPath, Error))
	{
		FString CleanupError;
		if (!DeleteExact(TemporaryBackupPath, CleanupError))
		{
			Error += TEXT(" Cleanup failed: ") + CleanupError;
		}
		return false;
	}
	if (InspectFile(BackupPath, InspectionError, true) != EInspection::Compatible)
	{
		Error = TEXT("Published backup failed application-state validation; previous backup remains recognized.");
		return false;
	}
	return true;
}

bool FAIityWorldStore::CheckWritable(FString& Error)
{
	Error.Reset();
	if (!HasDiskReserve(Error) || !Execute(TEXT("BEGIN IMMEDIATE TRANSACTION;"), Error))
	{
		return false;
	}
	return Execute(TEXT("ROLLBACK;"), Error);
}

void FAIityWorldStore::Close()
{
	// Prepared statements are function-local and must already be destroyed.
	if (Database.IsValid())
	{
		Database.Close();
	}
#if PLATFORM_MAC
	if (WorldLockFd >= 0)
	{
		flock(WorldLockFd, LOCK_UN);
		close(WorldLockFd);
		WorldLockFd = -1;
	}
#endif
	Path.Reset();
}
