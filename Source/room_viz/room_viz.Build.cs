// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class room_viz : ModuleRules
{
	public room_viz(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput","NavigationSystem","AIModule", "HTTP", "Json", "JsonUtilities", "UMG", "ImageWrapper", "Slate", "SlateCore" });
	}
}
