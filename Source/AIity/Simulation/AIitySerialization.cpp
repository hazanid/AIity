#include "AIitySerialization.h"

#include "AIityRules.h"

#include <iomanip>
#include <sstream>

namespace AIity
{
std::string SerializeWorld(const FWorldState& State)
{
	std::ostringstream Out;
	Out << "AIITY " << State.SchemaVersion << '\n';
	Out << State.Seed << ' ' << State.RandomState << ' ' << State.Tick << ' ' << State.LogicalSeconds << ' '
		<< State.ExecutionEpoch << ' ' << State.NextActionId << ' ' << State.NextEventId << '\n';
	Out << State.Founders.size() << '\n';
	for (const FFounder& Founder : State.Founders)
	{
		Out << Founder.Id << ' ' << std::quoted(Founder.Name) << ' ' << std::quoted(Founder.Sex) << ' '
			<< Founder.AgeYears << ' ' << std::quoted(Founder.Appearance) << ' '
			<< Founder.Personality.Curiosity << ' ' << Founder.Personality.Sociability << ' '
			<< Founder.Personality.Patience << ' ' << Founder.Personality.RiskTolerance << ' '
			<< Founder.Personality.Generosity << ' ' << std::quoted(Founder.Personality.PreferredActivity) << ' '
			<< Founder.Needs.Hunger << ' ' << Founder.Needs.Thirst << ' ' << Founder.Needs.Fatigue << ' '
			<< Founder.Needs.Stamina << ' ' << Founder.Needs.Health << ' '
			<< Founder.Food << ' ' << Founder.Water << ' ' << Founder.X << ' ' << Founder.Y << ' '
			<< Founder.Action.Id << ' ' << Founder.Action.Epoch << ' ' << static_cast<int32_t>(Founder.Action.Kind) << ' '
			<< Founder.Action.TargetId << ' ' << Founder.Action.TargetX << ' ' << Founder.Action.TargetY << ' '
			<< Founder.Action.AwaitingMovement << '\n';
	}
	Out << State.Resources.size() << '\n';
	for (const FResourceNode& Resource : State.Resources)
	{
		Out << Resource.Id << ' ' << static_cast<int32_t>(Resource.Kind) << ' ' << Resource.Amount << ' '
			<< Resource.Capacity << ' ' << Resource.X << ' ' << Resource.Y << '\n';
	}
	Out << State.History.size() << '\n';
	for (const FEvent& Event : State.History)
	{
		Out << Event.Id << ' ' << Event.Tick << ' ' << Event.AgentId << ' ' << std::quoted(Event.Type) << ' '
			<< std::quoted(Event.Message) << '\n';
	}
	return Out.str();
}

bool DeserializeWorld(const std::string& Text, FWorldState& State, std::string& Error)
{
	Error.clear();
	std::istringstream In(Text);
	std::string Magic;
	uint32_t SchemaVersion = 0;
	if (!(In >> Magic >> SchemaVersion) || Magic != "AIITY" || SchemaVersion != 2)
	{
		Error = "Save has an unsupported schema.";
		return false;
	}

	FWorldState Loaded;
	Loaded.SchemaVersion = SchemaVersion;
	size_t Count = 0;
	if (!(In >> Loaded.Seed >> Loaded.RandomState >> Loaded.Tick >> Loaded.LogicalSeconds >>
		Loaded.ExecutionEpoch >> Loaded.NextActionId >> Loaded.NextEventId >> Count) || Count != 10)
	{
		Error = "Save header or founder count is invalid.";
		return false;
	}
	for (size_t Index = 0; Index < Count; ++Index)
	{
		FFounder Founder;
		int32_t ActionKind = 0;
		if (!(In >> Founder.Id >> std::quoted(Founder.Name) >> std::quoted(Founder.Sex) >> Founder.AgeYears >>
			std::quoted(Founder.Appearance) >> Founder.Personality.Curiosity >> Founder.Personality.Sociability >>
			Founder.Personality.Patience >> Founder.Personality.RiskTolerance >> Founder.Personality.Generosity >>
			std::quoted(Founder.Personality.PreferredActivity) >> Founder.Needs.Hunger >> Founder.Needs.Thirst >>
			Founder.Needs.Fatigue >> Founder.Needs.Stamina >> Founder.Needs.Health >> Founder.Food >> Founder.Water >>
			Founder.X >> Founder.Y >> Founder.Action.Id >> Founder.Action.Epoch >> ActionKind >>
			Founder.Action.TargetId >> Founder.Action.TargetX >> Founder.Action.TargetY >> Founder.Action.AwaitingMovement))
		{
			Error = "Founder record is truncated.";
			return false;
		}
		if (ActionKind < 0 || ActionKind > static_cast<int32_t>(EActionKind::Rest))
		{
			Error = "Founder action is invalid.";
			return false;
		}
		Founder.Action.Kind = static_cast<EActionKind>(ActionKind);
		Loaded.Founders.push_back(Founder);
	}

	if (!(In >> Count))
	{
		Error = "Resource count is missing.";
		return false;
	}
	for (size_t Index = 0; Index < Count; ++Index)
	{
		FResourceNode Resource;
		int32_t Kind = 0;
		if (!(In >> Resource.Id >> Kind >> Resource.Amount >> Resource.Capacity >> Resource.X >> Resource.Y) ||
			Kind < 0 || Kind > static_cast<int32_t>(EResourceKind::Water))
		{
			Error = "Resource record is invalid.";
			return false;
		}
		Resource.Kind = static_cast<EResourceKind>(Kind);
		Loaded.Resources.push_back(Resource);
	}

	if (!(In >> Count))
	{
		Error = "History count is missing.";
		return false;
	}
	for (size_t Index = 0; Index < Count; ++Index)
	{
		FEvent Event;
		if (!(In >> Event.Id >> Event.Tick >> Event.AgentId >> std::quoted(Event.Type) >> std::quoted(Event.Message)))
		{
			Error = "History record is truncated.";
			return false;
		}
		Loaded.History.push_back(Event);
	}

	if (!FRules::Validate(Loaded, Error))
	{
		return false;
	}
	State = std::move(Loaded);
	return true;
}
}
