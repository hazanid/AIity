# AIity — a living Stone Age world

Status: owner-approved on 2026-09-18.

This is the canonical product and architecture plan. Approval covers the foundation
and M1. M2, M3, and further increments are roadmap, not current implementation.
[Status](status.md) says what source exists and which gates have real evidence.

## Glossary

- **WorldState:** The authoritative, versioned logical state at a committed simulation tick.
- **Agent:** An individual with identity, needs, knowledge, goals, and an action controller.
- **Intent:** A proposed typed action, not permission to change the world.
- **Action:** An admitted intent with duration, prerequisites, costs, effects, and interruption rules.
- **Observation:** What an agent can perceive or remember; it differs from the manager's global view.
- **Event:** An accepted outcome, rejected request, conversation, or intervention in local history.
- **SoulProfile:** A fictional birth record with inherited/generated traits and a model policy.
- **GodRequest:** A bounded request for advice, a resource intervention, or a new capability.
- **Observatory:** The manager interface inside Unreal.
- **Checkpoint:** A durable logical state and event boundary used to resume.
- **execution epoch:** An identity for one runtime session that makes old callbacks stale.
- **movement receipt:** One terminal engine outcome for one movement Action and execution epoch.
- **candidate tick:** An immutable possible next WorldState that is not public before commit.
- **development baseline:** A buildable technical foundation for independent issue work;
  not a playable release.
- **playable release:** A wider gate covering graphics, use, packaging, performance, and
  milestone behavior.
- **issue worker:** One owner for one issue, isolated worktree, branch, and pull request.
- **durable XY:** A founder's last committed spatial observation in `X` and `Y`; not an
  exact live actor pose.
- **presentation placement:** A collision-safe actor location derived from durable XY
  without changing WorldState.

## Product contract

Build a local Unreal Engine desktop simulation in a believable river valley with five
adult men and five adult women. Each founder has a name, age, appearance, personality,
memories, needs, relationships, possessions, and goals. The owner watches life unfold,
follows people, reads all conversations, and speaks through a constrained God interface.
AI chooses among allowed possibilities; executable world rules decide outcomes.

The approved product uses Unreal, a playable village, hybrid local intelligence, an
omniscient manager, human approval of real code changes, and a world that pauses while
closed. No remote multi-user service or Linear ticket is required.

Intimacy uses an authoritative age of at least 18 simulated years, mutual current
consent, and the ability to withdraw. Motives may include affection, recreation,
pleasure, and wanting children. Presentation may show courtship, affection, and
before/after scenes. Sexual activity stays implied or covered, with no explicit sexual
anatomy, acts, or erotic narration. Children have ordinary family and developmental
interactions only. Reproduction is a simulation rule, not graphic content.

This is a bounded simulation of selected physical, biological, and social rules. It
cannot reproduce every law of nature or unrestricted invention. New capabilities become
explicit, testable rules through reviewed increments.

## Architecture and ownership

Use one Unreal C++ application and one loopback-only Ollama process when M1 requires it.
Use Unreal navigation, CharacterMovement, collision, animation, landscape, lighting,
UI, HTTP/JSON, and SQLite support. Do not add a web dashboard, Python service, message
broker, vector database, multiplayer server, or general agent framework.

The logical simulation core is ordinary C++ with no rendering dependency. A thin Unreal
world subsystem owns it. Required executable checks are Unreal Automation tests launched
headlessly against the installed engine. A portable compiler check may help during setup
but never replaces engine or persistence gates. Add adapters only when a milestone needs
them.

Flow:

`local observations → needs/goals and optional model proposal → typed validation → scheduled Action → engine spatial feedback → authoritative effects → committed Event/WorldState → characters and Observatory`

Unreal is authoritative for collision and actual character movement. The logical core
owns resources, needs, relationships, trades, time, births, and permissions. Reaching a
berry bush does not grant food by itself. Arrival, reach, availability, and completion
must all succeed.

Every spatial feedback item carries agent, Action, execution epoch, and receipt identity.
The core rejects stale, duplicate, impossible, or canceled outcomes. Blocked routes time
out and release reservations. An accepted movement Action has at most one accepted
terminal outcome: `arrived`, `blocked`, `interrupted`, or `failed`. Adapters report
observations only. They cannot mutate inventory, logical position, progress, needs, or
Action state.

