using UnrealBuildTool;
using System.Collections.Generic;

public class AIityEditorTarget : TargetRules
{
	public AIityEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AIity");
	}
}
