#include "AIityRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace AIity
{
namespace
{
constexpr int32_t NeedMaximum = 1000;
constexpr size_t DisplayHistoryLimit = 64;
constexpr uint64_t SignedStorageMaximum =
	static_cast<uint64_t>(std::numeric_limits<int64_t>::max());

int32_t ClampNeed(int32_t Value)
{
	return std::max(0, std::min(NeedMaximum, Value));
}

FResourceNode* FindResource(FWorldState& State, EResourceKind Kind, uint64_t Id = 0)
{
	for (FResourceNode& Resource : State.Resources)
	{
		if (Resource.Kind == Kind && (Id == 0 || Resource.Id == Id))
		{
			return &Resource;
		}
	}
	return nullptr;
}

FFounder* FindFounder(FWorldState& State, uint64_t Id)
{
	for (FFounder& Founder : State.Founders)
	{
		if (Founder.Id == Id)
		{
			return &Founder;
		}
	}
	return nullptr;
}

void ClearAction(FFounder& Founder)
{
	Founder.Action = FAction{};
}

const char* ActionName(EActionKind Kind)
{
	switch (Kind)
	{
	case EActionKind::GatherFood: return "gather food";
	case EActionKind::GatherWater: return "gather water";
	case EActionKind::Eat: return "eat";
	case EActionKind::Drink: return "drink";
	case EActionKind::Rest: return "rest";
	default: return "wait";
	}
}

bool IsGatherAction(EActionKind Kind)
{
	return Kind == EActionKind::GatherFood || Kind == EActionKind::GatherWater;
}
}

FWorldState FRules::CreateFoundationWorld(uint64_t Seed)
{
	struct FFounderSeed
	{
		const char* Name;
		const char* Sex;
		int32_t Age;
		const char* Appearance;
		int32_t Curiosity;
		int32_t Sociability;
		int32_t Patience;
		int32_t Risk;
		int32_t Generosity;
		const char* Preference;
	};

	const std::array<FFounderSeed, 10> Seeds = {{
		{"Aru", "man", 31, "tall, umber skin, braided black hair", 720, 460, 610, 540, 690, "stone walking"},
		{"Bram", "man", 27, "broad, brown skin, close dark curls", 430, 710, 520, 670, 580, "river fishing"},
		{"Cadan", "man", 35, "lean, ochre skin, long wavy hair", 650, 390, 820, 310, 740, "tool shaping"},
		{"Daro", "man", 23, "compact, dark skin, short coiled hair", 810, 630, 380, 760, 420, "valley scouting"},
		{"Ennar", "man", 29, "sturdy, bronze skin, shoulder-length hair", 510, 560, 730, 450, 800, "camp tending"},
		{"Fara", "woman", 26, "tall, brown skin, long braided hair", 760, 680, 470, 590, 710, "plant gathering"},
		{"Gila", "woman", 33, "strong, deep brown skin, cropped curls", 440, 520, 850, 390, 770, "wood collecting"},
		{"Hessa", "woman", 21, "slight, copper skin, wavy dark bob", 880, 740, 330, 720, 610, "bird watching"},
		{"Ilya", "woman", 30, "athletic, olive skin, thick black braid", 590, 410, 690, 640, 480, "river swimming"},
		{"Jora", "woman", 24, "round-faced, dark skin, beaded twists", 670, 830, 560, 370, 860, "story sharing"}
	}};

	FWorldState State;
	State.Seed = Seed;
	State.RandomState = Seed == 0 ? 0x9e3779b97f4a7c15ULL : Seed;
	const auto NextRandom = [&State]()
	{
		State.RandomState ^= State.RandomState << 13;
		State.RandomState ^= State.RandomState >> 7;
		State.RandomState ^= State.RandomState << 17;
		return State.RandomState;
	};
	const auto VaryTrait = [&NextRandom](int32_t Base)
	{
		return std::max(0, std::min(1000, Base + static_cast<int32_t>(NextRandom() % 41) - 20));
	};
	for (size_t Index = 0; Index < Seeds.size(); ++Index)
	{
		const FFounderSeed& SeedFounder = Seeds[Index];
		FFounder Founder;
		Founder.Id = Index + 1;
		Founder.Name = SeedFounder.Name;
		Founder.Sex = SeedFounder.Sex;
		Founder.AgeYears = SeedFounder.Age;
		Founder.Appearance = SeedFounder.Appearance;
		Founder.Personality = {VaryTrait(SeedFounder.Curiosity), VaryTrait(SeedFounder.Sociability),
			VaryTrait(SeedFounder.Patience), VaryTrait(SeedFounder.Risk), VaryTrait(SeedFounder.Generosity),
			SeedFounder.Preference};
		Founder.X = static_cast<int32_t>((Index % 5) * 140) - 280 + static_cast<int32_t>(NextRandom() % 31) - 15;
		Founder.Y = static_cast<int32_t>((Index / 5) * 180) - 90 + static_cast<int32_t>(NextRandom() % 31) - 15;
		State.Founders.push_back(Founder);
	}
	State.Resources = {
		{101, EResourceKind::Food, 80, 80, -1200, 500},
		{102, EResourceKind::Water, 120, 120, 1200, -450}
	};
	return State;
}

void FRules::AddEvent(FCandidateTick& Candidate, uint64_t AgentId, const char* Type, const std::string& Message)
{
	if (std::string(Type) == "unavailable")
	{
		for (auto It = Candidate.State.History.rbegin(); It != Candidate.State.History.rend(); ++It)
		{
			if (It->AgentId == AgentId)
			{
				if (It->Type == Type && It->Message == Message)
				{
					return;
				}
				break;
			}
		}
	}
	FEvent Event;
	Event.Id = Candidate.State.NextEventId++;
	Event.Tick = Candidate.State.Tick;
	Event.AgentId = AgentId;
	Event.Type = Type;
	Event.Message = Message;
	Candidate.Events.push_back(Event);
	Candidate.State.History.push_back(Event);
	if (Candidate.State.History.size() > DisplayHistoryLimit)
	{
		Candidate.State.History.erase(Candidate.State.History.begin(),
			Candidate.State.History.begin() + (Candidate.State.History.size() - DisplayHistoryLimit));
	}
}

FCandidateTick FRules::BuildEpochCandidate(const FWorldState& Committed,
	const char* EventType, const char* Message)
{
	FCandidateTick Candidate;
	Candidate.State = Committed;
	Candidate.State.Tick++;
	Candidate.State.ExecutionEpoch++;
	for (FFounder& Founder : Candidate.State.Founders)
	{
		if (Founder.Action.AwaitingMovement || IsGatherAction(Founder.Action.Kind))
		{
			Founder.Action.AwaitingMovement = true;
			Founder.Action.Epoch = Candidate.State.ExecutionEpoch;
		}
	}
	AddEvent(Candidate, 0, EventType, Message);
	return Candidate;
}

FCandidateTick FRules::BuildResumeCandidate(const FWorldState& Committed)
{
	return BuildEpochCandidate(Committed, "reopen",
		"World reopened paused; active movement was reissued without offline time.");
}

FCandidateTick FRules::BuildRetryCandidate(const FWorldState& Committed)
{
	return BuildEpochCandidate(Committed, "bridge_reissue",
		"Persistence retry committed a new execution epoch and reissued active movement.");
}

FCandidateTick FRules::BuildCandidate(const FWorldState& Committed, const std::vector<FMovementReceipt>& InputReceipts)
{
	FCandidateTick Candidate;
	Candidate.State = Committed;
	Candidate.State.Tick++;
	Candidate.State.LogicalSeconds++;
	Candidate.State.RandomState ^= Candidate.State.RandomState << 13;
	Candidate.State.RandomState ^= Candidate.State.RandomState >> 7;
	Candidate.State.RandomState ^= Candidate.State.RandomState << 17;

	std::vector<FMovementReceipt> Receipts = InputReceipts;
	std::stable_sort(Receipts.begin(), Receipts.end(), [](const FMovementReceipt& Left, const FMovementReceipt& Right)
	{
		return Left.ReceiptId < Right.ReceiptId;
	});

	for (const FMovementReceipt& Receipt : Receipts)
	{
		FFounder* Founder = FindFounder(Candidate.State, Receipt.AgentId);
		if (Receipt.ReceiptId == 0 || Founder == nullptr || !Founder->Action.AwaitingMovement ||
			Founder->Action.Id != Receipt.ActionId || Founder->Action.Epoch != Receipt.Epoch ||
			Receipt.Epoch != Candidate.State.ExecutionEpoch || Receipt.ReceiptId != Receipt.ActionId ||
			Receipt.Outcome < EMovementOutcome::Arrived || Receipt.Outcome > EMovementOutcome::Failed)
		{
			AddEvent(Candidate, Receipt.AgentId, "movement_rejected", "Rejected stale or impossible movement receipt.");
			continue;
		}

		const int64_t DeltaX = static_cast<int64_t>(Receipt.X) - Founder->Action.TargetX;
		const int64_t DeltaY = static_cast<int64_t>(Receipt.Y) - Founder->Action.TargetY;
		const bool ReachedTarget = DeltaX * DeltaX + DeltaY * DeltaY <= 150LL * 150LL;
		if (Receipt.Outcome == EMovementOutcome::Arrived && !ReachedTarget)
		{
			AddEvent(Candidate, Receipt.AgentId, "movement_rejected", "Rejected arrival outside the target radius.");
			continue;
		}

		Candidate.ConsumedReceiptIds.push_back(Receipt.ReceiptId);
		if (Receipt.Outcome != EMovementOutcome::Arrived)
		{
			AddEvent(Candidate, Founder->Id, "movement_failed", std::string(ActionName(Founder->Action.Kind)) + " movement ended without arrival.");
			ClearAction(*Founder);
			continue;
		}

		Founder->X = Receipt.X;
		Founder->Y = Receipt.Y;
		Founder->Action.AwaitingMovement = false;
		AddEvent(Candidate, Founder->Id, "movement_arrived", Founder->Name + " reached the destination.");
	}

	std::sort(Candidate.State.Founders.begin(), Candidate.State.Founders.end(), [](const FFounder& Left, const FFounder& Right)
	{
		return Left.Id < Right.Id;
	});

	for (FFounder& Founder : Candidate.State.Founders)
	{
		Founder.Needs.Hunger = ClampNeed(Founder.Needs.Hunger + 2);
		Founder.Needs.Thirst = ClampNeed(Founder.Needs.Thirst + 3);
		Founder.Needs.Fatigue = ClampNeed(Founder.Needs.Fatigue + 1);
		if (Founder.Needs.Hunger == NeedMaximum || Founder.Needs.Thirst == NeedMaximum)
		{
			Founder.Needs.Health = ClampNeed(Founder.Needs.Health - 4);
		}

		if (Founder.Action.Kind != EActionKind::None && !Founder.Action.AwaitingMovement)
		{
			FResourceNode* Resource = nullptr;
			switch (Founder.Action.Kind)
			{
			case EActionKind::GatherFood:
				Resource = FindResource(Candidate.State, EResourceKind::Food, Founder.Action.TargetId);
				if (Resource != nullptr && Resource->Amount > 0)
				{
					Resource->Amount--;
					Founder.Food++;
					AddEvent(Candidate, Founder.Id, "gather", Founder.Name + " gathered one portion of food.");
				}
				else
				{
					AddEvent(Candidate, Founder.Id, "unavailable", "No food remained at the gathering patch.");
				}
				break;
			case EActionKind::GatherWater:
				Resource = FindResource(Candidate.State, EResourceKind::Water, Founder.Action.TargetId);
				if (Resource != nullptr && Resource->Amount > 0)
				{
					Resource->Amount--;
					Founder.Water++;
					AddEvent(Candidate, Founder.Id, "gather", Founder.Name + " gathered one portion of water.");
				}
				else
				{
					AddEvent(Candidate, Founder.Id, "unavailable", "No water remained at the river access.");
				}
				break;
			case EActionKind::Eat:
				if (Founder.Food > 0)
				{
					Founder.Food--;
					Founder.Needs.Hunger = ClampNeed(Founder.Needs.Hunger - 420);
					AddEvent(Candidate, Founder.Id, "eat", Founder.Name + " ate one portion of food.");
				}
				break;
			case EActionKind::Drink:
				if (Founder.Water > 0)
				{
					Founder.Water--;
					Founder.Needs.Thirst = ClampNeed(Founder.Needs.Thirst - 500);
					AddEvent(Candidate, Founder.Id, "drink", Founder.Name + " drank one portion of water.");
				}
				break;
			case EActionKind::Rest:
				Founder.Needs.Fatigue = ClampNeed(Founder.Needs.Fatigue - 240);
				Founder.Needs.Stamina = ClampNeed(Founder.Needs.Stamina + 300);
				AddEvent(Candidate, Founder.Id, "rest", Founder.Name + " rested.");
				break;
			default:
				break;
			}
			ClearAction(Founder);
		}

		if (Founder.Action.Kind != EActionKind::None)
		{
			continue;
		}

		if (Founder.Needs.Thirst >= 600 && Founder.Water > 0)
		{
			Founder.Action.Kind = EActionKind::Drink;
		}
		else if (Founder.Needs.Hunger >= 650 && Founder.Food > 0)
		{
			Founder.Action.Kind = EActionKind::Eat;
		}
		else if (Founder.Needs.Fatigue >= 700 || Founder.Needs.Stamina <= 200)
		{
			Founder.Action.Kind = EActionKind::Rest;
		}
		else
		{
			const EResourceKind WantedKind = ((Founder.Id + Candidate.State.Tick + (Candidate.State.RandomState & 1ULL)) % 2 == 0)
				? EResourceKind::Food : EResourceKind::Water;
			FResourceNode* Resource = FindResource(Candidate.State, WantedKind);
			if (Resource == nullptr || Resource->Amount <= 0)
			{
				AddEvent(Candidate, Founder.Id, "unavailable", "No finite gathering resource is available.");
				continue;
			}
			Founder.Action.Kind = WantedKind == EResourceKind::Food ? EActionKind::GatherFood : EActionKind::GatherWater;
			Founder.Action.TargetId = Resource->Id;
			Founder.Action.TargetX = Resource->X;
			Founder.Action.TargetY = Resource->Y;
			Founder.Action.AwaitingMovement = true;
		}

		Founder.Action.Id = Candidate.State.NextActionId++;
		Founder.Action.Epoch = Candidate.State.ExecutionEpoch;
		AddEvent(Candidate, Founder.Id, "action_started", Founder.Name + " started to " + ActionName(Founder.Action.Kind) + ".");
	}

	return Candidate;
}

bool FRules::Validate(const FWorldState& State, std::string& Error)
{
	Error.clear();
	if (State.SchemaVersion != 2 || State.Founders.size() != 10 || State.History.size() > DisplayHistoryLimit)
	{
		Error = "Unsupported schema or founder count.";
		return false;
	}
	if (State.Tick > SignedStorageMaximum || State.LogicalSeconds > SignedStorageMaximum)
	{
		Error = "Tick or LogicalSeconds exceeds SQLite signed storage range.";
		return false;
	}
	std::set<uint64_t> FounderIds;
	for (const FFounder& Founder : State.Founders)
	{
		if (Founder.Id == 0 || !FounderIds.insert(Founder.Id).second ||
			Founder.Food < 0 || Founder.Water < 0 ||
			Founder.Needs.Hunger < 0 || Founder.Needs.Hunger > NeedMaximum ||
			Founder.Needs.Thirst < 0 || Founder.Needs.Thirst > NeedMaximum ||
			Founder.Needs.Fatigue < 0 || Founder.Needs.Fatigue > NeedMaximum ||
			Founder.Needs.Stamina < 0 || Founder.Needs.Stamina > NeedMaximum ||
			Founder.Needs.Health < 0 || Founder.Needs.Health > NeedMaximum)
		{
			Error = "Founder state violates bounds or stable ID rules.";
			return false;
		}
	}
	for (const FResourceNode& Resource : State.Resources)
	{
		if (Resource.Id == 0 || Resource.Amount < 0 || Resource.Amount > Resource.Capacity)
		{
			Error = "Resource state violates finite capacity.";
			return false;
		}
	}
	for (const FEvent& Event : State.History)
	{
		if (Event.Id > SignedStorageMaximum || Event.Tick > SignedStorageMaximum ||
			Event.AgentId > SignedStorageMaximum)
		{
			Error = "Event identity exceeds SQLite signed storage range.";
			return false;
		}
	}
	return true;
}

bool FRules::ValidatePersistenceEnvelope(const FCandidateTick& Candidate, std::string& Error)
{
	if (!Validate(Candidate.State, Error))
	{
		return false;
	}
	for (const FEvent& Event : Candidate.Events)
	{
		if (Event.Id > SignedStorageMaximum || Event.Tick > SignedStorageMaximum ||
			Event.AgentId > SignedStorageMaximum)
		{
			Error = "Candidate Event identity exceeds SQLite signed storage range.";
			return false;
		}
	}
	for (uint64_t ReceiptId : Candidate.ConsumedReceiptIds)
	{
		if (ReceiptId > SignedStorageMaximum)
		{
			Error = "Consumed receipt identity exceeds SQLite signed storage range.";
			return false;
		}
	}
	return true;
}
}
