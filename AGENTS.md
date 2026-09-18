# AIity agent guide

This file applies to the entire repository.

## Read first

Before substantive work, read:

1. [README.md](README.md)
2. [docs/status.md](docs/status.md)
3. [docs/plan.md](docs/plan.md)
4. [docs/development.md](docs/development.md)

Then inspect the source, tests, and any task-specific instructions.
The plan records intent. Status records evidence. Code and tests record current behavior.

## Current scope

The active milestone is the foundation: a runnable, saved Unreal settlement.
Approved M1 follows the foundation and adds local-model lives and dialogue.
M2 economy and invention, M3 generations, and later ecology are roadmap only.
Do not silently move roadmap work into the active milestone.

Owner-approved exception (2026-09-18): engine-independent M1 contracts may live
under `prototypes/m1/` plus `scripts/m1-contract-check.cpp` and
[docs/m1-contracts.md](docs/m1-contracts.md) while Unreal installs. Do not edit
foundation Source, schema, or runtime for this exception. After fresh review, move
the contracts into the runtime later; do not keep two copies.

The product is one Unreal C++ app.
The logical core under `Source/AIity/Simulation/` is ordinary portable C++.
Engine adapters, presentation, and persistence live under `Source/AIity/`.
Use Unreal facilities before adding services, frameworks, or dependencies.

## Development baseline

The latest owner direction is **main first**. It overrides earlier baseline gates that
waited for complete graphical or clustered-restore proof before integrating this
reviewed, buildable source. This is not a playable release and not parallel-ready.

The baseline landed through [PR #12](https://github.com/hazanid/AIity/pull/12).
A foundation-fix agent next addresses
remaining runtime proof and bugs [#5](https://github.com/hazanid/AIity/issues/5),
[#7](https://github.com/hazanid/AIity/issues/7), [#8](https://github.com/hazanid/AIity/issues/8),
and [#11](https://github.com/hazanid/AIity/issues/11) before parallel feature work.
Graphical, clustered-restore, and playability proof remain required before parallel
issues and before any playable claim.

For delegated work, use one issue, one owner, one isolated worktree, one branch, and one pull request.
Ownership must not overlap. Serialize review and merge. Serialize Unreal builds,
Automation, and graphical sessions on the shared Mac. Portable work may run
independently in isolated worktrees.

Durable world open/create/load/resume belongs in `OnWorldBeginPlay`, after its
superclass and before `GameMode::StartPlay`; do not add map-name filters.
[#9](https://github.com/hazanid/AIity/issues/9) portable pull-request checks (`feature`)
and [#10](https://github.com/hazanid/AIity/issues/10) camera launch (`bug` + `UI`)
remain backlog. Latest native build passed in 8.17 seconds; `run.JPpLs9` passed all
13 `AIity.*` tests. Graphical and foundation follow-up remain after `main`.

## Integration authority

Owner-approved policy (2026-09-18): the lead agent owns integration into `main` and may
commit scoped, verified changes directly to `main`. Preserve unrelated user changes;
direct-write authority does not expand product scope or waive required checks.

Subagents work on isolated `codex/` branches/worktrees and open PRs. They must not push
to `main`, approve their own work as the final reviewer, or merge. The lead reviews their
diffs and evidence, resolves blocking findings, and approves and merges their PRs.
Serialize integration and shared-host Unreal runs. Do not force-push or bypass required
checks. This explicit owner policy overrides personal workflow defaults that reserve
merging for the human or require a PR for every lead-authored change.

## Required invariants

World rules validate every model proposal and spatial observation.
Models and adapters never mutate authoritative state or execute code.
Agent perception is local; the owner can inspect all delivered dialogue.
Simulation uses fixed ticks, stable IDs, seeded randomness, and bounded resources.
Goods and later money use integer quantities with explicit sources and sinks.

Only one immutable candidate tick may await persistence.
Publish effects only after one atomic SQLite commit succeeds.
Never acknowledge an effect before commit or retry it under a duplicate identity.
A failed write pauses safely and leaves the last committed WorldState public.

Each movement action has an action ID and execution epoch.
Accept at most one valid terminal outcome per action and epoch.
Reject stale, duplicate, canceled, mismatched, and impossible receipts.
Reopen paused at committed simulation time and apply no offline progression.
Reissue active movement under the new committed epoch before accepting callbacks.
Never migrate, replace, or initialize an unknown or newer save.
Only one process may own a writable world at a time.

Founder `X` and `Y` are the last committed spatial observation, not an exact live actor
pose. Collision-safe presentation offsets never mutate them. Reconcile all ten founder
actors as one stable-ID group or fail paused without a partial attempt. Every new gather
Action requires an actual valid engine arrival receipt, even when durable XY equals its
resource.

Values bound into SQLite integers must fit signed 64-bit storage. Tick, LogicalSeconds,
Event identity fields, and consumed receipt IDs reject larger values before a
transaction. Seed and PRNG state remain full-range uint64 text.

Later adult intimacy requires authoritative age 18 or older and current mutual,
withdrawable consent. Presentation stays non-explicit. Children have no sexual
actions or prompts. Models cannot decide age or consent validity.

God interventions are typed, bounded, committed, and audited.
Runtime agents receive no shell, secret, filesystem, arbitrary network, or code tools.
Runtime-originated source-change requests still need explicit owner approval; the
simulation never rewrites itself. Development subagent PRs follow the integration policy.

## Verification and repository care

Run only narrow checks relevant to the change. Do not run full CI locally.
Portable checks do not prove Unreal compilation, Automation tests, graphics, or play.
Never claim an unrun gate passed. Record exact evidence in `docs/status.md`.
Do not commit saves, secrets, model weights, engine files, caches, or machine paths.
Track binary Unreal assets with appropriate Git LFS and document asset provenance.
Preserve user edits. Avoid force pushes, unreviewed merges, and destructive cleanup.
Update docs when a milestone, invariant, command, dependency, or blocker changes.

## GitHub issues

Every new GitHub issue needs one or more of `UI`, `graphics`, `bug`, `algorithm`,
`feature`. See [.cursor/rules/github-issues.mdc](.cursor/rules/github-issues.mdc).