The core validates a terminal outcome and commits it atomically with effects and the
consumed receipt identity. Pose observations may be committed for perception and save
continuity, but never count as arrival or grant benefits. Reopen creates a new execution
epoch, reissues active movement from committed pose and progress, and rejects prior
session callbacks.

Durable XY is not an exact current actor pose. Presentation may place an actor at a
nearby collision-safe offset without repairing or migrating WorldState. New gather
Actions always require a valid engine arrival receipt, even when durable XY equals the
resource XY. Future proximity and perception rules must distinguish durable XY from live
physical pose.

The simulation uses a fixed tick, stable entity order, explicit seeded randomness, and
integer resource quantities. Rendering runs separately. Logical replay consumes recorded
model decisions and spatial outcomes. It does not rerun an LLM or promise bit-identical
physics and animation. Initial history replay is read-only reconstruction from Events and
Checkpoints. Cinematic rewind is later work.

Repository layout:

- `AIity.uproject`, `Config/`, and future `Content/`: Unreal application and source assets.
- `Source/AIity/Simulation/`: portable WorldState and rules.
- `Source/AIity/Persistence/`: SQLiteCore storage.
- `Source/AIity/`: engine bridge, characters, world, and Observatory.
- `Source/AIity/Private/Tests/`: targeted Unreal Automation scenarios.
- `scripts/`: narrow checks.
- `prototypes/m1/`: owner-approved portable M1 contracts only, not runtime.
- `docs/`: plan, development, status, rules, asset provenance, and M1 contracts.

The owner approved development-baseline-first integration. A safe, reviewed development
baseline may land before finished graphics and deferred features. This does not permit
broken startup, data loss, unsafe shutdown, skipped native checks, skipped reviews, or a
playable-release claim.

After the development baseline, issue workers may work in parallel only when ownership
does not overlap. Each issue uses one isolated worktree, branch, and pull request.
Review and merge are serialized. Work that shares GameMode, FounderCharacter, or save
schema must declare ordering before edits begin.

## Time and safe persistence

One simulation second is the base unit. Initial day length is 30 real minutes at 1×.
Controls offer pause, 1×, 2×, and 4×. Rates are gameplay assumptions, not biological
claims. Aging and gestation follow a documented simulation calendar. Tests may advance
the logical clock. Accelerated generations are deferred.

Use one SQLite world database through Unreal SQLiteCore. One writer commits changed
state and Events in one transaction per logical tick. Keep periodic Checkpoints for
recovery and history. Record schema version, seed, PRNG state, logical clock, next IDs,
Action progress, reservations, inventories, relationships, conversation turns, model
identities, and pending GodRequests as those features arrive. Use prepared statements
and a bounded persistence path. Never acknowledge an irreversible command before commit.

In v1, allow only one candidate tick awaiting commit. It contains state changes, Events,
consumed model/spatial receipts, PRNG state, and ID counters. Only after `COMMIT` may it
be published to later ticks, adapters, prompts, UI outcomes, or command acknowledgements.
Writes cannot be coalesced, reordered, or skipped. Rendering may interpolate authorized
movement but cannot expose an uncommitted logical effect.

Keep the last committed WorldState while writing. Queue saturation, slow storage,
low-space protection, or commit failure applies backpressure and pauses simulation time.
Discard failed candidates and keep the last good state. Resync visuals before retry.
Optimize this ordering only after measurement.

Normal close:

1. Stop accepting decisions.
2. Pause at a tick boundary.
3. Cancel inference and invalidate callbacks.
4. Finish the current durable commit.
5. create and validate a Checkpoint/backup.
6. Close SQLite.

The UI shows saving, saved, or failed and offers retry. It never claims success after a
failed save.

Reopen:

1. Obtain an exclusive lifetime world lock.
2. Inspect compatibility without changing the file.
3. Validate schema and database.
4. Recover from a validated backup if needed while preserving damaged evidence.
5. Restore the last committed tick.
6. Commit one new execution epoch and reissue active movement.
7. Rebuild actors and verify positions.
8. Open paused.

