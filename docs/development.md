# Development

## Current verification

The lead foundation pass supersedes the older setup/evidence notes below. The primary
checkout built successfully; `run.ATEqpo` passed all 13 targeted tests and
`foundation-arrival-20260918` passed the added out-of-range arrival assertions.
Real isolated clustered and active-action reopen preserved logical time and committed
state, with exactly one resume transition. See [status](status.md) for exact evidence.

For safe graphical diagnosis, close the world, preserve the complete present SQLite
family and backups, and copy it to a new isolated directory. Launch the game with
`-game -windowed -ResX=1280 -ResY=720 -userdir=<isolated-directory>`; Unreal uses its
`Saved/Worlds/` subdirectory. Inspect only closed copies. Never open the source production
database through a diagnostic SQLite connection. Keep unattended crash uploads disabled.
Ordinary and F9-then-close standalone checks passed; the historical render-thread
viewport assertion matches upstream [UE-382156](https://issues.unrealengine.com/issue/UE-382156)
and remains an engine-version retest obligation. Do not patch the installed engine.

Keyboard selection/follow, pause and speed are verified. Camera framing matched on
repeated 720p/1080p launches; use `-ForceRes` when the display clamps requested resolution.
Physical mouse acceptance, HUD readability and rendering remain follow-up. Bounded issue work may
proceed in isolated branches, with serialized native/graphical runs and lead integration.
Packaging, accessibility and performance still gate release.

## Glossary

- **portable check:** A normal C++ compiler check for the logical core only.
- **Automation gate:** Targeted Unreal tests run by the command-line editor.
- **world database:** The local SQLite save under Unreal's `Saved/Worlds/`.
- **validated backup:** A SQLite backup created and checked through SQLite.
- **report parent:** A safe directory under which one unique test report is created.
- **world lock:** The stable sibling file whose Mac advisory lock gives one process
  ownership from inspection through close.
- **recovery marker:** A durable file that tells the next open to finish a staged recovery.
- **shared clock:** The source helper that scales logical and founder movement time together.
- **frame budget:** Capped simulation time shared by fixed ticks, movement, and timeout.
- **reissue transition:** A committed new execution epoch that makes old movement reports stale.
- **presentation helper:** Portable founder-selection and HUD-size logic used by Unreal UI.
- **process-kill gate:** Test-only proof of SQLite state after killing one owned child process.
- **commit barrier:** Test-only marker and bounded wait at one exact commit phase.
- **VFS:** SQLite's engine-specific file layer.
- **M2 contract prototype:** Portable validation and publication helpers under
  `prototypes/m1/`. Not Unreal, Ollama, or JSON decoding.
- **development baseline:** A buildable technical foundation for issue branches; not a
  playable release.
- **issue worker:** One owner for one issue, isolated worktree, branch, and pull request.
- **durable XY:** A founder's last committed spatial observation in `X` and `Y`; not an
  exact live actor pose.
- **presentation placement:** A collision-safe actor location derived from durable XY
  without changing WorldState.

## Current prerequisites

Xcode 26.3 build 17C529, first launch, Apple Metal 32023.864, and SDK 26.2 are verified
from an owner-managed, nonstandard installation by using `DEVELOPER_DIR` for each
command. Do not record or assume the machine-specific Xcode path.

Unreal Engine 5.8.2 compiled `AIityEditor` Mac Development arm64 in lifecycle build
55054, which passed in 13.84 seconds. Targeted report `run.ARybOf` completed all 11
`AIity.*` tests with zero failed, not run, or in process. An isolated fresh world passed
at paused Tick 0. One reopen passed at Tick 1 with exactly one epoch/Event and unchanged
founders, resources, and LogicalSeconds. Three ordinary closes were clean, including one
after F9. The current founder-placement and gather-arrival changes need a new build and
test run. No clustered-restore, package, accessible-UI, or playable gate has passed.
Ollama is not needed for the foundation or for the portable M2 contract prototype.
Install it only for approved M2 adapter work after engine setup.

Official references:

- [Unreal Engine macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)
- [Unreal Engine release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes)
- [SQLiteCore API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/SQLiteCore)

## First engine setup

Verified editor compile: Unreal Engine 5.8.2, Xcode 26.3, SDK 26.2, `AIityEditor`
Mac Development arm64. All 11 targeted tests passed before the current
founder-placement change.

1. Use the verified Xcode 26.3 installation through a per-command `DEVELOPER_DIR`.
2. Build current source, then rerun all `AIity.*` tests and validate clustered restore.
3. `AIity.uproject` associates engine `5.8`. `scripts/check-world.sh` autodetects the
   verified install unless `UE_ROOT` is set.
4. Open `AIity.uproject` with that installed engine.
5. Record later play/recovery evidence in [status.md](status.md) only after it exists.

`AIity.Build.cs` intentionally leaves `CppStandard` unset so UE 5.8 supplies its
supported default. Both targets use `BuildSettingsVersion.V7`. Do not add an
override build environment or suppress the engine warning checks. Portable core,
presentation, and M2 contract checks remain independent C++17 checks.

`Config/DefaultEngine.ini` deliberately starts `/Engine/Maps/Entry` with
`AAIityGameMode`. The game mode builds the starter valley from hard engine-mesh
references. `DefaultGame.ini` also marks `/Engine/BasicShapes` for cooking. Missing
required geometry produces a visible startup failure instead of a silent partial world.
No source-controlled binary map exists yet.

`AndroidFileServer` is explicitly disabled in `AIity.uproject`. Android networking is
outside foundation scope, and no generated runtime settings belong in project config.

## Portable pull-request checks

[Portable checks](../.github/workflows/portable-checks.yml) runs on each pull request,
checking out its exact head commit on a GitHub-hosted Ubuntu runner. It compiles and
runs the three C++17 checks below with warnings as errors, checks the Automation
wrapper's Bash syntax, and checks patch whitespace against the PR merge base.
Any failed command fails the job. Updated PRs cancel obsolete runs.

The workflow uses a read-only contents token, an immutable official checkout action
pin, and does not retain checkout credentials. It needs no repository secrets,
engine installation, model weights, or personal paths. Branch protections are unchanged.

These checks cover portable logic and M2 prototypes only. Unreal compilation,
SQLiteCore adapters, Automation, graphics, and packaged performance still require
their separate local gates. Serialize all Unreal builds, Automation, and graphical
sessions on the shared Mac; passing this workflow is not playable-release evidence.

## Checks

The portable check has been exercised successfully:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror \
  scripts/core-check.cpp \
  Source/AIity/Simulation/AIityRules.cpp \
  Source/AIity/Simulation/AIitySerialization.cpp \
  -o /tmp/aiity-core-check &&
/tmp/aiity-core-check
```

It checks deterministic founders/rules, finite resource conservation, movement receipt
handling, starvation cost, serialization, resume reissue, and bounded depleted-resource
history. It also checks old non-awaiting gathers on resume/retry and uint64 values that
must be rejected before signed SQLite storage, while Seed and PRNG remain full-range
text. It does not compile Unreal code or run SQLiteCore.

The portable presentation-helper check has also been exercised successfully:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror \
  scripts/presentation-check.cpp \
  -o /tmp/aiity-presentation-check &&
/tmp/aiity-presentation-check
```

It checks previous/next founder wrap and HUD layout bounds at 1280×720 and 1920×1080.
It does not prove controller view rotation, real font wrapping, rendering, or cooking.

Portable M2 contract check:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror \
  scripts/m1-contract-check.cpp \
  prototypes/m1/AIityM1Contracts.cpp \
  -o /tmp/aiity-m1-contract-check &&
/tmp/aiity-m1-contract-check
```

It checks final current-epoch/permission admission, composite response identity,
atomic ledger and per-agent capacity refusal, metadata-aware bounded context,
private/group audience isolation, owner transcript deduplication, safe derived
memory, inert GodRequests, and survival hand-back markers. It is not JSON parsing,
Ollama, SQLite dialogue, survival routing, or God-reply proof. See
[m1-contracts.md](m1-contracts.md).

After engine setup, run the targeted Automation gate:

```sh
UE_ROOT="/path/to/installed/UE_5.x" scripts/check-world.sh
```

Optional:

```sh
AIITY_TEST_TIMEOUT_SECONDS=1200 \
AIITY_TEST_REPORT_PARENT="$PWD/TestReports/AIity" \
UE_ROOT="/path/to/installed/UE_5.x" \
scripts/check-world.sh
```

The script must run only `AIity.*`, use a unique report directory, and fail on timeout,
engine/process failure, missing report, zero tests, incomplete `inProcess`, or
failed/unrun tests. Report `run.ARybOf` passed all 11 tests after the lifecycle change.
Later foundation and presentation reruns passed; see current status for exact reports. The script
never clears the report parent; each run uses a new `run.*` child and preserves existing
files. Engine output
goes to a regular `engine-output.log` in that child. The script prints its path and at
most the final 200 lines or 64 KiB. A descendant holding standard output open can no
longer keep a captured pipe waiting for end-of-file. Autodetection uses the verified
Unreal 5.8 install; `UE_ROOT` still overrides.

Only regular JSON report files pass the wrapper's `lstat` check; symlinks and special
files are ignored. Output-tail reads are capped at 64 KiB. Reports are decoded with
`utf-8-sig`, which accepts the BOM produced by the native Automation report. A
recognized aggregate must include `inProcess`. That count is added to discovered total
and to failed/unrun. A missing field is rejected as incomplete.

The owner approved disabling unattended crash reports only. The lead applied that
setting to the shared Unreal 5.8 editor while the engine was stopped. Analytics remain
unchanged. No privacy flag or host path is stored in this repository.

Do not run full CI locally. Interactive and packaged gates are separate from Automation.

## Baseline issue workflow

The owner's latest main-first direction landed the reviewed, buildable baseline in
[PR #12](https://github.com/hazanid/AIity/pull/12). Native build and all 13 targeted tests
passed. Remaining graphical restore/shutdown verification precedes parallel feature work
and release, rather than blocking this initial integration.

The lead owns `main`, may commit scoped and verified work directly, and reviews,
approves and merges subagent PRs. Subagents must use isolated issue branches/worktrees;
they may not push to `main` or merge. Required checks and blocking reviews still apply.
See [integration authority](../AGENTS.md#integration-authority).

Assign one issue worker to one isolated worktree, branch, and pull request. Ownership
must not overlap. Review and merge are serialized before dependent work starts.
[Issue #3](https://github.com/hazanid/AIity/issues/3) passed its lifecycle gates.
Foundation #5/#7/#8/#11 passed their bounded acceptance and are closed; the upstream
shutdown race is not claimed fixed. HUD #4 and graphics #6 are integrated and verified.
M2 dialogue #1 remains pending; manager movement #2 belongs to M3. See the roadmap.

Portable work may run independently in isolated worktrees. On this shared Mac, serialize
Unreal builds, Automation, and graphical sessions.
Portable pull-request checks #9 are integrated. Camera launch #10 has tested source
improvements; physical-pointer acceptance remains open. The baseline is not a release.

`AIity.Persistence.ProcessCrash` passed all three macOS process-kill phases.
Its parent creates a fresh GUID directory under `Saved/Tests/ProcessCrash`, commits a
baseline, and starts the current command-line editor as a non-detached child with a
child-only flag, phase, fixture token, and nonce. The child can only rebuild that
validated fixture path and cannot launch another child.

At before-BEGIN, before-COMMIT, or after-COMMIT, test-only WorldStore code atomically
writes and syncs `commit.marker` with nonce, phase, and child PID, then waits no more than
60 seconds. The parent waits no more than 30 seconds, requires marker PID to equal the PID
returned by `CreateProc`, and confirms the owned handle is still live.

Crash-phase success requires `kill(SIGKILL)` to return 0 against that live owned PID.
That is POSIX send confirmation, not `waitpid`/`WIFSIGNALED` proof. Unreal
`WaitForProc` does not expose native signal-exit status here, so SIGKILL-death remains
engine-unverified. A child that already exited, or timeout/failure cleanup that later
stops the owned handle, is not forced-termination evidence. Cleanup still SIGKILLs only
the owned `CreateProc` PID and waits a bounded time so descendants are not left running.

The child launcher keeps Epic's UE 5.8 `Automation RunTests ...; Quit` form. Small
fixture directories are retained for diagnosis. No production database path is accepted
and no recursive delete is used.

The candidate is Tick 300 so it also exercises Checkpoint rollback/publication. The
parent reopens with WorldStore. Before-BEGIN and before-COMMIT must match the exact
baseline with no candidate Event, receipt, or Checkpoint. After-COMMIT must match the
exact candidate, Event IDs, one receipt, and one Checkpoint. This is abrupt owned-process
proof, not complete power-loss, filesystem, or hardware-failure proof. The latest
earlier run met these assertions; native `waitpid` signal status remains unverified.

## Launch and controls

After `AIityEditor` builds, use the isolated standalone workflow above for diagnosis.
Foundation and camera graphical evidence is recorded in status; this is not a packaged
playability or performance gate.

- `Space`: pause/resume
- `R`: retry the writable storage probe after a persistence failure
- `1`, `2`, `4`: speed
- Left click: select
- `[` / `]`: previous / next founder
- `F`: follow selection
- `WASD`: observer movement; `Q` / `E`: descend / ascend
- Hold right mouse button and move the mouse: look around

The world must open paused at Tick 0 when fresh, then commit exactly one resume transition
when reopening an existing save. `Initialize` does superclass work only. Durable
open/create/load/resume now begins in `OnWorldBeginPlay`, before `GameMode::StartPlay`.
Shutdown saves and closes only when a Store exists. Native build, targeted Automation,
fresh Tick 0, exact one-reopen, and three ordinary-close checks passed.

Current source gives spawned founders an AI controller and uses one clock for logical
ticks, physical founder movement, and movement timeout.
Pause, initialization failure, and persistence failure return zero frame budget and
reject movement receipts. Founder owner ticks run before CharacterMovement without a
prerequisite cycle. The freeze path consumes queued pawn input, stops velocity, and clears
accumulated forces before CharacterMovement runs.

A failed commit clears pending bridge receipts, resets partial frame budget, keeps the
committed WorldState public, and resyncs founder actors to it. `R` checks disk reserve and
a real immediate SQLite transaction, then commits a reissue transition before clearing
the failure. Success remains paused until `Space`. Startup geometry or founder-spawn
failure is permanent for that run and `R` cannot clear it.

Each frame supplies at most four simulation seconds. The subsystem drains every whole
fixed tick in that frame, one committed candidate at a time. Founder timeout uses the
same capped seconds. CharacterMovement walk speed is adjusted so its real-frame movement
has the same distance budget, including a large hitch. Automation passed these bridge
contracts, and one live run observed basic pause, 4× speed, selection, follow, and
movement. The later clustered-restore pass succeeded; broader playability remains unfinished. See
[status.md](status.md).

## Founder placement and durable XY

Durable XY is the last committed spatial observation. It is not an exact live actor
pose. Runtime presentation tries durable XY first, then 24 fixed nearby candidates in
stable founder-ID order. Every spawn uses `DontSpawnIfColliding`; Pawn and world
blocking collision remain enabled. Each nonzero offset is logged. Candidate coordinates
are widened before addition and must fit Unreal's existing safe actor-placement bound.
Unsupported coordinates fail before existing actors are changed; they are not clamped.

Initial spawn and persistence-failure rollback use the same group reconciliation path.
Rollback runs once when a runtime persistence failure begins. It clears pending receipts,
stops movement components, and resets actor Action/epoch receipt state. A complete
replacement group keeps the same stable IDs, so selection and follow continue by ID.
If any founder cannot be placed, only actors created by that attempt are removed and the
world remains failed and paused. WorldState, resources, and time are not changed.
Shutdown marks teardown before saving, so a close-time save failure reports the error
without creating replacement actors.

Every new gather Action now waits for an actual valid engine arrival receipt, even when
durable XY equals the resource XY. A presentation offset alone cannot grant food or
water. Resume and retry also repair old schema-2 pending gathers whose saved
`AwaitingMovement` was false. The portable regression proves no receipt or stale receipt
means no goods and one fresh valid arrival means one effect.

`AIity.Bridge.FounderPlacement` is a native regression that creates an isolated transient
world and never begins play or opens a production save. It expects separated positions
to remain unchanged, ten co-located durable positions to become ten unique
nonoverlapping blocking capsules, bounded-search failure to clean only attempt actors,
and serialized WorldState to remain unchanged. This test passed in the foundation pass and again after the M2 graphics change.
It also expects `INT32_MIN`/`INT32_MAX` coordinate preflight rejection to preserve the
existing group and serialized state.

The initial view and follow source set controller rotation, not only pawn rotation.
The prototype HUD keeps the selected founder name visible and has measured wrapping and
reserved status space. Measured 16-pixel body text at 720p (20 at 1080p), bounded long names and failure
status, and procedural colors/lighting are now verified. Art remains primitive.

Verify founder movement, mouse and keyboard selection, follow framing, needs, Event feed,
failed movement, pause/speed, save status, geometry, and exact paused reopen before
calling the foundation playable.

## World database and recovery

The runtime world database is:

`Saved/Worlds/Foundation.sqlite`

Its backups use the same path with `.backup` and `.backup.previous`. These files and
SQLite `-wal`/`-shm` files are local generated data and must not be committed.

SQLite integer columns are signed 64-bit. Tick, LogicalSeconds, Event ID/Tick/AgentId,
and consumed receipt ID must fit that range. WorldState validation rejects corrupt loaded
values. Candidate envelope validation rejects Event and receipt overflow before `BEGIN`.
Seed and PRNG remain full-range uint64 values serialized as text. The native
`AIity.Persistence.SignedStorageRange` regression passed in the foundation native run; it verifies
all invalid candidates to leave current state, Events, receipts, and Checkpoints
unchanged.

Backup inspection opens a temporary read-only handle. Prepared statements must
finalize before that handle `Close`s. Installed SQLite destructors only assert if
the handle is still open.

Unreal 5.8 uses its custom SQLite VFS by default. Native evidence proved its read-only
API can delete source WAL during close. WorldStore therefore never SQLite-opens an
inspection source. While its existing world lock is held, it atomically creates a unique
private mode-0700 sibling directory, copies the source primary and present WAL to fixed
scratch names, and opens only the scratch database. SHM is derived and is not copied.
After every query is finalized, close must succeed before exact scratch primary, WAL,
SHM, and journal names are deleted and the empty directory is removed non-recursively.
Copy, close, or cleanup failure returns unknown and cannot permit recovery. Failed
scratch remains diagnostic where cleanup could not complete.

Any rollback journal on the original fails before source inspection or interrupted
recovery. A rollback journal also counts as evidence when the primary is missing.
Rollback journals are never copied or replayed. Backup, previous backup, recovering,
temporary backup, and temporary rotation inputs must be self-contained primary files
without WAL or rollback-journal dependencies.

Before copying, source primary plus WAL bytes are measured. Free scratch space must
cover those bytes while leaving the configured disk reserve. This runs only during
open, recovery, and backup validation, never per Tick. Its temporary-byte ceiling is
exactly primary plus WAL size; its open/backup delay grows with retained database
history. Native latency remains to be measured.

The `AIity.Persistence.ReadOnlyWalFamilyPreservation` regression makes schema 2
before WAL, disables automatic checkpointing, and commits schema 999 only in WAL. While
the seed's sole connection is idle, it copies the complete present main/WAL/SHM family
to a GUID probe. The probe must reject unsupported schema 999 and preserve every source
byte and presence bit except its expected `.lock`. Other probes require a zero-byte
primary plus nonempty WAL, an existing rollback journal, and a missing-primary rollback
journal to remain unchanged. A recoverable primary also proves that a backup with a WAL
dependency cannot authorize publication. This controlled fixture does not prove a safe
copy of a database that is changing.

Before any manual reset:

1. Close every AIity/Unreal process.
2. Copy the exact `Saved/Worlds/` directory to a separate dated backup location.
3. Confirm the copy contains the database and any `-wal`, `-shm`, `.backup`, and
   `.backup.previous` files.
4. Only then rename the exact `Saved/Worlds/Foundation.sqlite*` family for diagnosis.

Do not use a broad recursive delete. Do not remove another project, report directory,
home directory, or arbitrary path. A future in-app reset needs confirmation and
database-safe backup first.

On write, low-space, schema, lock, or recovery failure, keep the world paused and preserve
the last committed state. Never initialize over an unknown/newer save or overwrite a
damaged original during backup recovery.

Persistence source now takes a Mac advisory lock on the stable `.lock` sibling before
inspection or recovery and holds it through close. Unknown, unreadable, missing-schema,
and newer saves fail closed without backup replacement. A supported damaged primary may
use only a backup whose WorldState deserializes and matches stored Tick and
LogicalSeconds.

Recovery first copies and syncs the damaged primary plus existing `-wal` and `-shm`
files. It then writes a durable `.recovery` marker, removes checked stale sidecars, and
atomically replaces the primary with the validated `.recovering` file. The marker lets
the next open finish an interrupted publication. This does not claim that several file
renames are one atomic action. Backup rotation atomically publishes
`.backup.previous` before replacing `.backup`, so a recognized validated copy remains.
Owned SIGKILL boundary tests passed. Native `waitpid`/`WIFSIGNALED` death, power-loss,
SQLiteCore filesystem faults, and graphical play remain unproven. Details are listed in
[status.md](status.md).

## Binary assets and local models

Current art references built-in engine shapes; see [asset provenance](asset-provenance.md).
Future `.uasset` and `.umap` files use Git LFS only after remote quota/support is checked.
Never commit engine binaries, caches, saves, secrets, or model weights.

M2 local model setup is not implemented. When it is, Ollama must bind to loopback with
cloud features disabled. Record exact artifact digest/settings in the world database and
[status.md](status.md). Do not invent setup commands before that adapter exists.
