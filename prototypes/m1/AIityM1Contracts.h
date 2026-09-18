#pragma once

#include "../../Source/AIity/Simulation/AIityWorldState.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AIity
{
namespace M1
{

constexpr uint32_t ContractVersion = 1;
constexpr uint64_t MaxOfferedChoices = 8;
constexpr uint64_t MaxAudienceAgents = 8;
constexpr uint64_t MaxMessageBytes = 2048;
constexpr uint64_t MaxIntentBytes = 256;
constexpr uint64_t MaxContextBytes = 16384;
constexpr uint64_t MaxContextRecords = 64;
constexpr uint64_t MaxCurrentPermissions = 32;
constexpr uint64_t MaxPublishedDeliveries = 64;
constexpr uint64_t MaxConsumedResponses = 64;
constexpr uint64_t MaxTotalMemories = 128;
constexpr uint64_t MaxMemoriesPerAgent = 32;
constexpr uint64_t MemoryBaseMetadataBytes = 48;
constexpr uint64_t MemoryAudienceIdBytes = 8;

enum class ELifecycle : uint8_t
{
	Open,
	Paused,
	Replaced,
	Cancelled,
	Consumed,
	Expired
};

enum class EChoiceKind : uint8_t
{
	SurvivalAction,
	PrivateTalk,
	GroupTalk
};

enum class EMemoryKind : uint8_t
{
	Observation,
	SpeakerClaim,
	Inference
};

enum class EGodStatus : uint8_t
{
	Queued,
	Acknowledged
};

enum class EFallbackReason : uint8_t
{
	InvalidProposal,
	Offline,
	Timeout,
	Cancelled
};

struct FOfferedChoice
{
	uint64_t ChoiceId = 0;
	EChoiceKind Kind = EChoiceKind::SurvivalAction;
	EActionKind ActionKind = EActionKind::None;
	uint64_t ConversationId = 0;
	std::vector<uint64_t> MembershipSnapshot;
};

struct FRequestTicket
{
	uint64_t RequestId = 0;
	uint64_t AgentId = 0;
	uint64_t Epoch = 0;
	uint64_t AttemptId = 0;
	uint64_t SourceTick = 0;
	uint64_t DeadlineTick = 0;
	ELifecycle Lifecycle = ELifecycle::Open;
	std::vector<FOfferedChoice> Offered;
};

struct FProposal
{
	uint32_t ContractVersion = 0;
	uint64_t RequestId = 0;
	uint64_t AgentId = 0;
	uint64_t Epoch = 0;
	uint64_t AttemptId = 0;
	uint64_t ChoiceId = 0;
	std::string Text;
	std::string IntentSummary;
};

struct FConversation
{
	uint64_t Id = 0;
	EChoiceKind Kind = EChoiceKind::PrivateTalk;
	std::vector<uint64_t> Members;
};

struct FAudienceSnapshot
{
	std::vector<uint64_t> AgentIds;
};

struct FResponseKey
{
	uint64_t RequestId = 0;
	uint64_t AttemptId = 0;
};

struct FCurrentPermission
{
	uint64_t AgentId = 0;
	EChoiceKind Kind = EChoiceKind::SurvivalAction;
	EActionKind ActionKind = EActionKind::None;
	uint64_t ConversationId = 0;
};

struct FDeliveryCandidate
{
	uint32_t ContractVersion = 0;
	uint64_t RequestId = 0;
	uint64_t AgentId = 0;
	uint64_t Epoch = 0;
	uint64_t AttemptId = 0;
	uint64_t OfferedChoiceId = 0;
	uint64_t ConversationId = 0;
	EChoiceKind Kind = EChoiceKind::SurvivalAction;
	EActionKind ActionKind = EActionKind::None;
	std::string Text;
	std::string IntentSummary;
	FAudienceSnapshot Audience;
};

struct FMemoryRecord
{
	uint64_t Id = 0;
	uint64_t OwnerAgentId = 0;
	EMemoryKind Kind = EMemoryKind::Observation;
	uint64_t SourceRequestId = 0;
	uint64_t SourceAttemptId = 0;
	FAudienceSnapshot Audience;
	std::string Content;
};

struct FDerivedMemoryCandidate
{
	uint64_t OwnerAgentId = 0;
	EMemoryKind Kind = EMemoryKind::Inference;
	uint64_t SourceRequestId = 0;
	uint64_t SourceAttemptId = 0;
	FAudienceSnapshot Audience;
	std::string Content;
};

struct FGodRequest
{
	uint64_t Id = 0;
	uint64_t RequesterId = 0;
	std::string Text;
	EGodStatus Status = EGodStatus::Queued;
};

struct FFallback
{
	EFallbackReason Reason = EFallbackReason::InvalidProposal;
};

struct FAdmissionWorld
{
	uint64_t CurrentTick = 0;
	uint64_t CurrentEpoch = 0;
	std::vector<FCurrentPermission> CurrentPermissions;
	std::vector<FConversation> Conversations;
};

struct FPublicationLedger
{
	bool bFailNextCommit = false;
	std::vector<FResponseKey> ConsumedResponses;
	std::vector<FDeliveryCandidate> Published;
	std::vector<FMemoryRecord> Memories;
	uint64_t NextMemoryId = 1;

	bool TryCommit(const FRequestTicket& Ticket, const FProposal& Proposal,
		const FAdmissionWorld& World, std::string& Error);
};

bool ByteLengthOk(const std::string& Text, uint64_t MaxBytes);
bool AgentInAudience(uint64_t AgentId, const FAudienceSnapshot& Audience);
bool ValidateMemoryRecord(const FMemoryRecord& Memory, std::string& Error);
uint64_t MemoryBudgetBytes(const FMemoryRecord& Memory);
bool ValidateProposal(const FRequestTicket& Ticket, const FProposal& Proposal,
	const FAdmissionWorld& World, FDeliveryCandidate& Out, std::string& Error);
std::vector<FMemoryRecord> ContextForAgent(uint64_t AgentId,
	const std::vector<FMemoryRecord>& Memories);
std::vector<FDeliveryCandidate> ManagerTranscript(
	const std::vector<FDeliveryCandidate>& Published);
bool DeriveAccessPreservingMemory(uint64_t AgentId, const FMemoryRecord& Source,
	const std::string& DerivedText, FDerivedMemoryCandidate& Out, std::string& Error);
bool AdmitGodRequest(uint64_t NextId, uint64_t TrustedRequesterId, const std::string& Text,
	FGodRequest& Out, std::string& Error);
FFallback HandBackToSurvival(EFallbackReason Reason);

}
}