Apply no elapsed wall-clock time. Canceled model requests are reconsidered from fresh
state, not replayed as live requests.

SQLite crash recovery returns the last committed tick; only uncommitted work may be
lost. Transactional IDs prevent duplicate trades, messages, gifts, and births. Disk-full
or write failure pauses and preserves the last good save. Before schema migration, make
a consistent validated backup. Reject newer unsupported schemas without changing them.
Retain a prior validated backup. Never copy only a live database file while ignoring its
journal.

History is local and complete by default. Store Events in indexed SQLite tables instead
of copying all history into each WorldState snapshot. Keep only a bounded display buffer
in memory. Retain current state plus bounded periodic Checkpoints. Show storage use and
pause at a configured reserve instead of silently dropping history. Export, archive, and
delete are deliberate owner actions; deletion needs confirmation. Saves use OS account
permissions. Application encryption is not promised. Exports may contain transcripts and
must say so.

## World rule catalog

### Space, matter, and environment

- One bounded river valley with navigable land, water, gathering patches, trees, stone,
  camp space, and a readable day/night cycle.
- Gravity, collision, slopes, reach, carrying capacity, travel distance, and walking or
  swimming constrain Actions.
- Water traversal costs stamina. Exhaustion triggers explicit rescue, escape, or injury.
- Edible plants, wood, stone, fuel, and stockpiled water are finite.
- River supply is a declared environmental inflow. Sites regenerate only through stated
  rates and capacities.
- Crafting transforms inputs to products and waste. Eating consumes food, fire consumes
  fuel, spoilage destroys food, and wear reduces durability.
- Weather, temperature, wetness, seasons, fire spread, water quality, disease, and
  predation are later layers. Do not show them as implemented statistics before rules use them.

### Bodies, personality, and knowledge

- Track stamina separately from sleep fatigue, hunger, thirst, health, and stress.
- Rest restores stamina. Sleep lowers fatigue while metabolism continues.
- Training spends stamina and food/water, improves skill slowly, and has diminishing returns.
- Initial traits are curiosity, sociability, patience, risk tolerance, generosity, and
  preferred activities. Sex does not assign personality or work.
- Agents perceive nearby visible entities and messages addressed to them.
- Private dialogue is private from other Agents but visible to the owner.
- Public speech has range; group membership controls group delivery.
- Memories separate observed fact, a speaker claim, and inference.
- Preferences, friendship, conflict, trust, grief, and learning grow from bounded Events.
- Injury, aging, and death have explicit transitions. Death stops Actions and inference.
- A hidden lifespan trait is a longevity value and optional maximum age, not hazard immunity.

### Actions and tradeoffs

- Walk/run: location changes; time, hunger, thirst, stamina, distance, and load matter.
- Swim: movement and practice with higher stamina, exposure, and recovery risk.
- Gather/forage: reserve and remove available finite goods; stop when depleted.
- Eat/drink: consume owned or reachable safe supplies; never duplicate or go negative.
- Rest/sleep: restore different body resources while time and metabolism continue.
- Train/practice: improve skills with time, effort, hydration, and material costs.
- Talk/listen/play: affect knowledge, trust, enjoyment, or conflict with bounded turns.
- Trade/gift: move owned items and finite money through exact, expiring, atomic offers.
- Craft/build/repair: require knowledge, tools, materials, place, and labor.
- Courtship/intimacy/parenting: require relationship, authoritative age, consent, and care rules.
- Request God/research: submit bounded requests or experiments with explicit costs.

### Economy and invention — M2 roadmap

Use barter plus finite symbolic shell currency at the first trading milestone. Shell
money is a gameplay convention, not a historical claim. Starting balances are world
data; only an explicit God mint Event creates money. Basic survival resources remain
gatherable without money.

Track ownership, private inventory, camp stock, capacity, offers, consumption,
production, and scarcity. Initial prices come from needs and stock with simple bounded
bargaining. Credit, tax, banking, land ownership, and institutions wait for supported rules.

The authored discovery path is cutting stone → cordage → gathering tool → fire-making →
cooked food → shelter/storage. Experiments use labor, material, practice, and knowledge;
failed attempts may consume inputs. Teaching spreads discoveries. Dialogue claims do not
create inventions.

