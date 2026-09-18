#pragma once

#include "AIityWorldState.h"

namespace AIity
{
class FRules
{
public:
	static FWorldState CreateFoundationWorld(uint64_t Seed);
	static FCandidateTick BuildResumeCandidate(const FWorldState& Committed);
	static FCandidateTick BuildRetryCandidate(const FWorldState& Committed);
	static FCandidateTick BuildCandidate(const FWorldState& Committed, const std::vector<FMovementReceipt>& Receipts);
	static bool Validate(const FWorldState& State, std::string& Error);
	static bool ValidatePersistenceEnvelope(const FCandidateTick& Candidate, std::string& Error);

private:
	static void AddEvent(FCandidateTick& Candidate, uint64_t AgentId, const char* Type, const std::string& Message);
	static FCandidateTick BuildEpochCandidate(const FWorldState& Committed,
		const char* EventType, const char* Message);
};
}
