#include "AIityM1Contracts.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace AIity
{
namespace M1
{
namespace
{

std::vector<uint64_t> SortedUnique(std::vector<uint64_t> Values)
{
	std::sort(Values.begin(), Values.end());
	Values.erase(std::unique(Values.begin(), Values.end()), Values.end());
	return Values;
}

bool SameMembers(std::vector<uint64_t> Left, std::vector<uint64_t> Right)
{
	return SortedUnique(std::move(Left)) == SortedUnique(std::move(Right));
}

bool KnownAction(EActionKind Kind)
{
	switch (Kind)
	{
	case EActionKind::GatherFood:
	case EActionKind::GatherWater:
	case EActionKind::Eat:
	case EActionKind::Drink:
	case EActionKind::Rest:
		return true;
	default:
		return false;
	}
}

bool KnownChoiceKind(EChoiceKind Kind)
{
	switch (Kind)
	{
	case EChoiceKind::SurvivalAction:
	case EChoiceKind::PrivateTalk:
	case EChoiceKind::GroupTalk:
		return true;
	default:
		return false;
	}
}

bool KnownMemoryKind(EMemoryKind Kind)
{
	switch (Kind)
	{
	case EMemoryKind::Observation:
	case EMemoryKind::SpeakerClaim:
	case EMemoryKind::Inference:
		return true;
	default:
		return false;
	}
}

const FConversation* FindConversation(const FAdmissionWorld& World, uint64_t Id)
{
	for (const FConversation& Conversation : World.Conversations)
	{
		if (Conversation.Id == Id)
		{
			return &Conversation;
		}
	}
	return nullptr;
}

const FOfferedChoice* FindChoice(const FRequestTicket& Ticket, uint64_t ChoiceId)
{
	for (const FOfferedChoice& Choice : Ticket.Offered)
	{
		if (Choice.ChoiceId == ChoiceId)
		{
			return &Choice;
		}
	}
	return nullptr;
}

bool SameResponse(const FResponseKey& Left, const FResponseKey& Right)
{
	return Left.RequestId == Right.RequestId && Left.AttemptId == Right.AttemptId;
}

bool AlreadyConsumed(const FPublicationLedger& Ledger, const FResponseKey& Response)
{
	for (const FResponseKey& Item : Ledger.ConsumedResponses)
	{
		if (SameResponse(Item, Response))
		{
			return true;
		}
	}
	return false;
}

bool HasCurrentPermission(const FAdmissionWorld& World, uint64_t AgentId,
	const FOfferedChoice& Choice)
{
	for (const FCurrentPermission& Permission : World.CurrentPermissions)
	{
		if (Permission.AgentId != AgentId || Permission.Kind != Choice.Kind)
		{
			continue;
		}
		if (Choice.Kind == EChoiceKind::SurvivalAction &&
			Permission.ActionKind == Choice.ActionKind && Permission.ConversationId == 0)
		{
			return true;
		}
		if (Choice.Kind != EChoiceKind::SurvivalAction &&
			Permission.ActionKind == EActionKind::None &&
			Permission.ConversationId == Choice.ConversationId)
		{
			return true;
		}
	}
	return false;
}

uint64_t CountMemoriesForAgent(const FPublicationLedger& Ledger, uint64_t AgentId)
{
	uint64_t Count = 0;
	for (const FMemoryRecord& Memory : Ledger.Memories)
	{
		if (Memory.OwnerAgentId == AgentId)
		{
			++Count;
		}
	}
	return Count;
}

}

bool ByteLengthOk(const std::string& Text, uint64_t MaxBytes)
{
	return Text.size() <= MaxBytes;
}

bool AgentInAudience(uint64_t AgentId, const FAudienceSnapshot& Audience)
{
	if (AgentId == 0)
	{
		return false;
	}
	for (uint64_t Member : Audience.AgentIds)
	{
		if (Member == AgentId)
		{
			return true;
		}
	}
	return false;
}

uint64_t MemoryBudgetBytes(const FMemoryRecord& Memory)
{
	return MemoryBaseMetadataBytes +
		MemoryAudienceIdBytes * Memory.Audience.AgentIds.size() +
		Memory.Content.size();
}

bool ValidateMemoryRecord(const FMemoryRecord& Memory, std::string& Error)
{
	Error.clear();
	if (Memory.Id == 0 || Memory.OwnerAgentId == 0 ||
		Memory.SourceRequestId == 0 || Memory.SourceAttemptId == 0 ||
		!KnownMemoryKind(Memory.Kind))
	{
		Error = "Memory identities or kind are invalid.";
		return false;
	}
	if (Memory.Audience.AgentIds.empty() ||
		Memory.Audience.AgentIds.size() > MaxAudienceAgents ||
		!ByteLengthOk(Memory.Content, MaxMessageBytes))
	{
		Error = "Memory audience or text exceeds its bound.";
		return false;
	}
	for (uint64_t AgentId : Memory.Audience.AgentIds)
	{
		if (AgentId == 0)
		{
			Error = "Memory audience identities must be nonzero.";
			return false;
		}
	}
	for (uint64_t Left = 0; Left < Memory.Audience.AgentIds.size(); ++Left)
	{
		for (uint64_t Right = Left + 1; Right < Memory.Audience.AgentIds.size(); ++Right)
		{
			if (Memory.Audience.AgentIds[Left] == Memory.Audience.AgentIds[Right])
			{
				Error = "Memory audience identities must be distinct.";
				return false;
			}
		}
	}
	if (!AgentInAudience(Memory.OwnerAgentId, Memory.Audience))
	{
		Error = "Memory owner is outside the immutable source audience.";
		return false;
	}
	return true;
}

bool ValidateProposal(const FRequestTicket& Ticket, const FProposal& Proposal,
	const FAdmissionWorld& World, FDeliveryCandidate& Out, std::string& Error)
{
	Out = FDeliveryCandidate{};
	Error.clear();
	if (Proposal.ContractVersion != AIity::M1::ContractVersion)
	{
		Error = "Proposal contract version is unsupported.";
		return false;
	}
	if (Ticket.RequestId == 0 || Ticket.AgentId == 0 || Ticket.Epoch == 0 ||
		Ticket.AttemptId == 0)
	{
		Error = "Request ticket identities must be nonzero.";
		return false;
	}
	if (World.CurrentEpoch == 0 || Ticket.Epoch != World.CurrentEpoch)
	{
		Error = "Request epoch is not the authoritative current epoch.";
		return false;
	}
	if (World.CurrentPermissions.empty() ||
		World.CurrentPermissions.size() > MaxCurrentPermissions)
	{
		Error = "Current permission projection is missing or exceeds its bound.";
		return false;
	}
	for (const FCurrentPermission& Permission : World.CurrentPermissions)
	{
		const bool bSurvival = Permission.Kind == EChoiceKind::SurvivalAction &&
			KnownAction(Permission.ActionKind) && Permission.ConversationId == 0;
		const bool bTalk = (Permission.Kind == EChoiceKind::PrivateTalk ||
			Permission.Kind == EChoiceKind::GroupTalk) &&
			Permission.ActionKind == EActionKind::None && Permission.ConversationId != 0;
		if (Permission.AgentId == 0 || (!bSurvival && !bTalk))
		{
			Error = "Current permission projection contains an invalid record.";
			return false;
		}
	}
	if (Ticket.Offered.empty() || Ticket.Offered.size() > MaxOfferedChoices)
	{
		Error = "Offered choices must be a nonempty bounded allowlist.";
		return false;
	}
	std::vector<uint64_t> ChoiceIds;
	for (const FOfferedChoice& Choice : Ticket.Offered)
	{
		if (Choice.ChoiceId == 0 || !KnownChoiceKind(Choice.Kind))
		{
			Error = "Offered choice identity or kind is invalid.";
			return false;
		}
		const bool bSurvival = Choice.Kind == EChoiceKind::SurvivalAction &&
			KnownAction(Choice.ActionKind) && Choice.ConversationId == 0 &&
			Choice.MembershipSnapshot.empty();
		const bool bPrivate = Choice.Kind == EChoiceKind::PrivateTalk &&
			Choice.ActionKind == EActionKind::None && Choice.ConversationId != 0 &&
			Choice.MembershipSnapshot.size() == 2;
		const bool bGroup = Choice.Kind == EChoiceKind::GroupTalk &&
			Choice.ActionKind == EActionKind::None && Choice.ConversationId != 0 &&
			Choice.MembershipSnapshot.size() >= 2 &&
			Choice.MembershipSnapshot.size() <= MaxAudienceAgents;
		if (!bSurvival && !bPrivate && !bGroup)
		{
			Error = "Offered choice metadata is invalid or exceeds its bound.";
			return false;
		}
		for (uint64_t AgentId : Choice.MembershipSnapshot)
		{
			if (AgentId == 0)
			{
				Error = "Offered audience identities must be nonzero.";
				return false;
			}
		}
		if (SortedUnique(Choice.MembershipSnapshot).size() !=
			Choice.MembershipSnapshot.size())
		{
			Error = "Offered audience identities must be distinct.";
			return false;
		}
		ChoiceIds.push_back(Choice.ChoiceId);
	}
	if (SortedUnique(ChoiceIds).size() != Ticket.Offered.size())
	{
		Error = "Offered choice identities must be distinct.";
		return false;
	}
	if (Proposal.RequestId != Ticket.RequestId || Proposal.AgentId != Ticket.AgentId ||
		Proposal.Epoch != Ticket.Epoch || Proposal.AttemptId != Ticket.AttemptId)
	{
		Error = "Proposal does not match the trusted request identity.";
		return false;
	}
	if (Ticket.Lifecycle == ELifecycle::Replaced)
	{
		Error = "Replaced request cannot admit a proposal.";
		return false;
	}
	if (Ticket.Lifecycle == ELifecycle::Cancelled)
	{
		Error = "Cancelled request cannot admit a proposal.";
		return false;
	}
	if (Ticket.Lifecycle == ELifecycle::Consumed)
	{
		Error = "Consumed request cannot admit a proposal.";
		return false;
	}
	if (Ticket.Lifecycle == ELifecycle::Expired || World.CurrentTick > Ticket.DeadlineTick)
	{
		Error = "Expired request cannot admit a proposal.";
		return false;
	}
	if (Ticket.Lifecycle != ELifecycle::Open)
	{
		Error = "Request is not open.";
		return false;
	}
	if (Ticket.SourceTick > World.CurrentTick)
	{
		Error = "Future request records are rejected.";
		return false;
	}
	if (Ticket.DeadlineTick < Ticket.SourceTick)
	{
		Error = "Request deadline predates its source tick.";
		return false;
	}
	if (!ByteLengthOk(Proposal.Text, MaxMessageBytes) ||
		!ByteLengthOk(Proposal.IntentSummary, MaxIntentBytes))
	{
		Error = "Proposal text exceeds the byte limit.";
		return false;
	}
	const FOfferedChoice* Choice = FindChoice(Ticket, Proposal.ChoiceId);
	if (Choice == nullptr)
	{
		Error = "Selected choice is not on the trusted allowlist.";
		return false;
	}
	if (!HasCurrentPermission(World, Ticket.AgentId, *Choice))
	{
		Error = "Selected choice is no longer currently permitted.";
		return false;
	}

	FDeliveryCandidate Candidate;
	Candidate.ContractVersion = Proposal.ContractVersion;
	Candidate.RequestId = Ticket.RequestId;
	Candidate.AgentId = Ticket.AgentId;
	Candidate.Epoch = Ticket.Epoch;
	Candidate.AttemptId = Ticket.AttemptId;
	Candidate.OfferedChoiceId = Choice->ChoiceId;
	Candidate.Kind = Choice->Kind;
	Candidate.ActionKind = Choice->ActionKind;
	Candidate.ConversationId = Choice->ConversationId;
	Candidate.Text = Proposal.Text;
	Candidate.IntentSummary = Proposal.IntentSummary;

	if (Choice->Kind == EChoiceKind::SurvivalAction)
	{
		if (!KnownAction(Choice->ActionKind) || Choice->ConversationId != 0)
		{
			Error = "Survival choice must name a known action and no conversation.";
			return false;
		}
		if (!Proposal.Text.empty())
		{
			Error = "Survival fallback choices cannot carry model dialogue.";
			return false;
		}
		Out = Candidate;
		return true;
	}

	if (Choice->ConversationId == 0 || Choice->MembershipSnapshot.empty() ||
		Choice->MembershipSnapshot.size() > MaxAudienceAgents)
	{
		Error = "Talk choice is missing a bounded trusted audience snapshot.";
		return false;
	}
	if (!AgentInAudience(Ticket.AgentId, FAudienceSnapshot{Choice->MembershipSnapshot}))
	{
		Error = "Sender is not in the trusted conversation membership.";
		return false;
	}
	const FConversation* Conversation = FindConversation(World, Choice->ConversationId);
	if (Conversation == nullptr || Conversation->Kind != Choice->Kind)
	{
		Error = "Conversation is unknown or unauthorized for this route.";
		return false;
	}
	if (Conversation->Members.empty() ||
		Conversation->Members.size() > MaxAudienceAgents)
	{
		Error = "Current conversation membership exceeds its bound.";
		return false;
	}
	for (uint64_t AgentId : Conversation->Members)
	{
		if (AgentId == 0)
		{
			Error = "Current conversation identities must be nonzero.";
			return false;
		}
	}
	if (SortedUnique(Conversation->Members).size() != Conversation->Members.size())
	{
		Error = "Current conversation identities must be distinct.";
		return false;
	}
	if (!SameMembers(Conversation->Members, Choice->MembershipSnapshot))
	{
		Error = "Conversation membership changed during admission.";
		return false;
	}
	const std::vector<uint64_t> Audience = SortedUnique(Choice->MembershipSnapshot);
	if (Audience.size() != Choice->MembershipSnapshot.size() ||
		Audience.size() > MaxAudienceAgents || Audience.front() == 0)
	{
		Error = "Delivery audience exceeds the recipient bound.";
		return false;
	}
	if (Choice->Kind == EChoiceKind::PrivateTalk && Audience.size() != 2)
	{
		Error = "Private talk requires exactly two trusted members.";
		return false;
	}
	if (Choice->Kind == EChoiceKind::GroupTalk && Audience.size() < 2)
	{
		Error = "Group talk requires at least two trusted members.";
		return false;
	}
	Candidate.Audience.AgentIds = Audience;
	Out = Candidate;
	return true;
}

bool FPublicationLedger::TryCommit(const FRequestTicket& Ticket,
	const FProposal& Proposal, const FAdmissionWorld& World, std::string& Error)
{
	Error.clear();
	FDeliveryCandidate Candidate;
	if (!ValidateProposal(Ticket, Proposal, World, Candidate, Error))
	{
		return false;
	}
	const FResponseKey Response{Candidate.RequestId, Candidate.AttemptId};
	if (AlreadyConsumed(*this, Response))
	{
		Error = "Duplicate request/attempt response identity is rejected.";
		return false;
	}
	if (Published.size() >= MaxPublishedDeliveries ||
		ConsumedResponses.size() >= MaxConsumedResponses)
	{
		Error = "Publication ledger capacity is full; nothing was consumed.";
		return false;
	}

	std::vector<FMemoryRecord> NewMemories;
	if (Candidate.Kind == EChoiceKind::PrivateTalk ||
		Candidate.Kind == EChoiceKind::GroupTalk)
	{
		if (Memories.size() + Candidate.Audience.AgentIds.size() > MaxTotalMemories)
		{
			Error = "Total memory capacity is full; nothing was consumed.";
			return false;
		}
		if (NextMemoryId == 0 ||
			NextMemoryId > std::numeric_limits<uint64_t>::max() -
				Candidate.Audience.AgentIds.size())
		{
			Error = "Memory identity capacity is exhausted; nothing was consumed.";
			return false;
		}
		uint64_t NextId = NextMemoryId;
		for (uint64_t Owner : Candidate.Audience.AgentIds)
		{
			if (CountMemoriesForAgent(*this, Owner) >= MaxMemoriesPerAgent)
			{
				Error = "Per-agent memory capacity is full; nothing was consumed.";
				return false;
			}
			FMemoryRecord Memory;
			Memory.Id = NextId++;
			Memory.OwnerAgentId = Owner;
			Memory.Kind = EMemoryKind::SpeakerClaim;
			Memory.SourceRequestId = Candidate.RequestId;
			Memory.SourceAttemptId = Candidate.AttemptId;
			Memory.Audience = Candidate.Audience;
			Memory.Content = Candidate.Text;
			if (!ValidateMemoryRecord(Memory, Error))
			{
				return false;
			}
			NewMemories.push_back(Memory);
		}
	}

	if (bFailNextCommit)
	{
		bFailNextCommit = false;
		Error = "Commit failed; nothing was published or consumed.";
		return false;
	}
	ConsumedResponses.push_back(Response);
	Published.push_back(Candidate);
	Memories.insert(Memories.end(), NewMemories.begin(), NewMemories.end());
	NextMemoryId += NewMemories.size();
	return true;
}

std::vector<FMemoryRecord> ContextForAgent(uint64_t AgentId,
	const std::vector<FMemoryRecord>& Memories)
{
	std::vector<FMemoryRecord> Context;
	uint64_t Used = 0;
	for (const FMemoryRecord& Memory : Memories)
	{
		if (Context.size() >= MaxContextRecords)
		{
			break;
		}
		std::string Error;
		if (!ValidateMemoryRecord(Memory, Error) ||
			Memory.OwnerAgentId != AgentId ||
			!AgentInAudience(AgentId, Memory.Audience))
		{
			continue;
		}
		const uint64_t Need = MemoryBudgetBytes(Memory);
		if (Need > MaxContextBytes - Used)
		{
			break;
		}
		Context.push_back(Memory);
		Used += Need;
	}
	return Context;
}

std::vector<FDeliveryCandidate> ManagerTranscript(
	const std::vector<FDeliveryCandidate>& Published)
{
	std::vector<FDeliveryCandidate> Transcript;
	for (const FDeliveryCandidate& Delivery : Published)
	{
		if (Transcript.size() >= MaxPublishedDeliveries)
		{
			break;
		}
		if (Delivery.Kind != EChoiceKind::PrivateTalk &&
			Delivery.Kind != EChoiceKind::GroupTalk)
		{
			continue;
		}
		bool bSeen = false;
		for (const FDeliveryCandidate& Existing : Transcript)
		{
			if (Existing.RequestId == Delivery.RequestId &&
				Existing.AttemptId == Delivery.AttemptId)
			{
				bSeen = true;
				break;
			}
		}
		if (!bSeen)
		{
			Transcript.push_back(Delivery);
		}
	}
	return Transcript;
}

bool DeriveAccessPreservingMemory(uint64_t AgentId, const FMemoryRecord& Source,
	const std::string& DerivedText, FDerivedMemoryCandidate& Out, std::string& Error)
{
	Out = FDerivedMemoryCandidate{};
	if (!ValidateMemoryRecord(Source, Error) ||
		Source.OwnerAgentId != AgentId ||
		!AgentInAudience(AgentId, Source.Audience))
	{
		Error = "Source memory is not available to the deriving agent.";
		return false;
	}
	if (!ByteLengthOk(DerivedText, MaxMessageBytes))
	{
		Error = "Derived memory text exceeds the byte limit.";
		return false;
	}
	Out.OwnerAgentId = Source.OwnerAgentId;
	Out.Kind = EMemoryKind::Inference;
	Out.SourceRequestId = Source.SourceRequestId;
	Out.SourceAttemptId = Source.SourceAttemptId;
	Out.Audience = Source.Audience;
	Out.Content = DerivedText;
	return true;
}

bool AdmitGodRequest(uint64_t NextId, uint64_t TrustedRequesterId, const std::string& Text,
	FGodRequest& Out, std::string& Error)
{
	Out = FGodRequest{};
	Error.clear();
	if (NextId == 0 || TrustedRequesterId == 0)
	{
		Error = "GodRequest identities must be nonzero trusted values.";
		return false;
	}
	if (!ByteLengthOk(Text, MaxMessageBytes))
	{
		Error = "GodRequest text exceeds the byte limit.";
		return false;
	}
	Out.Id = NextId;
	Out.RequesterId = TrustedRequesterId;
	Out.Text = Text;
	Out.Status = EGodStatus::Queued;
	return true;
}

FFallback HandBackToSurvival(EFallbackReason Reason)
{
	FFallback Fallback;
	Fallback.Reason = Reason;
	return Fallback;
}

}
}
