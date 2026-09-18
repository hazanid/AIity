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
	}
}
