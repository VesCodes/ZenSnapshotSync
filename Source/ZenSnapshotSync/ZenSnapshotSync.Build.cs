using UnrealBuildTool;

public class ZenSnapshotSync : ModuleRules
{
	public ZenSnapshotSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Zen",
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Json",
		});
	}
}