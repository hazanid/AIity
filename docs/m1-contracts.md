# M2 portable contracts (legacy `m1` paths)

Owner approved this engine-independent preparation on 2026-09-18 while Unreal
installs. It is a bounded exception, not M2 runtime integration and not a foundation
release gate.

The owner renumbered milestones on 2026-09-18. These unintegrated contracts belong
to M2; their file paths, namespaces and executable output retain historical M1 names.
No implementation or acceptance status changed with the numbering.

## Glossary

- **Request ticket:** Trusted request ID, agent ID, execution epoch, attempt ID,
  source tick, deadline, lifecycle, and offered choices. The runtime creates it. A
  model cannot.
- **Offered choice:** A request-scoped choice ID for one allowed survival action or
  talk route. It is not an admitted Action ID or Event ID.
- **Proposal:** Required contract version, one untrusted selected choice ID, text,
  and a short intent summary. It cannot allocate IDs, name extra recipients, or
  gain tools.
- **Candidate:** A validated outcome waiting for commit. It is not delivered
  dialogue, consumed identity, memory, inventory, or public success.
- **Audience snapshot:** The actual recipients frozen at admission from trusted
  membership. It is never a model-built recipient list.
- **Response identity:** Trusted `(RequestId, AttemptId)` pair consumed at most once
  on successful commit. Request IDs are unique for the world lifetime; attempt IDs
  are unique within one request. Epoch is checked separately at admission.
- **Fallback:** Hand control back to existing deterministic survival rules. This
  prototype does not copy a needs algorithm.
- **GodRequest:** An inert queued record with trusted requester ID and text. Queued
  is not a capability. Owner/God replies are designed below and are not implemented
  in this preparation.
- **durable XY:** A founder's last committed spatial observation in `X` and `Y`; not an
  exact live actor pose.

Current foundation `FEvent` has no audience field. Do not reuse it for private
dialogue.

## Limits

These are byte counts, not character counts. A UTF-8 character may use several
bytes.

| Limit | Value |
| --- | --- |
| Proposals per validation | 1 |
| Offered choices per ticket | 1–8 |
| Current permission records | 1–32 |
| Distinct delivery recipients | at most 8 |
| Message / GodRequest text | 2048 UTF-8 bytes |
| Intent summary | 256 UTF-8 bytes |
| Agent prompt context | 16384 UTF-8 bytes |
| Agent prompt records | at most 64 |
| Published deliveries / consumed response keys | 64 each |
| Total live memories | 128 |
| Live memories per agent | 32 |
| Private membership | exactly 2 trusted agents |
| Group membership | 2–8 trusted agents |

Source tick may be older than the current tick. Inference may take time. Future
source ticks, passed deadlines, and replaced, cancelled, consumed, unknown, or
mismatched identities are rejected. The ticket epoch must equal the current
authoritative epoch. Offered choices, current permissions, and membership are
checked again at final admission.

## Authority

- Models never mutate WorldState, inventories, or God status.
- Recipients come from the trusted snapshot on the offered choice, then must still
  match the live conversation.
- Current permissions are a bounded trusted projection. A historical offered
  choice cannot override a removed current permission.
- Command-looking text, markup, URLs, paths, and fake system prompts are stored as
  data only. They do not pick tools, load files, fetch URLs, or approve GodRequests.
- Keyword filters are not authority. Trusted IDs and allowlists are.
- Manager transcript is a separate function. It is never a prompt flag on agent
  context.
