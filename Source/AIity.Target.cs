using UnrealBuildTool;
using System.Collections.Generic;

public class AIityTarget : TargetRules
{
	public AIityTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AIity");
	}
}