Unsupported inventions become GodRequests describing behavior, ingredients, limits, and
requested capability. Code work requires a reviewed task and human approval. New builds
migrate saves safely. No live model-authored code executes.

### Family, birth, and SoulProfile — M3 roadmap

Founders are unrelated adults aged 20–35. Adult status comes from authoritative age.
Both people must be living, awake, able to consent, and at least 18. Consent is specific,
short-lived, withdrawable, and rechecked. Refusal has no automatic penalty. Family limits
are explicit. Children never enter sexual Actions, prompts, or scenes.

Fertility, desire for children, pregnancy, health costs, gestation, birth, infant care,
childhood, maturation, and death are separate states. Intimacy does not guarantee
pregnancy. A population budget counts pending pregnancies. If no birth can be admitted,
disclose it before conception; never silently remove a child. Initial growth target is
ten founders to twenty people, constrained by measured performance and owner settings.

At committed birth, God assigns a bounded SoulProfile with inherited traits, seeded
variation, appearance, longevity, latent talent, and allowed local model profile. Parents
propose the name; God validates it with a deterministic parent-origin fallback. Sex,
interests, personality, and model identity remain separate. Talent changes bounded
learning or endurance, not physical permissions. Supernatural powers require a later
approved rule expansion.

Local models may suggest profiles or names. Deterministic validation and fallback ensure
birth never waits forever for inference. Model choice changes reasoning style, not care,
consent protection, or resource access.

### Animals — later roadmap

The first nonhuman increment adds deer and birds through the same needs/Action structure
with deterministic species controllers. Grazing, drinking, sleeping, fleeing,
reproduction, and ecological resource use arrive together. Pets and domestication wait
for feeding, trust, ownership, and care rules.

## Local intelligence — M1

All humans have a continuously active needs/goals controller. A fair asynchronous
scheduler asks a local model at meaningful Events or intervals. Survival reflexes work
without inference. The model may choose a valid goal/Action, speak privately or to a
group, propose trade terms, suggest an experiment, or petition God. It has no direct
state access.

Benchmark Qwen3 1.7B Q4 and Qwen3 4B Instruct Q4 through Ollama. Offer economical,
richer, and clearly labeled deterministic fallback profiles. Record exact model digest
and settings in saves. Download only selected official artifacts after tool approval.
No paid or cloud inference in v1.

Initially load one model and allow one in-flight request. Use a bounded fair queue,
2,048–4,096-token contexts, concise output, deadlines, cancellation, and capped retries.
Humans may share weights but always have separate prompts, identity, observation, and
memory. Mixed models load sequentially with queue fairness; profile switching must be
limited and measured.

Requests contain allowed Action IDs and compact known facts. Responses use a fixed JSON
schema. Revalidate types, ranges, IDs, ownership, version, proximity, age, consent, and
budgets. Stale, invalid, or timed-out responses create visible fallback Events. Pause and
close invalidate pending responses. Speed controls cannot multiply inference without bound.

Ollama binds only to loopback with cloud features disabled. Models get no filesystem,
terminal, browser, credential, SQL, code execution, or arbitrary network tools.
Conversation cannot grant capability. Observatory shows a short intent summary and
observable evidence, never hidden model reasoning.

## God and owner controls

Any Agent may privately petition God. God uses a bounded policy to recommend advice,
resource intervention, recipe research, or a developer task. The manager sees requester,
evidence, cost, expected effect, and approval state.

The owner may pause/resume, set speed, inspect people/resources, speak, approve bounded
simulated grants, and manage GodRequests. Each intervention is a typed command with Event
ID, exact target, bounds, and audit record. Controls exist only in the in-process UI.
Agents cannot forge them through text or expose them on a network.

Source/rule changes become reviewable local tasks and are never posted automatically.
Future approved development may create branches and pull requests. Deployment and save
migration happen at safe restart. No automatic live code rewrite.

## Observatory and art direction

Use one in-engine interface: quiet full-screen world, collapsible inspector, and live
Event feed. Select/follow a person or use free camera. Show needs, possessions, current
Action, goals, relationships, model status, and recent history. Filter by person, group,
public/private channel, Action, trade, birth, request, or intervention. Show all categories
by default with unread counts and paused follow-scrolling.

