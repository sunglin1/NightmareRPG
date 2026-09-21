// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NightmareRPG : ModuleRules
{
	public NightmareRPG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"NightmareRPG",
			"NightmareRPG/Variant_Platforming",
			"NightmareRPG/Variant_Platforming/Animation",
			"NightmareRPG/Variant_Combat",
			"NightmareRPG/Variant_Combat/AI",
			"NightmareRPG/Variant_Combat/Animation",
			"NightmareRPG/Variant_Combat/Gameplay",
			"NightmareRPG/Variant_Combat/Interfaces",
			"NightmareRPG/Variant_Combat/UI",
			"NightmareRPG/Variant_SideScrolling",
			"NightmareRPG/Variant_SideScrolling/AI",
			"NightmareRPG/Variant_SideScrolling/Gameplay",
			"NightmareRPG/Variant_SideScrolling/Interfaces",
			"NightmareRPG/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
