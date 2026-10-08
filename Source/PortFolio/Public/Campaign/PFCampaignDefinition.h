#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PFCampaignDefinition.generated.h"

UENUM(BlueprintType)
enum class EPFCampaignObjective : uint8 { Reach, Defeat, Interact, Rally };

UENUM(BlueprintType)
enum class EPFCampaignPhase : uint8 { Preparing, Playing, Camera, Resetting, Traveling, Complete, Fault, Starting };

USTRUCT(BlueprintType)
struct FPFCampaignEnemy
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Anchor;
	UPROPERTY(EditAnywhere) bool bKwang = true;
	UPROPERTY(EditAnywhere) bool bCommander = false;
	UPROPERTY(EditAnywhere) bool bFinalBoss = false;
	UPROPERTY(EditAnywhere) TArray<FName> Patrol;
};

USTRUCT(BlueprintType)
struct FPFCampaignEncounter
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Id;
	UPROPERTY(EditAnywhere) FName Trigger;
	UPROPERTY(EditAnywhere) int32 FirstStep = 0;
	UPROPERTY(EditAnywhere) int32 LastStep = 0;
	UPROPERTY(EditAnywhere) float TriggerRadius = 1400.f;
	UPROPERTY(EditAnywhere) TArray<FPFCampaignEnemy> Enemies;
	UPROPERTY(EditAnywhere) TArray<FPFCampaignEnemy> Reinforcements;
};

USTRUCT(BlueprintType)
struct FPFCampaignStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) EPFCampaignObjective Type = EPFCampaignObjective::Reach;
	UPROPERTY(EditAnywhere) FText Objective;
	UPROPERTY(EditAnywhere) FText Radio;
	UPROPERTY(EditAnywhere) FName Target;
	UPROPERTY(EditAnywhere) TArray<FName> RequiredEncounters;
	UPROPERTY(EditAnywhere) bool bCheckpoint = false;
	UPROPERTY(EditAnywhere) FName Checkpoint;
	UPROPERTY(EditAnywhere) FName Shot;
	UPROPERTY(EditAnywhere) TArray<FName> Waypoints;
};

USTRUCT(BlueprintType)
struct FPFCampaignSupply
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Anchor;
	UPROPERTY(EditAnywhere) FText Record;
};

// 챕터 목표, 전투, 연출 정의
UCLASS(BlueprintType)
class PORTFOLIO_API UPFCampaignDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) int32 Chapter = 3;
	UPROPERTY(EditAnywhere) FText ChapterTitle;
	UPROPERTY(EditAnywhere) FName StartCheckpoint = TEXT("Start");
	UPROPERTY(EditAnywhere) FName IntroShot;
	UPROPERTY(EditAnywhere) FText IntroRadio;
	UPROPERTY(EditAnywhere) TArray<FPFCampaignStep> Steps;
	UPROPERTY(EditAnywhere) TArray<FPFCampaignEncounter> Encounters;
	UPROPERTY(EditAnywhere) TArray<FPFCampaignSupply> Supplies;
	UFUNCTION(CallInEditor, Category="Campaign")
	void SetChapterDefaults();
	static FString GetAssetPath(int32 Chapter);
};