History includes delivered dialogue, Action start/completion/failure, observed
consequences, intent summaries, births/deaths, and interventions. It is paginated or
virtualized. Reading old history does not pause the world. Live and history modes are clear.

Visual target: a small dense valley with paths, riverbank, rocks, vegetation, firelight,
atmospheric depth, responsive water, grounded animation, and distinct clothed humans.
Stage strong camera views early, then improve with licensed assets. No paid asset purchase
is approved. Use modest reusable characters first and profile higher-detail faces.

Accessibility includes keyboard use, remappable camera, visible focus, scalable text,
adequate contrast, non-color labels, speech subtitles, reduced camera motion, and
nonvisual Event descriptions. Saving, offline model, invalid Action, no selection, and
recovery states must be understandable.

## Delivery sequence

### M0 — toolchain and runnable world

Use an isolated worktree. Replace Unity ignore rules with Unreal exclusions. Keep source
assets/config and exclude intermediates, caches, saves, and model weights. Verify full
Xcode, a compatible stable Unreal 5, installation and cache space, and Ollama when M1
needs it. Pin versions only after a real Mac build. Pause for account action, unsupported
toolchain, or storage choice. Never delete unrelated files for space.

### Foundation PR — active milestone

Create the Unreal project and Mac target, bounded starter environment, ten founder
records/actors, deterministic movement, and gather/eat/drink/rest. Add selectable camera,
minimal needs/Event display, versioned SQLite schema, single-candidate ordering, seeded
core, terminal movement protocol, and real headless Automation entry point.

Required evidence: local engine build, rendered movement and inspection, rule scenarios,
normal paused reopen without offline time, forced interruption recovery without duplicate
effects, and honest engine asset provenance. The foundation is a usable technical base,
not completion of the full AI world.