- Future proximity and perception work under
  [issue #1](https://github.com/hazanid/AIity/issues/1) must use an explicit live spatial
  observation when exact pose matters. Durable XY and collision-safe presentation
  placement are not proof that two actors are currently nearby.

## Candidate and publication protocol

This is an in-memory protocol check, not SQLite and not durable messaging.

1. `ValidateProposal` returns a preview candidate or an error. It does not publish
   and the preview is never authority.
2. `TryCommit` accepts the original proposal, current ticket, and current admission
   world. It calls the same validator and constructs its own candidate.
3. A failed `TryCommit` leaves no published message, no memory, and an unconsumed
   response identity.
4. A successful `TryCommit` publishes once and consumes the composite response key.
5. A repeated `(RequestId, AttemptId)` is rejected. Different request IDs may use
   the same attempt number.
6. Epoch, permission, deadline, membership, identity, pause, cancel, and replacement
   changes are checked immediately before publication.
7. Unsupported proposal contract versions are rejected at preview and final
   admission. The published record keeps the validated version, offered choice,
   epoch, and bounded intent summary.
8. Capacity refusal is atomic and does not evict audit history.
9. Survival-action candidates carry no dialogue and create no memories. Talk
   candidates freeze audience and write one speaker-claim memory per recipient.

## Memory and privacy

Each delivered talk memory stores owner, kind (observation, speaker claim, or
inference), composite source identity, audience snapshot, and content. Records with
invalid IDs, enum kind, text, owner, duplicate/zero audience, or oversized audience
are excluded. Agent context includes only records the agent owns and is in the
audience for.

Context uses a conservative compile-time budget: 48 metadata bytes plus 8 bytes per
audience ID plus content bytes. It also stops at 64 records. This is not future JSON
or transport size. New group members do not inherit old snapshots. Departed members
keep memories they already own.

The derived-memory helper accepts caller-produced text, checks the same text bound,
and copies the immutable source identity, owner, and audience. It allocates no
authoritative memory ID and does not truncate UTF-8 or claim to summarize.
Manager transcript reads one published delivery per composite response key, not one
copy per recipient memory. It includes the bounded intent summary for owner
inspection. Agent memory and context contain delivered text only; private intent
summary is not copied into another agent's context.

## GodRequest

`AdmitGodRequest` takes a trusted requester ID. Status is always queued. Agent
text cannot impersonate, approve, grant goods, or create executable tasks. A queued
or acknowledged record has no runtime capability.

Owner/God replies are **not** implemented here. The later durable design is: one
GodRequest ID, plus a monotonic reply revision owned by the manager, committed
before any agent-visible acknowledgement. Do not treat that reply protocol as
tested.

## Future wire examples (not parsed in this pass)

Typed C++ record checks are not JSON decoder proof. A later Unreal HTTP/JSON
adapter must bound bytes, reject unknown and duplicate fields, and keep IDs as
decimal strings so uint64 values are not rounded.

Request ticket fragment:

```json
{
  "request_id": "11",
  "agent_id": "1",
  "epoch": "3",
  "attempt_id": "1001",
  "source_tick": "10",
  "deadline_tick": "40",
  "offered_choice_ids": ["21", "22", "23"]
}
```

Proposal fragment:

```json
{
  "contract_version": 1,
  "request_id": "11",
  "agent_id": "1",
  "epoch": "3",
  "attempt_id": "1001",
  "choice_id": "21",
  "text": "We should rest by the river.",
  "intent_summary": "ask"
}
```

Required proposal keys are `contract_version`, `request_id`, `agent_id`, `epoch`,
`attempt_id`, `choice_id`, `text`, and `intent_summary`. Additional keys are
rejected later. Enums are closed: survival actions are the existing `EActionKind`
values except `None`; talk kinds are private or group.

The typed prototype is compile-time contract version 1. Every proposal must echo
that version, including final admission. It has no runtime decoder. The later
Unreal decoder must carry and reject unsupported wire contract versions.

## Future local model adapter

Documentation checked 2026-09-18. Do not install Ollama in this pass.

- [Ollama chat API](https://docs.ollama.com/api/chat): `POST /api/chat` has
  `stream` default true, so the adapter must send
  `stream: false` and validate completion. Omit tools. Do not publish hidden
  thinking.
- [Ollama structured outputs](https://docs.ollama.com/capabilities/structured-outputs)
  still need independent validation.
- [Ollama FAQ](https://docs.ollama.com/faq): bind `127.0.0.1:11434` and set
  `OLLAMA_NO_CLOUD=1` when installing.

Approved M2 target remains one loaded model and one in-flight request. This
preparation has no scheduler or transport.

## Tests in this pass

`scripts/m1-contract-check.cpp` covers identity binding, allowlists, byte limits,
unknown enum/choice values, final epoch/permission/deadline/membership/identity
changes, failed commit, one-shot publish, composite deduplication, capacity refusal,
metadata-aware context limits, private sentinel exclusion, successful group
publication, group join/leave access, owner transcript deduplication, safe derived
memory, unsupported proposal versions, bounded intent publication without agent
context exposure, open-then-cancel rejection, GodRequest inertness, command text as
data, and fallback markers.

It does not cover JSON syntax, Unreal HTTP, Ollama, SQLite dialogue, God reply
revisions, or runtime routing from a fallback marker into existing survival rules.

## Later integration

Move these reviewed functions into `Source/AIity/Simulation/` once. Do not keep a
second copy. Foundation engine gates stay required before playable M2.
