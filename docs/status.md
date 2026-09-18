# Project status

Date: 2026-09-18

## Foundation verification — 2026-09-18 lead pass

This is a verified development foundation for bounded follow-up issues, not a playable
release or an implemented M1 model/dialogue runtime. Graphical polish, camera/mouse
interaction, packaging, accessibility, and performance remain separate unfinished work.

Fresh inspection found clean synchronized `main` at `1b48115`. PR #12 is merged;
Cursor Bugbot completed successfully and its inline-comment list is empty. The old
`Continue AIity to verified playable release` heartbeat is now **PAUSED**. The archived
workflow and deleted worktree were not resumed.

### Native and portable evidence

- The primary checkout built `AIityEditor Mac Development arm64` with Unreal 5.8.2,
  Xcode 26.3/SDK 26.2 in 61.59 seconds (exit 0).
- `TestReports/AIity/run.ATEqpo` passed all 13 `AIity.*` tests: 11 success and 2 with
  expected corrupt-fixture SQLite warnings; failed 0, notRun 0, inProcess 0.
- That includes `FounderPlacement`, `SignedStorageRange`, legacy-gather resume/retry,
  unchanged save-family preservation, and the three owned process-kill commit phases.
- Added only explicit out-of-range arrival assertions to the existing portable/native
  rules checks: a receipt 151 units from the target grants no goods, consumes neither
  resource nor receipt identity, and keeps the Action awaiting movement.
- Portable core check passed. The added native regression compiled in 8.03 seconds;
  `TestReports/AIity/foundation-arrival-20260918` passed the one changed rules test,
  with failed/notRun/inProcess all zero.
- Final source rebuild after removing temporary mouse diagnostics passed in 10.02 seconds.
  There is no retained runtime behavior change in this foundation pass.

### Graphical and persistence evidence

All graphical sessions used `-game -windowed` at 1280×720 and an isolated `-userdir`.
The closed production SQLite family (including present WAL and both backups) was copied
and SHA-256 compared before use. SQLite inspection was performed on additional closed
copies only. Production family hashes stayed unchanged.

Local evidence is retained under `Saved/FoundationVerification/20260918-main/`;
reports, save copies, screenshots and machine paths are not committed.

- The preserved clustered save opened with ten live `AIityFounderCharacter` instances
  (confirmed by the native object list), six logged collision-safe offsets, no startup
  failure, and the complete stable-ID group. Both clusters were visible through follow.
  Native `FounderPlacement` additionally checks blocking collision, non-overlap, all ten
  IDs, coordinate bounds, unchanged state, and bounded-placement failure cleanup.
- First ordinary close exited 0. Closed-copy comparison proved Tick 241→242, exactly
  one reopen Event and epoch increment, LogicalSeconds still 238, and byte-identical
  serialized founder/resource/action records.
- Second launch stayed paused at Tick 243. F9 produced `ScreenShot00000.png`; immediate
  normal window close exited 0. Closed-copy comparison again showed LogicalSeconds 238,
  unchanged founder/resource/action records, and exactly one new reopen Event.
- A fresh isolated world opened paused at Tick 0, visibly moved founders after Space,
  and paused at Tick 10. It committed 11 arrivals and 11 gathers, then closed with exit 0
  after F9. The closed save had ten pending movement Actions.
- That active save reopened paused at Tick 11 / LogicalSeconds 10. Closed-copy comparison
  confirmed the same ten Action IDs, epoch 2, unchanged other founder fields/resources,
  and exactly one reopen Event. Ordinary close exited 0.
- A subsequent restored run visibly moved at 4×, paused at Tick 34 / LogicalSeconds 32,
  and closed with exit 0 after F9. Its new Events included 14 arrivals, 14 gathers, and
  three blocked movement outcomes. This run included an experimental input-mode change,
  subsequently removed; no simulation/persistence code was changed.
- Final unchanged runtime reopened paused at Tick 35, then visibly moved and gathered at
  1×, paused at Tick 69 / LogicalSeconds 66, and exited 0 after F9. Closed-copy Events
  confirm the single Tick 35 reopen; production save-family hashes still match.