Current baseline order completed [issue #3](https://github.com/hazanid/AIity/issues/3)
lifecycle activation. [Issue #7](https://github.com/hazanid/AIity/issues/7) clustered
founder restore is source-written and awaits native proof.
[Issue #5](https://github.com/hazanid/AIity/issues/5) ordinary shutdown remains an
investigation. Features and polish [#1](https://github.com/hazanid/AIity/issues/1),
[#2](https://github.com/hazanid/AIity/issues/2),
[#4](https://github.com/hazanid/AIity/issues/4), and
[#6](https://github.com/hazanid/AIity/issues/6) remain backlog. [Status](status.md)
records the passing native build and targeted 11-test report, plus the graphical
failures that still block baseline readiness.

Owner-approved exception (2026-09-18): while Unreal installs, portable M1 contracts
may be prepared under `prototypes/m1/` with `scripts/m1-contract-check.cpp` and
[m1-contracts.md](m1-contracts.md). This does not start the M1 runtime PR, change
foundation schema, or satisfy M1 acceptance. After fresh review, move the contracts
into the runtime later; do not keep two copies.

### M1 — approved next PR

Deliver ten distinct persistent lives, movement, finite survival, sleep and need decay,
and understandable autonomous choices. Integrate one real local model for private/group
dialogue and occasional goals with deterministic fallback. Add the first full Observatory,
pause/speed, local history, safe reopen, and a complete GodRequest conversation/inbox.
Capability acknowledgement means queued, not implemented. Resource grants wait for M2.

M1 acceptance: 30 minutes at 1× with all founders completing several survival Actions;
real private and group model dialogue delivered correctly, filterable, and preserved
after reopen; a founder GodRequest, reply, acknowledgement/rejection, and preserved
history; meaningful depletion and blocked movement; exact committed reopen; forced
interruption without duplicate effects; no offline time; real Unreal capture and measured
performance. A mockup or headless executable alone does not pass.

Build M1 in two runnable checkpoints:

1. Bounded Ollama goals and both dialogue channels.
2. GodRequest loop, filtering, visual polish, performance, and recovery proof.

### M2 — society that makes things

Add shell currency/barter, atomic offers, gifts, relationships, memory, training,
swimming, recipe discovery, teaching, crafting, shelter/storage, bounded resource grants,
and first deer/birds. Show shortage causing negotiation and discovery changing work.

### M3 — generations and controlled invention

Add consent-gated non-explicit intimacy, pregnancy, child care, parent naming,
God-assigned SoulProfiles, model profiles, aging/death/inheritance, family history, and
capacity accounting. Add exportable developer requests. Demonstrate a validated birth,
reopen during and after pregnancy, and rejection of unsupported live code change.

### Further increments

Weather and exposure; ecology and domestication; construction and experiments; community
institutions and deeper economy; genetics/disease; migration and larger populations;
better animation/faces; cinematic history. Choose from measured limits and emergent
stories, not speculative infrastructure. Humans review and merge each increment.

## Verification and release gates

Run targeted local checks only. CI runs after a pull request is opened.

Required executable gate: C++ tests registered under `AIity.Rules`,
`AIity.Persistence`, and `AIity.Bridge`, under `Source/AIity/Private/Tests/`.
Add `AIity.Inference` in M1. `scripts/check-world.sh` must resolve a pinned engine and run
the Mac command-line editor with the project path, `-unattended -NullRHI -nosplash`, only
the `AIity.*` group, verified completion, and a unique report path. It fails on engine
absence, timeout, failed/zero tests, or missing report. It must not erase a caller path or
run the full engine suite.

Core scenarios include resource/money conservation with explicit sources/sinks,
unavailable goods, exhaustion, interruption/cancellation, starvation/dehydration,
repeatable seeded decisions, actual arrival before gathering, collision-safe group
placement, bounded placement failure cleanup, spatial failure, and duplicate/stale
rejection. Money checks arrive with M2.

Persistence scenarios include normal close, real interruption before/during/after commit,
disk-full injection, unsupported schema unchanged, corrupted newest backup with prior
validated recovery, wall-clock delay, repeated load, exclusive second-writer rejection,
bounded storage, and low-space pause. Compare state/Event boundaries, not screenshots.

AI boundary scenarios in M1 include valid schema, malformed/oversized response, missing
IDs, privileged commands, prompt injection, offline runtime, cancellation, late callback,
bounded memory, and fair scheduling.

M2/M3 checks include concurrent trade acceptance, inventory limits, reservations,
pregnancy capacity, age 17 vs 18, consent withdrawal, duplicate birth delivery, naming
fallback, and forbidden juvenile prompts/Actions.

Unreal proof must compile and open the project; move/select/follow people; inspect live UI,
dialogue, navigation, and needs; check keyboard, contrast, and reduced motion; and test
save/reopen. Capture representative screenshots and short video when available.

Measured target: ten founders at 30 FPS or more in a packaged 1080p scene on the target
host while the selected model runs, with responsive pause/input, bounded inference queue,
and no sustained memory-pressure warning. Lower settings or report limits if it fails.
Never assert an unmeasured guarantee.

Require security, architecture, and engineering review before PR. Commit no secrets or
model binaries. Record asset provenance. Use Git LFS only after checking remote support
and quota. Missing engine, account action, licensed art, or failed performance remains an
explicit blocker, not a success claim.

## Known choices and official sources

The original repository is <https://github.com/hazanid/AIity>. Unreal is the chosen
engine despite the repository's original Unity ignore file. Mature cinematic visuals are
the target, not current proof. No paid assets are authorized.

Choose and pin a compatible Unreal/Xcode pair only after compiling the real project;
official requirements and release notes can differ:

- [Unreal Engine macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)
- [Unreal Engine release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes)
- [SQLiteCore API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/SQLiteCore)
- [Unreal C++ Automation tests](https://dev.epicgames.com/documentation/unreal-engine/write-cplusplus-tests-in-unreal-engine)
- [Automation test groups](https://dev.epicgames.com/documentation/unreal-engine/configure-automation-tests-in-unreal-engine)
- [Ollama structured output](https://docs.ollama.com/capabilities/structured-outputs)
- [Ollama runtime FAQ](https://docs.ollama.com/faq)
- [Official Qwen3 tags](https://ollama.com/library/qwen3/tags)

Observed host/tool facts belong in [status.md](status.md) and must be rechecked rather
than treated as permanent requirements.
