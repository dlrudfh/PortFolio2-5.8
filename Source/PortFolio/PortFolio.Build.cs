// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// 게임 모듈 빌드 설정
public class PortFolio : ModuleRules
{
	public PortFolio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AIModule", "InputCore", "Niagara", "UMG", "Slate", "SlateCore", "GameplayAbilities", "GameplayTags", "GameplayTasks", "EnhancedInput", "OnlineSubsystem", "OnlineSubsystemSteam" });
	}
}
