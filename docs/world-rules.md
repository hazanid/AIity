# Foundation world rules

## Glossary

- **WorldState:** Authoritative logical state at the last committed durable write.
- **Tick:** Durable commit-sequence counter. It advances for every published candidate,
  including reopen transitions. It is not wall-clock time and is not LogicalSeconds.
- **LogicalSeconds:** Simulation-time seconds. They advance only with normal simulation
  candidates, not with reopen.
- **candidate tick:** A possible next WorldState. It stays private until SQLite commits it.
- **Checkpoint:** A bounded periodic copy of WorldState kept for recovery/history. The
  foundation retains at most eight Checkpoint rows.
- **founder:** One of the ten adults in the first settlement.
- **action:** A typed task owned by the logical core.
- **movement receipt:** One final engine report for one action and execution epoch.
- **execution epoch:** A number that changes on reopen, making old movement reports invalid.
- **resource node:** A place with a finite food or water amount.
- **event:** A saved, readable result. Complete history lives in SQLite; WorldState keeps
  only a bounded display buffer.
- **shared clock:** One source helper that scales logical and founder movement time.
- **frame budget:** Capped simulation time shared by fixed ticks, movement, and timeout.
- **reissue transition:** A committed new execution epoch that makes old receipts stale.
- **world lock:** A stable sibling file whose Mac advisory lock gives one process ownership.
- **recovery marker:** A durable file that makes staged recovery resumable after interruption.
- **durable XY:** A founder's last committed spatial observation in `X` and `Y`; not an
  exact live actor pose.
- **presentation placement:** A collision-safe actor location derived from durable XY
  without changing WorldState.

## Clock and ordering

Normal simulation candidates advance Tick and LogicalSeconds together by one. Founder IDs
are processed from low to high. Input movement receipts are processed by receipt ID. A
candidate copies the committed WorldState, applies receipts and rules, and is saved in one
SQLite transaction. Only a successful commit replaces the public WorldState.

Reopen commits one transition that advances Tick and execution epoch, preserves
LogicalSeconds, and reissues awaiting movement under the new epoch. No offline wall-clock
time is applied. Tick exists so Checkpoint keys and commit ordering stay unique across
reopens.

The world opens paused. One source-written clock supplies at most four simulation seconds
per real frame at 1×, 2×, or 4×. The subsystem drains every whole fixed tick from that
frame budget, committing each candidate before the next. Movement timeout uses the same
budget. CharacterMovement walk speed is adjusted so physical distance uses that budget
even during a large hitch.

Pause, failed initialization, and failed persistence produce no frame budget and accept
no movement receipt. The freeze path consumes queued input, stops movement, and clears
forces before CharacterMovement ticks. A failed write discards pending bridge receipts.
Successful writable retry first commits a reissue transition and remains paused; only
fresh-epoch receipts can finish movement after resume. Startup world failure cannot be
retried during that run. Clock and retry contracts passed targeted Automation. Group
placement and retry rollback still need native proof. See [status.md](status.md).

## Founder behavior

The ten founders have stable IDs, names, ages, appearance notes, six personality values,
food, water, and five body needs. Five are men and five are women. Sex does not change
personality or work.

Hunger, thirst, and fatigue rise each simulation tick. A founder drinks at high thirst,
eats at high hunger, and rests at high fatigue or low stamina. Otherwise the founder
alternates between the finite food patch and river access. Gathering moves one unit from a
resource node to an inventory. Eating and drinking remove one inventory unit. No rule
creates inventory by accident.

Maximum hunger or thirst lowers health. The foundation has no death transition yet.

## Movement boundary

The core starts a gathering action with a stable action ID, target, and execution epoch.
Unreal may move the visible Character, but it cannot grant resources. It returns one
terminal movement receipt: arrived, blocked, interrupted, or failed.

Every new gather Action awaits that receipt, even when durable XY already equals the
resource XY. Presentation placement may offset a Character to avoid collision, so durable
XY alone never proves physical arrival.

The core validates agent, action, epoch, receipt identity, and outcome before consumption.
It rejects duplicate, stale, mismatched, and impossible receipts without reserving the
receipt ID. Arrival must be within the target radius. Only an accepted arrival can complete
gathering.

Reopen keeps Action IDs for awaiting movement and assigns the new execution epoch. Old
receipts are rejected so a fresh terminal outcome can still finish the reissued movement.
Resume and retry also force old schema-2 pending gathers with `AwaitingMovement=false`
back to awaiting movement under the new epoch.

Initial spawn and persistence-failure rollback place all ten founders as one stable-ID
group. They try durable XY first, then a fixed bounded nearby search with blocking
collision. Placement failure removes only actors created by that attempt and fails paused.

## Persistence

SQLiteCore stores:

1. One `current_state` WorldState row.
2. Up to eight periodic Checkpoints (every 300 Ticks in current source).
3. Complete indexed Events.
4. Accepted consumed receipt IDs.

SQLite integer columns are signed 64-bit. Tick, LogicalSeconds, Event ID/Tick/AgentId,
and consumed receipt ID must fit before commit. Candidate overflow is rejected before
the transaction. Seed and PRNG remain full-range uint64 text.

WorldState serialization carries at most 64 recent Events for the HUD. Old Events remain
in SQLite and are not copied into every snapshot. Repeated identical unavailable Events are
suppressed until that founder has another outcome.

Each candidate uses `BEGIN IMMEDIATE` and one `COMMIT`. A failed write rolls back and pauses
the world while the last committed WorldState remains public. Commits pause before write
when free disk space is below the configured reserve.

An existing database is inspected read-only before writable setup. Unknown, unreadable,
missing-schema, and newer saves are rejected unchanged. Only a demonstrably new database
initializes schema transactionally. A stable sibling `.lock` file uses a Mac advisory OS
lock before inspection and remains held through close. Missing primary plus any save-family
evidence never initializes a new world.

Normal save checkpoints the WAL, creates a temporary backup with `VACUUM INTO`, validates
SQLite and the saved WorldState, then publishes it. WorldState Tick and LogicalSeconds
must match their SQLite row. Backup rotation publishes a validated `.backup.previous`
before atomically replacing `.backup`.

Automatic recovery applies only when schema 2 is readable but SQLite or WorldState
validation fails. It tries `.backup`, then `.backup.previous`. Before publication it
copies and syncs the damaged primary and existing journal family. A durable recovery
marker makes checked sidecar removal and atomic primary replacement resumable after an
interruption. Unknown and unsupported primaries are never replaced. The foundation does
not migrate schemas. Engine and forced-process recovery proof remains open.
