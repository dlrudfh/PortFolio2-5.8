#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFTutorialManager.generated.h"

class APFCharacter;
class APFPlayerController;
class APFPlayerState;
class UPFInventoryWidget;
class UPFStatWidget;
class UPFTutorialWidget;
class UWidget;

// 맵의 튜토리얼 진행, 훈련 봇 관리
UCLASS()
class PORTFOLIO_API APFTutorialManager : public AActor
{
	GENERATED_BODY()

public:
	APFTutorialManager();
	static APFTutorialManager* Find(UWorld* World);
	void NotifyChestState(APFPlayerController* Controller, FName ItemName, const FVector& ItemLocation, bool bCollected);
	void NotifyQuickSlotUse(APFPlayerController* Controller, int32 ItemID, int32 CountBeforeUse);
	void PrepareCombat(APFPlayerController* Controller);
	void NotifyBotState(APFPlayerController* Controller, APFCharacter* Bot, bool bDefeated);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class EStep : uint8 { Move, Jump, Sprint, View, Attack, Ultimate, Chest, Inventory, KwangBot, Stats, Finished };
	static constexpr int32 StepCount = static_cast<int32>(EStep::Finished);

	struct FLocalProgress
	{
		TWeakObjectPtr<APFPlayerController> Controller;
		TWeakObjectPtr<UPFTutorialWidget> Guide;
		TWeakObjectPtr<UPFInventoryWidget> InventoryWindow;
		TWeakObjectPtr<UPFStatWidget> StatWindow;
		TWeakObjectPtr<UWidget> HighlightWidget;
		TWeakObjectPtr<APFCharacter> ObservedCharacter;
		TWeakObjectPtr<APFCharacter> TutorialBot;
		TWeakObjectPtr<AActor> MarkerActor;
		FString HighlightCaption;
		FString MarkerCaption;
		FVector MarkerLocation = FVector::ZeroVector;
		FVector PreviousPosition = FVector::ZeroVector;
		EStep Step = EStep::Move;
		uint8 StepFlags = 0;
		int32 InitialViewMode = 0;
		TMap<int32, int32> RequestedItemCounts;
		TSet<FName> ChestRewards;
		float ActionTime = 0.f;
		float TravelDistance = 0.f;
		float TargetRefreshRemaining = 0.f;
		float BotRequestRemaining = 0.f;
		bool bIntroFinished = false;
		bool bInitialized = false;
		bool bPreviousViewFixed = false;
		bool bWasAttacking = false;
		bool bHasMovementTarget = false;
		bool bTutorialBotDefeated = false;
		bool bBotSpawnFailed = false;
	};

	struct FCombatState
	{
		TWeakObjectPtr<APFCharacter> Bot;
		bool bDefeated = false;
	};

	void UpdatePlayer(APFPlayerController* Controller, FLocalProgress& State, float DeltaTime);
	void BeginStep(FLocalProgress& State, APFCharacter* Character);
	void UpdateStep(FLocalProgress& State, APFCharacter* Character, APFPlayerState* PlayerState, float DeltaTime);
	void CompleteStep(FLocalProgress& State);
	void UpdateGuideText(FLocalProgress& State, APFCharacter* Character);
	void ReturnToTitle();
	bool FindMovementTarget(FLocalProgress& State, APFCharacter* Character);
	void FindNearbyTarget(FLocalProgress& State, APFCharacter* Character);
	void HandleBotDied(APFCharacter* Bot);
	void ReleaseCombat(FCombatState& Combat);

	// 로컬 플레이어별 진행 상태
	TMap<TWeakObjectPtr<APFPlayerController>, FLocalProgress> LocalProgress;
	// 서버의 플레이어별 전투 대상
	TMap<TWeakObjectPtr<APFPlayerController>, FCombatState> CombatStates;
	FTimerHandle TitleReturnTimer;
};
