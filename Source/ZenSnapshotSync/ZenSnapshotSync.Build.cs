using UnrealBuildTool;

public class ZenSnapshotSync : ModuleRules
{
	public ZenSnapshotSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Json",
			"Zen",
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Slate",
			"SlateCore",
			"TargetPlatform",
			"ToolMenus",
		});
	}
}