- Keyboard selection, founder follow, pause/resume, and 4× were graphically observed.
  Mouse selection did not change selection through automated clicks. Temporary logging
  showed the handler running with unavailable mouse coordinates. A native input-mode
  experiment did not resolve it and was removed; no mouse-selection success is claimed.
  This remains camera/input follow-up under #10.
- Known #4 small text, #6 gray materials/black sky/lighting and Lumen warnings, and #10
  inconsistent initial camera framing remain visible. None is described as finished.

### Shutdown investigation (#5)

The preserved historical crash calls `FSceneViewport::Destroy` from viewport destruction
on the render thread, via `FSlateRHIRenderer::DrawWindow_RenderThread` releasing a shared
reference. The installed 5.8.2 source still obtains a temporary viewport shared pointer
on that rendering path, while `Destroy` requires the game thread.

This closely matches Epic's unresolved [UE-382156](https://issues.unrealengine.com/issue/UE-382156)
and the [Epic support discussion](https://forums.unrealengine.com/t/fsceneviewport-updateviewportrhi-assertion-failed-isingamethread/2736082).
It supports an engine viewport-lifetime race; it does not prove F9 is its trigger.
The current bounded ordinary-close and screenshot-close attempts did not reproduce it.
No engine patch, assertion suppression, crash upload, or project workaround is claimed.
Use the verified standalone `-game` workflow for development, pause before close, and
retain the closed save family if the race recurs. Successful bounded runs are not a
promise that an intermittent upstream defect is gone. Keep upstream tracking for an
engine-version retest; no speculative project regression can fix engine ownership.

### Remaining release gates and integration

- Follow-up issues #4 HUD, #6 graphics, #9 portable PR checks, #10 camera/input, and
  later approved M1 work remain. #1 dialogue and #2 manager movement need bounded
  runtime contracts before implementation; prototypes are not runtime behavior.
- Packaged cooking/1080p performance, broader accessibility, and a complete playable
  M1 session are unverified. Native signal-exit status, power-loss, filesystem-fault,
  and recovery-publication interruption proof remain distinct from SIGKILL-send tests.
- Lead owns `main`; issue subagents use non-overlapping isolated `codex/` branches and
  PRs. Review/merge and all shared-host Unreal runs are serialized. No force push or
  bypassed checks. Runtime agents receive no execution tools.

The sections below preserve the baseline's implementation inventory and historical
investigation record. Statements about an underway rebuild or unrun tests there are
historical and superseded by the exact evidence above.

## Baseline record (historical)

## Glossary

- **source-written:** Files exist in the worktree. Presence is not engine proof.
- **engine-built:** Unreal successfully compiled the project.
- **play-tested:** A person opened the world and checked real behavior.
- **blocked:** Required evidence cannot run until a prerequisite or fix exists.
- **Tick:** Durable commit-sequence counter in WorldState and SQLite. It advances on
  reopen transitions. It is not wall-clock time.
- **LogicalSeconds:** Simulation-time seconds. Reopen must not advance them.
- **VFS:** SQLite's engine-specific file layer.
- **development baseline:** Reviewed, buildable source suitable to land on `main`;
  not a playable release and not parallel-issue ready.
- **issue worker:** One owner for one issue, isolated worktree, branch, and pull request.
- **durable XY:** A founder's last committed spatial observation in `X` and `Y`; not an
  exact live actor pose.
- **presentation placement:** A collision-safe actor location derived from durable XY
  without changing WorldState.

## Active milestone

Foundation PR: runnable, saved settlement.

The owner approved the foundation and M1 plan. Foundation runtime gates still block a
playable M1. Owner also approved engine-independent M1 contract preparation while Unreal
installs; see [m1-contracts.md](m1-contracts.md). M2 economy/invention, M3 generations,
and later ecology remain roadmap.

The owner now directed **main-first** integration of this reviewed, buildable source.
That overrides waiting for complete graphical or clustered-restore proof before `main`.
Those gates remain required before parallel features and before any playable claim.
Remaining runtime foundation proof and bugs
[#5](https://github.com/hazanid/AIity/issues/5),
[#7](https://github.com/hazanid/AIity/issues/7),
[#8](https://github.com/hazanid/AIity/issues/8), and
[#11](https://github.com/hazanid/AIity/issues/11) are handled after `main` by a
foundation-fix agent, then parallel work may start.

Integration status: [PR #12](https://github.com/hazanid/AIity/pull/12) merged to `main`
at `cb3d10e` on 2026-09-18. The local main checkout is synchronized. Saves and native
reports were copied and byte-compared before the clean foundation worktree was removed.
The optional Bugbot review was still running at merge; no required GitHub checks or
branch rules were configured. Later findings belong to labeled foundation issues.

The lead may commit scoped, verified work directly to `main` and owns review/approval/
merge of development subagent PRs; see [integration authority](../AGENTS.md#integration-authority).

Latest evidence: corrected native Mac Development arm64 build passed in 8.17 seconds.
Targeted report `run.JPpLs9` passed all 13 `AIity.*` tests (11 success plus 2 success
with expected corrupt-fixture SQLite warnings; failed 0, notRun 0, inProcess 0).
Graphical and remaining foundation bugs stay follow-up after `main`.

## Writer session note

This is not a playable release and not parallel-ready. Small text and primitive
rendering remain deferred under [#4](https://github.com/hazanid/AIity/issues/4) and
[#6](https://github.com/hazanid/AIity/issues/6).

## Source-written

- Unreal project, game/editor targets, module, Mac config, and Unreal ignore/LFS rules.
- Portable C++ WorldState (schema 2), seeded founders, needs, finite food/water, survival
  Actions, movement receipts, deterministic tick ordering, and text serialization.
- SQLiteCore store and Unreal world subsystem with commit-before-publication ordering.
- Procedural engine-content valley, ten simple founder Characters, observer camera,
  mouse and keyboard selection, controller-based follow, and prototype Canvas
  needs/Event/status display.
- Unreal Automation source groups `AIity.Rules`, `AIity.Persistence`, and `AIity.Bridge`.
- Narrow portable check and Unreal Automation wrapper scripts.
- Portable M1 contract prototype under `prototypes/m1/` (not runtime).
- Repository knowledge: `AGENTS.md`, `docs/plan.md`, `docs/development.md`, this file,
  `docs/world-rules.md`, `docs/asset-provenance.md`, and `docs/m1-contracts.md`.

Source-written does not mean play, Automation, or recovery passed.

## Corrections present in source and evidence state

These review items have matching source edits. Native evidence is listed separately:

1. Reopen uses `BuildResumeCandidate`: advances Tick and execution epoch once, preserves
   LogicalSeconds, and reissues awaiting movement under the new epoch.
2. Receipt validation happens before consumption; stale-then-valid delivery is covered in
   portable and Automation source.
3. WorldState keeps at most 64 display Events; durable Events and consumed receipts live
   in SQLite. Current snapshot plus up to eight periodic Checkpoints replace
   snapshot-per-tick history archives. Repeated identical unavailable Events are suppressed.
4. Existing databases are inspected read-only before writable open/setup. Future/missing
   schemas are rejected unchanged in the paths covered by current source.
5. New databases initialize schema in one transaction. Exclusive SQLite lock is taken on
   writable open. A stable sibling `.lock` receives a nonblocking Mac advisory lock before
   inspection/recovery and remains held through `Close`.
6. Backup creation uses temporary `VACUUM INTO`, SQLite plus WorldState validation, and
   atomic same-directory replacement. Rotation publishes recognized
   `.backup.previous` before replacing `.backup`. Fault seams cover both boundaries.
7. Known schema-2 corruption may recover only from a deserializable WorldState whose Tick
   and LogicalSeconds match SQLite metadata. Unknown, unreadable, missing-schema, and
   newer primaries fail closed without replacement.
8. Recovery copies and syncs the complete present primary/`-wal`/`-shm` family, writes a
   durable marker, checks sidecar removal, and atomically replaces only the primary. A
   later open finishes an interrupted marked recovery. Missing primary plus save-family
   evidence never creates a world.
9. Disk-reserve pause and commit/backup/recovery fault-injection seams exist in source.
10. Spawned founders request a native AI controller. One portable clock supplies the
    capped frame budget for logical ticks, walk distance, movement timeout, pause, and
    persistence-failure freeze at 1×/2×/4×.
11. Failed initialization cannot resume. Failed persistence requires `R` to pass disk and
    SQLite writable checks, then remains paused until `Space`.
12. `scripts/check-world.sh` no longer clears a caller path; it creates a unique `run.*`
   directory under a report parent.
13. Persistence Automation test is named for atomic commit/reopen and no longer presents
    clean close as crash recovery. Owned SIGKILL boundary tests passed. Native
    `waitpid`/`WIFSIGNALED` death and power-loss remain unproven.
14. Likely portable compile issues noted earlier (`std::vector::reserve`, rules include,
    Algo usage) are addressed in current source. This does not prove Unreal compilation.
15. Initial observer possession and founder follow set controller rotation used by the
    view. Pawn rotation remains aligned at startup.
16. `[` and `]` cycle stable founder records with wrap. The selected founder name remains
    visible in the HUD.
17. HUD layout scales with viewport size, measures and wraps text to available width,
    splits overlong tokens, and reserves a distinct status area for long failures.
18. Game mode retains hard references to required Cube/Cylinder/Sphere meshes, packaging
    config always cooks `/Engine/BasicShapes`, and missing assets or shape spawns produce
    visible startup failure instead of silently continuing.
19. Failed writes clear pending bridge receipts and partial frame budget. Writable retry
    commits a new execution epoch and reissues active movement before it can become
    resumable. Old receipts stay stale; fresh-epoch receipts can finish.
20. CharacterMovement's native tick-before-owner default is disabled before registration.
    One owner-before-component prerequisite avoids a cycle. The owner freeze path consumes
    pending input, stops velocity, and clears accumulated forces before movement ticks.
21. Each frame is capped at four simulation seconds. The subsystem drains every whole
    fixed tick one durable candidate at a time. Timeout and CharacterMovement distance use
    the same capped budget during a hitch.
22. Geometry or incomplete founder spawning marks a permanent startup failure in the
    bridge. No simulation time or writable retry can resume that run.
23. Test-only WorldStore barriers cover before-BEGIN, after all writes before-COMMIT, and
    after-COMMIT. `AIity.Persistence.ProcessCrash` launches only an owned, non-detached
    child with GUID fixture token and nonce, verifies the marker PID against `CreateProc`,
    and treats crash-phase success only when `kill(SIGKILL)` returns 0 against that live
    owned PID. Already-exited children and timeout/failure cleanup are separate and are
    not forced-termination evidence. Native `waitpid`/`WIFSIGNALED` SIGKILL-death is not
    claimed; Unreal `WaitForProc` does not expose it here. The UE 5.8
    `Automation RunTests ...; Quit` launcher form is kept. Reopen is through WorldStore.
24. Production inspection never SQLite-opens a source save. While the world lock is
    held, it atomically creates a private mode-0700 sibling directory, copies the primary
    and present WAL to fixed `World.sqlite` scratch names, inspects there, closes after
    statements finalize, and removes only exact owned scratch files and the empty
    directory. SHM is derived and is not copied. Schema remains 2.
25. Persistence tests now stop with the SQLite error when required open, commit, load,
    or state checks fail. Source adds clean reopen without `-wal`/`-shm` and failed
    inspection preservation checks for primary, WAL, SHM, and backup bytes.
26. `scripts/check-world.sh` sends engine output to `engine-output.log` inside each
    unique report directory. It prints the path and a tail bounded to 200 lines and
    64 KiB. It no longer waits for end-of-file on a captured pipe held by a descendant.
27. Automation source makes schema 2 before WAL, commits schema 999 only in
    a nonempty uncheckpointed WAL, copies the idle complete family to a GUID probe, and
    requires unsupported-999 rejection without changing primary, WAL, or SHM. It also
    checks preservation of a zero-byte primary with a nonempty WAL, existing rollback
    journals, missing-primary rollback journals, and rejection of a backup that depends
    on WAL.
28. The Automation wrapper reads only regular JSON files based on `lstat`; symlinks and
    special files are ignored. Its output-tail read is capped at 64 KiB. Native report
    JSON is decoded with `utf-8-sig` so its observed BOM is accepted. Recognized
    reports must include `inProcess`; that count is part of discovered total and of
    failed/unrun, and a missing field is an incomplete report.
29. Rollback journals fail closed before source inspection or interrupted recovery and
    count as missing-primary save-family evidence. They are never copied or replayed.
    Backups, recovering files, and temporary publication inputs must have no WAL or
    rollback-journal dependency.
30. Backup publication also rejects WAL or rollback-journal evidence beside either
    destination, including orphan sidecars, before creating or replacing output.
31. `AndroidFileServer` is explicitly disabled in the project. Its generated runtime
    settings were removed because Android networking is outside foundation scope.
32. `Initialize` now does superclass work only. Durable open/create/load/resume starts
    in `OnWorldBeginPlay`, after its superclass and before `GameMode::StartPlay`.
    `Deinitialize` saves and closes only when a Store exists. Historical native
    Automation and Tick 0/reopen proof passed before later source corrections.
33. Founder actors are reconciled as one group in stable founder-ID order. Placement
    tries durable XY, then 24 fixed nearby offsets with
    `DontSpawnIfColliding`. Pawn/world collision stays enabled. Failed placement removes
    only actors created by that attempt. Persistence failure invokes this path once,
    resets bridge execution, and never repeatedly teleports actors.
34. Presentation offsets do not change durable XY. Every new gather Action awaits an
    actual valid arrival receipt, including when durable XY equals the resource.
    Portable regression proof passed; the new native placement regression awaits Unreal.
35. Placement coordinate addition widens before adding offsets. All candidates must fit
    Unreal's existing safe actor-placement bound before existing actors are touched.
    `INT32_MIN`/`INT32_MAX` preservation regression source is unrun.
36. Teardown is marked before shutdown saving. Save failures still report, but cannot
    reconcile or spawn founder actors. A failed Store open releases the Store object.
37. Resume and retry set every pending gather to `AwaitingMovement=true` under the new
    execution epoch, including old schema-2 saves that stored false. Portable no-receipt,
    stale-receipt, and fresh single-effect regressions passed; native proof is unrun.
38. WorldState and candidate-envelope validation reject SQLite-bound unsigned values
    above `INT64_MAX` before `BEGIN`. Seed and PRNG text keep full uint64 range. Portable
    boundary and corrupt-load checks passed. `AIity.Persistence.SignedStorageRange`
    source expects rejected candidates to preserve state, Events, receipts, and
    Checkpoints; it is unrun.

## Remaining review and runtime obligations

### Bridge / presentation

1. `AAIController`, `AutoPossessAI`, owner-before-CharacterMovement ordering, queued-input
   clearing, actor resync, and module dependencies compiled in the arm64 editor target.
   Visible movement and gathering were observed once. Clustered-save restore now has a
   source fix and native regression but needs runtime proof.
2. Pause, 1×/2×/4× speed, large hitch, multi-tick drain, timeout, stale receipt rejection,
   retry reissue, and permanent startup failure passed Automation. Selection, follow,
   pause, and 4× were observed once. Full interactive and reopen proof remains.
3. Initial/follow framing, `[`/`]` cycling, selected-name visibility, and measured text
   wrapping need checks at 1280×720 and 1920×1080, including long failure text.
4. Required floor, river, walls, resource markers, and lighting need editor and packaged
   cook proof. Hard references and cooking config are source evidence only.

### Persistence

The CISO approved persistence internals. Senior engineering approved the reviewed
architecture. Runtime evidence:

1. Historical targeted Automation completed 11 tests with zero failed, not run, or
   in process. That report does not cover later placement and review corrections.
2. Atomic persistence, checkpoint boundaries, SQLite family preservation, lock
   behavior, and injected fault paths passed that historical targeted Automation.
   Isolated Tick 0 and one exact reopen were also historical. A current native rebuild
   is underway.
3. The process-kill gate passed before-BEGIN, before-COMMIT, and after-COMMIT. Each
   phase required `kill(SIGKILL)==0` on the live `CreateProc` PID and exact reopened
   state. This is not native `waitpid`/`WIFSIGNALED`, power-loss, or
   recovery-publication proof.

### Still-required unverified gates

These remain after `main`, before parallel features and before any playable claim:

- Native rebuild and `AIity.*` rerun of current source (underway; not yet recorded).
- Native `waitpid`/`WIFSIGNALED` SIGKILL-death, power-loss, filesystem-fault, and
  recovery-publication interruption behavior.
- [Issue #7](https://github.com/hazanid/AIity/issues/7) clustered-save collision restore
  with all ten founders.
- [Issue #5](https://github.com/hazanid/AIity/issues/5) ordinary launch, pause, close,
  and one-reopen safety investigation.
- [Issue #8](https://github.com/hazanid/AIity/issues/8) gather-arrival companion and
  [issue #11](https://github.com/hazanid/AIity/issues/11).
- Rendered valley quality, collision, HUD, and broader accessibility proof.
- Packaged 1080p performance, accessibility, screenshots/video.
- Ollama, JSON decoder, durable dialogue, God reply, M1/M2/M3 runtime behavior.

Do not call the current source a proven playable or parallel-ready build.

## Evidence actually obtained

Current native evidence:

- Native rebuild of current source is underway. No new pass is recorded yet.
- Historical: lifecycle build 55054; targeted report `run.ARybOf` completed all 11 tests;
  isolated fresh Tick 0; one exact reopen; three ordinary closes. Later source
  corrections are not covered by that report.

Portable and shell evidence:

- Portable logical-core compile with
  `clang++ -std=c++17 -Wall -Wextra -Werror` and
  `scripts/core-check.cpp` output `AIity portable core checks passed`
  (current writer run). It checks actual-arrival enforcement even when durable XY equals
  a resource, plus the production receipt queue, stale/fresh retry epochs,
  queued-input freeze calls, permanent startup failure, shared large-hitch budget,
  multi-tick drain, and Checkpoint policy at Ticks 0/299/300.
- Current writer run: focused placement/gather source assertions, repository Markdown
  links, touched-file whitespace, and `git diff --check` passed. No Unreal command ran.
- Portable presentation-helper compile with `clang++ -std=c++17 -Wall -Wextra -Werror`
  output `AIity portable presentation checks passed`. It covers founder cycling and HUD
  layout bounds at 1280×720 and 1920×1080, not Unreal rendering.
- Portable M1 contract compile with `clang++ -std=c++17 -Wall -Wextra -Werror`
  output `AIity portable M1 contract checks passed`. It covers final current epoch,
  permissions, deadline, membership and identity checks; composite response keys;
  bounded atomic ledgers/memories; metadata-aware context; private/group isolation;
  owner transcript deduplication; derived memory; inert GodRequests; and fallback
  markers; required proposal version; bounded published intent; and open-then-cancel
  rejection. Not JSON parsing, Ollama, SQLite dialogue, survival routing, or God replies.
- Historical failures, now superseded by the passing native gate:

- The first native `AIityEditor` Mac Development arm64 build reached UnrealBuildTool and
  failed with exit 6 before C++ compilation. Observed errors were forced C++17 under
  UE 5.8 and V5 target defaults. The module now uses UE's supported C++ default and
  both targets use V7.
- Second native `AIityEditor` build compiled many project units, including the SQLite
  store and process-crash test, then failed with exit 6 on two C++ errors:
  nonexistent `GetSpectatorPawnMovement()` and incomplete `FAIityWorldStore` at
  generated subsystem construction. Those two callsites were source-corrected.
- Third native `AIityEditor` Mac Development arm64 build passed (exit 0) with Unreal
  5.8.2, Xcode 26.3, and SDK 26.2.
- First Automation run discovered 10 `AIity.*` tests. Six Bridge tests logged success.
  `AIity.Persistence.AtomicCommitAndReopen` then crashed on an unclosed
  `FSQLiteDatabase` in `InspectFile`. No passing Automation report exists.
- Local SQLite sessions now use `FScopedSqliteDatabase` so prepared statements
  finalize before `Close`, including early returns. A native build passed with that fix.
- The second Automation run logged the same six Bridge successes, then the primary
  read-only inspection failed with `unable to open database file`. A later unchecked
  empty-founder access crashed the run. Cleanup ended with exit 130. No passing report
  exists.
- Installed source shows Unreal's default SQLite VFS uses version 1 I/O methods with no
  shared-memory support. A native build compiled the exclusive-before-query setup and
  guarded prerequisites.
- A pre-fix Automation run reported 8 succeeded, 3 failed, and 0 not run. All six
  Bridge tests, checkpoint boundaries, and `ProcessCrash` with before-BEGIN,
  before-COMMIT, and after-COMMIT owned SIGKILL phases passed.
- Atomic persistence proved source-family mutation: damaged and unknown source WAL files
  disappeared during SQLite inspection. Rules failed from generic pointer comparison,
  not a confirmed deterministic-rule mismatch.
- The engine process exited 0 despite the two failures. Plain UTF-8 decoding rejected
  the report BOM, so the wrapper exited 4 with a false zero-test result.
- The isolated-copy fix, rollback-journal guards, exact serialized-value assertion,
  future-WAL/zero-page/journal regressions, and BOM decoder later passed the complete
  targeted native gate.
- Six `AIity.Bridge.*` tests passed, including retry epoch, queued-input freeze,
  large-frame budget, and permanent startup failure. That is Automation evidence only,
  not play or graphics proof.
- `AIity.Persistence.ProcessCrash` passed all three phases with bounded parent/child
  deadlines, child no-recursion, exact nonce/phase/PID checks, GUID-only fixture
  selection, `kill(SIGKILL)==0` against each live owned PID, and exact reopened
  baseline/candidate/Event/receipt state. Native signal-exit status remains unverified.
- Current runner fixture passed BOM JSON decoding while ignoring symlink and FIFO JSON
  paths.
- Current repository Markdown link check passed.
- Current touched-file trailing-whitespace check passed.
- Missing-engine behavior: `scripts/check-world.sh` exits `2` when Unreal is absent.
- Safe report-path fixture: unique `run.*` child, existing sentinel preserved; no
  recursive clear of the report parent.

The historical lifecycle build, complete targeted Automation, fresh Tick 0, one exact
reopen, and three observed closes passed. Current placement and review corrections still
need the underway native rebuild. Graphical clustered restore, shutdown investigation,
package, and performance remain pre-parallel and pre-release.

## Prerequisites and blockers

- Xcode 26.3 build 17C529, Apple Metal 32023.864, SDK 26.2, and Unreal Engine 5.8.2
  compiled `AIityEditor` Mac Development arm64. Further compiler/API errors may still
  appear in later rebuilds.
- The lead handles native rebuilds, Automation, and graphical sessions, serialized on
  this Mac.
- Ollama is not installed. It is not required for foundation corrections or the
  portable M1 contract prototype.
- Earlier host observations (Apple silicon, 24 GB memory, about 87 GiB free, macOS 26.1)
  must be rechecked before installation or performance work.

## Issue integration

Owner direction is main first, then a foundation-fix agent, then parallel issues.
Each later issue still uses one owner, isolated worktree, branch, and pull request.
Ownership must not overlap. Review and merge stay serialized. Portable work may run
independently; Unreal builds, Automation, and graphical sessions serialize on this Mac.

[Issue #3](https://github.com/hazanid/AIity/issues/3) lifecycle work is in this source.
[#5](https://github.com/hazanid/AIity/issues/5), [#7](https://github.com/hazanid/AIity/issues/7),
[#8](https://github.com/hazanid/AIity/issues/8), and
[#11](https://github.com/hazanid/AIity/issues/11) follow after `main`. Features and polish
[#1](https://github.com/hazanid/AIity/issues/1),
[#2](https://github.com/hazanid/AIity/issues/2),
[#4](https://github.com/hazanid/AIity/issues/4),
[#6](https://github.com/hazanid/AIity/issues/6),
[#9](https://github.com/hazanid/AIity/issues/9), and
[#10](https://github.com/hazanid/AIity/issues/10) wait until that foundation-fix pass.
This `main` integration is not a release.

## Exact next work

1. Create the pull request and land the reviewed buildable source on `main`.
2. After `main`, run the foundation-fix agent for remaining runtime proof and bugs
   [#5](https://github.com/hazanid/AIity/issues/5),
   [#7](https://github.com/hazanid/AIity/issues/7),
   [#8](https://github.com/hazanid/AIity/issues/8), and
   [#11](https://github.com/hazanid/AIity/issues/11).
3. Keep SIGKILL-send confirmation distinct from `waitpid` death, power-loss, and
   recovery-publication proof.
4. Require graphical clustered restore, ordinary shutdown, and playability only before
   parallel features and release, not as a pre-`main` bar.
5. After those foundation engine gates, move approved M1 contracts into runtime once.

See the [approved plan](plan.md), [development guide](development.md),
[world rules](world-rules.md), and [asset provenance](asset-provenance.md).
