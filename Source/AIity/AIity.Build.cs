using UnrealBuildTool;

public class AIity : ModuleRules
{
	public AIity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "InputCore", "SQLiteCore"
		});
		PrivateDependencyModuleNames.AddRange(new[] {
			"AIModule", "ApplicationCore", "Projects"
		});
		// The windowless native selection test owns an FSceneViewport render resource.
		if (!Target.bForceDisableAutomationTests &&
			(Target.bForceCompileDevelopmentAutomationTests ||
			 (Target.Configuration != UnrealTargetConfiguration.Test &&
			  Target.Configuration != UnrealTargetConfiguration.Shipping)))
		{
			PrivateDependencyModuleNames.Add("RenderCore");
		}
	}
}
