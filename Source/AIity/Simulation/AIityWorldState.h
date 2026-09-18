#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AIity
{
enum class EResourceKind : uint8_t
{
	Food,
	Water
};

enum class EActionKind : uint8_t
{
	None,
	GatherFood,
	GatherWater,
	Eat,
	Drink,
	Rest
};

enum class EMovementOutcome : uint8_t
{
	Arrived,
	Blocked,
	Interrupted,
	Failed
};

struct FNeeds
{
	int32_t Hunger = 180;
	int32_t Thirst = 140;
	int32_t Fatigue = 100;
	int32_t Stamina = 1000;
	int32_t Health = 1000;
};

struct FPersonality
{
	int32_t Curiosity = 500;
	int32_t Sociability = 500;
	int32_t Patience = 500;
	int32_t RiskTolerance = 500;
	int32_t Generosity = 500;
	std::string PreferredActivity;
};

struct FAction
{
	uint64_t Id = 0;
	uint64_t Epoch = 0;
	EActionKind Kind = EActionKind::None;
	uint64_t TargetId = 0;
	int32_t TargetX = 0;
	int32_t TargetY = 0;
	bool AwaitingMovement = false;
};

struct FFounder
{
	uint64_t Id = 0;
	std::string Name;
	std::string Sex;
	int32_t AgeYears = 0;
	std::string Appearance;
	FPersonality Personality;
	FNeeds Needs;
	int32_t Food = 2;
	int32_t Water = 2;
	int32_t X = 0;
	int32_t Y = 0;
	FAction Action;
};

struct FResourceNode
{
	uint64_t Id = 0;
	EResourceKind Kind = EResourceKind::Food;
	int32_t Amount = 0;
	int32_t Capacity = 0;
	int32_t X = 0;
	int32_t Y = 0;
};

struct FEvent
{
	uint64_t Id = 0;
	uint64_t Tick = 0;
	uint64_t AgentId = 0;
	std::string Type;
	std::string Message;
};

struct FMovementReceipt
{
	uint64_t ReceiptId = 0;
	uint64_t AgentId = 0;
	uint64_t ActionId = 0;
	uint64_t Epoch = 0;
	EMovementOutcome Outcome = EMovementOutcome::Failed;
	int32_t X = 0;
	int32_t Y = 0;
};

struct FWorldState
{
	uint32_t SchemaVersion = 2;
	uint64_t Seed = 0;
	uint64_t RandomState = 0;
	uint64_t Tick = 0;
	uint64_t LogicalSeconds = 0;
	uint64_t ExecutionEpoch = 1;
	uint64_t NextActionId = 1;
	uint64_t NextEventId = 1;
	std::vector<FFounder> Founders;
	std::vector<FResourceNode> Resources;
	std::vector<FEvent> History;
};

struct FCandidateTick
{
	FWorldState State;
	std::vector<FEvent> Events;
	std::vector<uint64_t> ConsumedReceiptIds;
};
}
