#pragma once

#include "Character/PFCharacter.h"
#include "System/Subsystems/PFNaviSubsystem.h"

#include "PFEnemy.generated.h"

// 공통 적 클래스
UCLASS(Abstract, meta=(PrioritizeCategories="Enemy PFCharacter UI GAS"))
class PORTFOLIO_API APFEnemy : public APFCharacter
{
	GENERATED_BODY()

public:
	APFEnemy();
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;
	virtual void PostInitializeComponents() override;

	virtual float GetAimPitch() const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	void SetAbility();

	void AcquireNearestPlayerTarget();

	bool IsPlayerTargetValid(const APFCharacter* Candidate) const;

	float GetTargetSurfaceDistance(const APFCharacter* Candidate) const;

	virtual bool ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const;

	void UpdateEnemyMovement(APFCharacter* Target, float SurfaceDistance, float DeltaTime);

	bool RefreshMovementPath(const APFCharacter* Target);

	bool FollowMovementPath(float DeltaTime);

	void BeginPathWalk(const FVector& Destination);

	void ClearMovementPath();

	bool HasPendingMovementPath() const;

	bool IsPathJumpBlocked() const;

	void RotateEnemyTowards(const FVector& WorldDirection, float DeltaTime);

	void SetEnemyDirection(EPFDirection NewDirection);

	void ClearEnemyIntent();

	void HandleDeathAnimationEnd();

protected:
	// 추적 대상 플레이어
	UPROPERTY(Transient)
	TWeakObjectPtr<APFCharacter> TargetCharacter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float DesiredCombatDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float DistanceTolerance = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat")
	bool bRetreatWhenTooClose = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.05"))
	float TargetRefreshInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.01"))
	float AttackCommandInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "0.1"))
	float PathRefreshInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "1.0"))
	float PathTargetMoveThreshold = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "1.0"))
	float PathPointAcceptanceRadius = 45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "1.0"))
	float JumpTakeoffTolerance = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "0.1"))
	float PathJumpTimeout = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "0.1"))
	float PathWalkStuckTimeout = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navi", meta = (ClampMin = "0.1"))
	float PathWalkMinimumProgressDistance = 10.f;

private:
	UPROPERTY(Replicated)
	float ReplicatedAimPitch = 0.f;

	float TargetRefreshTimeRemaining = 0.f;

	float AttackCommandTimeRemaining = 0.f;

	// 이동 경로 노드 목록
	TArray<int32> NodePath;

	int32 CurNodeIdx = 0;

	FVector GoalLocation = FVector::ZeroVector;

	float PathRefreshTimeRemaining = 0.f;

	FVector LastPathTargetLocation = FVector::ZeroVector;

	bool bExecutingPathJump = false;

	bool bPathJumpWasAirborne = false;

	bool bExecutingPathWalk = false;

	// 현재 보행, 점프 목적지
	FVector CurrentTraversalDestination = FVector::ZeroVector;

	// 이동 정체 판정 기준 위치
	FVector LastPathWalkProgressLocation = FVector::ZeroVector;

	float PathWalkStuckTimeRemaining = 0.f;

	float PathJumpTimeRemaining = 0.f;
};
