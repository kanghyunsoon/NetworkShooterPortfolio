// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NetworkShooter : ModuleRules
{
	public NetworkShooter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});
	}
}
