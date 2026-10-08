#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Campaign/PFCampaignDefinition.h"
#include "System/Framework/PFPlayerState.h"
#include "Character/PFCharacterControlTypes.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "PFCampaignSubsystem.generated.h"

USTRUCT()
struct FPFCampaignPlayerSnapshot
{
	GENERATED_BODY()
	UPROPERTY() ECHARACTER Character = CHARACTER_TWINBLAST;
	UPROPERTY() TArray<FPFInventorySlot> Inventory;
	UPROPERTY() TArray<int32> QuickSlots;
	UPROPERTY() FPFCharacterSharedStateSnapshot View;
	UPROPERTY() FPFStatValues Stats;
};

// 세션 내 캠페인 진행, 복구 기준
UCLASS()
class PORTFOLIO_API UPFCampaignSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static UPFCampaignSubsystem* Get(const UObject* Context);
	static bool IsCampaign(const UWorld* World);
	static FString PlayerKey(const APFPlayerState* Player);
	void ResetRun();
	void EnterChapter(int32 InChapter);
	void CapturePlayer(APFPlayerState* Player);
	bool RestorePlayer(APFPlayerState* Player);
	UPROPERTY() int32 Chapter = 3;
	UPROPERTY() int32 CheckpointStep = 0;
	UPROPERTY() FName Checkpoint = TEXT("Start");
	UPROPERTY() TMap<FString, FPFCampaignPlayerSnapshot> Players;
	UPROPERTY() TSet<FName> CompletedEncounters;
	UPROPERTY() TSet<FString> SupplyClaims;
	UPROPERTY() TSet<FString> Records;
	UPROPERTY() TSet<FString> CheckpointRecords;
	UPROPERTY() int32 Deaths = 0;
	UPROPERTY() double Elapsed = 0.;
};
