# Milestones and issue roadmap

Owner approved this organization on 2026-09-18. GitHub Milestones carry phase membership;
labels (`bug`, `feature`, `UI`, `graphics`, `algorithm`) describe work type. Do not add
redundant milestone labels. Preserve existing issue descriptions and labels.

The canonical behavior and invariants remain in [plan.md](plan.md); actual evidence is
in [status.md](status.md). Issue creation is not implementation completion.

## Numbering and scope

- [M1 — Technical foundation](https://github.com/hazanid/AIity/milestone/1): closed
  technical baseline, not a playable release. Includes completed lifecycle, restore,
  arrival, storage-range, shutdown-investigation and portable-CI issues.
- [M2 — First playable living world](https://github.com/hazanid/AIity/milestone/2):
  active approved delivery. Carries unfinished foundation presentation/input forward.
- [M3 — Society and production](https://github.com/hazanid/AIity/milestone/3): roadmap
  only, tracked by [#24](https://github.com/hazanid/AIity/issues/24).
- [M4 — Generations](https://github.com/hazanid/AIity/milestone/4): roadmap only,
  tracked by [#25](https://github.com/hazanid/AIity/issues/25).
- [Later world expansion #26](https://github.com/hazanid/AIity/issues/26): intentionally
  has no milestone or schedule until observed needs justify the next increment.

The former product M1/M2/M3 phases are now M2/M3/M4. Historical `prototypes/m1/`,
`scripts/m1-contract-check.cpp`, `docs/m1-contracts.md`, namespaces and test output keep
their original names to avoid unrelated code churn. They describe M2 preparation, not
an implemented model runtime. Historical status entries retain their original numbering.

## M2 issues

Presentation and usability:

- [#4 Readable HUD](https://github.com/hazanid/AIity/issues/4).
- [#6 Colors, dynamic lighting and sky](https://github.com/hazanid/AIity/issues/6).
- [#10 Camera and physical-input acceptance](https://github.com/hazanid/AIity/issues/10):
  source improvements landed; real-pointer evidence remains pending.
- [#15 Read-only object inspection](https://github.com/hazanid/AIity/issues/15).
- [#22 Reliable Mac launch and packaged build](https://github.com/hazanid/AIity/issues/22).

Survival and local intelligence:

- [#16 Understandable survival and depletion](https://github.com/hazanid/AIity/issues/16):
  bounded choices, sleep/rest and meaningful shortage, not silent resource refill.
- [#29 Purposeful idle exploration](https://github.com/hazanid/AIity/issues/29):
  owner-requested searching for useful nearby activities when needs are met, with
  bounded choices and survival interruption.
- [#17 Durable M2 contracts and records](https://github.com/hazanid/AIity/issues/17):
  sole initial schema/state owner for dialogue, memory and request identities.
- [#1 Nearby, awake dialogue eligibility](https://github.com/hazanid/AIity/issues/1).
- [#18 Bounded local-model adapter and scheduler](https://github.com/hazanid/AIity/issues/18).
- [#19 Validated goals and actual private/group dialogue](https://github.com/hazanid/AIity/issues/19).
- [#20 Filterable committed history and model status](https://github.com/hazanid/AIity/issues/20).
- [#21 Durable GodRequest advice/reply loop](https://github.com/hazanid/AIity/issues/21).

Release evidence:

- [#23 Complete M2 playability acceptance](https://github.com/hazanid/AIity/issues/23):
  real 30-minute 1x session, all founders performing survival actions, actual local-model
  private/group dialogue, history, GodRequest loop, meaningful depletion/blocked movement,
  safe reopen and interruption behavior. Also physical controls, accessible readable
  720p/1080p presentation, packaged launch and measured performance. Numeric performance
  budgets must be agreed before pass/fail. Native tests alone do not close M2.

## Dependency and integration order

1. #4 and #6 can have separate presentation/rendering owners. Lead serializes builds
   and graphical sessions. #10 is an acceptance task; reuse its integrated camera work.
2. #17 owns shared state/serialization/SQLite contracts. #16 may diagnose current-schema
   survival independently, but any new state fields must coordinate with #17. No two
   independent schema migrations may race. #29 exploration shares survival priorities
   with #16 and new action/visit fields with #17; do not create a competing schema change.
3. After #17 lands, #1 spatial eligibility and #18 inference adapter may proceed with
   non-overlapping ownership. #19 consumes both and performs end-to-end admission/delivery.
4. #15 follows #4 and #10. #20 follows durable history contracts and #4/#15 layout work;
   final dialogue proof needs #19. Serialize their HUD/controller edits.
5. #21 consumes #17/#19 and coordinates inbox layout with #20. It does not grant goods
   or capabilities. Resource interventions belong to M3.
6. #22 may prepare launch documentation independently; its final packaged asset/rendering
   acceptance follows #6. #23 waits for all M2 deliverables and integrates their evidence.

Every implementation issue has one owner, one isolated `codex/` branch/worktree and one
reviewable PR. If the issue proves too large, split it before implementation. Lead owns
review/merge, shared-file sequencing and all native/graphical execution on the shared Mac.
No model receives development shell, code or repository privileges.

## Later phases

M3 tracker #24 covers relationships, barter/finite shells, atomic offers/gifts,
recipe discovery/teaching/crafting, shelter/storage, training/swimming, bounded grants
and first animals. Existing [#2 manager movement](https://github.com/hazanid/AIity/issues/2)
is retained in M3: define typed bounded audited interventions and collision/action/save
semantics before implementation. It is separate from M2 read-only inspection.

M4 tracker #25 covers aging/death/inheritance, families, consent-gated non-explicit
intimacy, pregnancy/birth/care, parent naming, SoulProfiles and population capacity.
Authoritative age/consent, child protection and deterministic birth/reopen invariants
remain mandatory. Developer requests are reviewable data, never live executable code.

Refine these trackers into bounded implementation issues only when their phase is
selected. No artificial dates, speculative infrastructure or implied approval of later
runtime changes comes from assigning a milestone.
