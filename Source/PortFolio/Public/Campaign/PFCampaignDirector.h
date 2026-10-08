#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/PFCampaignDefinition.h"
#include "PFCampaignDirector.generated.h"

class APFCampaignAnchor;
class APFCampaignShot;
class APFCharacter;
class APFPlayerController;
class UPFCampaignSubsystem;

USTRUCT()
struct FPFCampaignProgress
{
	GENERATED_BODY()
	UPROPERTY() EPFCampaignPhase Phase = EPFCampaignPhase::Preparing;
	UPROPERTY() int32 Step = 0;
	UPROPERTY() int32 RemainingEnemies = 0;
	UPROPERTY() int32 Gathered = 0;
	UPROPERTY() int32 Living = 0;
	UPROPERTY() int32 SkipVotes = 0;
	UPROPERTY() int32 Viewers = 0;
	UPROPERTY() FName Shot;
	UPROPERTY() double PhaseStarted = 0.;
	UPROPERTY() double Elapsed = 0.;
	UPROPERTY() int32 Deaths = 0;
	UPROPERTY() int32 Records = 0;
	UPROPERTY() FText Radio;
	UPROPERTY() FText Error;
	UPROPERTY() TArray<FString> SupplyClaims;
	UPROPERTY() TObjectPtr<APFCharacter> Boss;
};

// 서버 캠페인 진행, 파티 복구, 연출 상태
UCLASS()
class PORTFOLIO_API APFCampaignDirector : public AActor
{
	GENERATED_BODY()
public:
	APFCampaignDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	static APFCampaignDirector* Find(const UWorld* World);
	static bool BlocksInput(const AController* Controller);
	static bool AreFriendly(const APFCharacter* Source, const APFCharacter* Target);
	bool FindPlayerStart(APFPlayerController* Player, FTransform& Result);
	bool DeferJoiningPlayer(APFPlayerController* Player);
	void HandlePlayerDeath(APFPlayerController* Player);
	void Interact(APFPlayerController* Player);
	void VoteSkip(APFPlayerController* Player);
	bool GetObjectiveLocation(APFPlayerController* Player, FVector& Result);
	APFCampaignShot* GetCurrentShot() const;
	APFCampaignAnchor* Anchor(FName Id) const;
	double ServerTime() const;
	bool IsCombatActive() const;
	bool IsEncounterPaused() const;
	FText GetObjectiveText() const;
	FText GetInteractionText(const APFPlayerController* Player) const;
	UPROPERTY(EditAnywhere, Replicated, Category="Campaign") TObjectPtr<UPFCampaignDefinition> Definition;
	UPROPERTY(Replicated) FPFCampaignProgress Progress;

protected:
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	struct FEncounterRuntime
	{
		TArray<TWeakObjectPtr<APFCharacter>> Enemies;
		int32 Players = 1;
		int32 Remaining = 0;
		bool bReinforced = false;
	};
	bool ValidateLayout();
	void Fail(const FString& Reason);
	void UpdatePlayers(const TArray<APFPlayerController*>& Members);
	void UpdateEncounters(const TArray<APFPlayerController*>& Members);
	bool SpawnEncounter(const FPFCampaignEncounter& Definition);
	bool SpawnWave(const FPFCampaignEncounter& Definition, const TArray<FPFCampaignEnemy>& Wave, FEncounterRuntime& Runtime);
	void EnemyDied(APFCharacter* Enemy);
	void ClearEncounters();
	void Advance();
	void StartShot(FName Id, bool bAdvanceAfter);
	void FinishShot();
	bool PartyReady(FName Target) const;
	bool CanReach(const APFPlayerController* Player, const APFCampaignAnchor* Target) const;
	const FPFCampaignStep* FindInteractionStep(const APFPlayerController* Player) const;
	const FPFCampaignSupply* FindSupply(const APFPlayerController* Player, FString& Claim) const;
	bool SaveCheckpoint(FName Id, int32 NextStep);
	bool RestoreParty(bool bRollback, bool bRespawnAll = false);
	void StartStage();
	void ResetCheckpoint();
	void FinishChapter();
	void RefreshProps();
	TArray<APFPlayerController*> Party() const;
	UPROPERTY(Transient) TObjectPtr<UPFCampaignSubsystem> Run;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<APFCampaignAnchor>> Anchors;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<APFCampaignShot>> Shots;
	TMap<FName, FEncounterRuntime> Encounters;
	TSet<FName> Completed;
	TSet<FString> Claims;
	TSet<TWeakObjectPtr<APFPlayerController>> Voters;
	TSet<TWeakObjectPtr<APFPlayerController>> RegisteredPlayers;
	TSet<TWeakObjectPtr<APFCharacter>> CountedDeaths;
	TMap<TWeakObjectPtr<APFPlayerController>, FIntPoint> RouteProgress;
	TWeakObjectPtr<UPFCampaignDefinition> VisualDefinition;
	int32 VisualStep = INDEX_NONE;
	FName ActiveCheckpoint = TEXT("Start");
	bool bAdvanceAfterShot = false;
	bool bStageSpawned = false;
	bool bRollbackPending = false;
	bool bReady = false;
};
