#pragma once

#include "AIController.h"
#include "Character/PFCharacterControlTypes.h"
#include "Character/PFCombatAimProvider.h"
#include "GameplayTagContainer.h"
#include "PFEnemyAIController.generated.h"

class APFCharacter;
class UAbilitySystemComponent;
class UPFNavLinkProxy;

// 적 탐색, 경로 이동, 공격 판단
UCLASS()
class PORTFOLIO_API APFEnemyAIController : public AAIController, public IPFCombatAimProvider
{
	GENERATED_BODY()

public:
	APFEnemyAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaTime) override;
	virtual bool TryGetCombatAim(FVector& OutAimPoint) override;
	float GetAimPitch() const;
	bool IsPathJumpBlocked() const;
	void GetExcludedJumpLinks(TSet<NavNodeRef>& OutLinks) const;
	void BeginNavigationJump(UPFNavLinkProxy* Link, const FVector& Destination);
	void HandleNavigationJumpFinished(UPFNavLinkProxy* Link);

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
	void HandleCharacterDied(APFCharacter* ControlledPawn);
	void HandleDeathAnimationEnd(APFCharacter* ControlledPawn);
	bool HasClearSightToTarget(const APFCharacter* Target) const;
	void AcquireNearestPlayerTarget();

	bool IsPlayerTargetValid(const APFCharacter* Candidate) const;

	float GetTargetSurfaceDistance(const APFCharacter* Candidate) const;

	virtual bool ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const;
	virtual bool ShouldApproachTarget(const APFCharacter* Target, float SurfaceDistance) const;

	void UpdateEnemyMovement(APFCharacter* Target, float SurfaceDistance, float DeltaTime);

	bool RefreshMovementPath(const APFCharacter* Target);

	void ClearMovementPath();
	void RequestNavigationRefresh();
	void WatchMovementProgress(float DeltaTime);
	void HandleCharacterLanded(APFCharacter* LandedCharacter);
	void HandleCharacterReady(APFCharacter* ReadyCharacter);
	void HandleNavigationTagChanged(const FGameplayTag Tag, int32 NewCount);
	void UnbindNavigationTags();
	void RestoreJumpMovement();
	void LaunchNavigationJump();
	void FailNavigationJump();
	NavNodeRef FindJumpLinkRef(const FVector& Destination) const;
	bool IsTargetAtMovementHeight(const APFCharacter* Target) const;

	void RotateEnemyTowards(const FVector& WorldDirection, float DeltaTime);

	void SetEnemyDirection(EPFDirection NewDirection);

	void ClearEnemyIntent();


protected:
	// 추적 대상 플레이어
	UPROPERTY(Transient)
	TWeakObjectPtr<APFCharacter> TargetCharacter;

	FPFCharacterAISettings CombatSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.05"))
	float TargetRefreshInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.01"))
	float AttackCommandInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "0.05"))
	float PathTargetCheckInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "1.0"))
	float PathTargetRefreshDistance = 50.f;

private:
	// 조종 중인 캐릭터
	UPROPERTY(Transient)
	TWeakObjectPtr<APFCharacter> ControlledCharacter;

	float TargetRefreshTimeRemaining = 0.f;

	float AttackCommandTimeRemaining = 0.f;

	// 현재 엔진 이동 요청
	FAIRequestID ActiveMoveRequest = FAIRequestID::InvalidRequest;

	// 동기 이동 요청의 검증된 출발점
	TOptional<FVector> PathRequestStart;

	FVector LastRequestedTarget = FVector::ZeroVector;
	FVector ProgressOrigin = FVector::ZeroVector;
	float TargetCheckTimeRemaining = 0.f;
	float NextPathRequestTime = 0.f;
	float NextRetryTime = 0.f;
	float NoProgressTime = 0.f;
	bool bNavigationRefreshRequested = true;
	bool bRetreating = false;
	bool bOutOfNavigation = false;
	bool bWaitingAtPathEnd = false;
	bool bMoveBlockedLastTick = false;
	bool bNavigationAbortPending = false;
	bool bSavedRVOAvoidance = false;
	bool bSavedPhysicsInteraction = false;

	// 봇별 실패 링크 재시도 제한
	TMap<NavNodeRef, float> FailedJumpLinks;

	// 진행 중인 자동 점프
	UPROPERTY(Transient)
	TWeakObjectPtr<UPFNavLinkProxy> ActiveJumpLink;

	NavNodeRef ActiveJumpRef = INVALID_NAVNODEREF;
	FVector JumpLandingFeet = FVector::ZeroVector;
	FVector JumpTakeoffFeet = FVector::ZeroVector;
	FVector JumpLinkDestination = FVector::ZeroVector;
	bool bExecutingPathJump = false;
	bool bApproachingJumpStart = false;
	float JumpTimeRemaining = 0.f;
	float SavedAirControl = 0.f;
	float SavedFallingFriction = 0.f;
	float SavedFallingBraking = 0.f;
	float SavedBrakingFriction = 0.f;

	// 이동 능력 태그 구독
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> NavigationASC;

	FDelegateHandle JumpTagHandle;
	FDelegateHandle MoveTagHandle;
};
