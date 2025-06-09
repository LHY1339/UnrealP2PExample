// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealP2PExample : ModuleRules
{
	public UnrealP2PExample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] 
		{
			"Core", 
			"CoreUObject",
			"Engine",
			"InputCore",
			"HeadMountedDisplay",
			"Sockets",
			"Networking",
			"OnlineSubsystemUtils"
		});
	}
}
