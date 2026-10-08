#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "System/Framework/PFEnemyAIController.h"
#include "PFCampaignEnemyController.generated.h"

// 캠페인 경비, 순찰, 지휘관 패턴
UCLASS()
class PORTFOLIO_API APFCampaignEnemyController : public APFEnemyAIController
{
	GENERATED_BODY()
public:
	APFCampaignEnemyController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void Configure(const FVector& Origin, const TArray<FVector>& Patrol,
		class APFCampaignDirector* InDirector, bool bCommander, bool bBoss);
	void NotifyDamageFrom(APFCharacter* Attacker);
	virtual void Tick(float DeltaTime) override;
	bool IsBossPatternActive() const;
protected:
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsPlayerTargetValid(const APFCharacter* Candidate) const override;
	virtual bool CanAcquireTarget(const APFCharacter* Candidate, const APFCharacter* Previous) const override;
	virtual bool HandleIdleMovement(float DeltaTime) override;
	virtual bool ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const override;
private:
	void BeginReturn();
	void FinishReturn();

	FVector GuardOrigin = FVector::ZeroVector;
	static constexpr float GuardRadius = 10000.f;
	TArray<FVector> PatrolPoints;
	int32 PatrolIndex = 0;
	float SavedWalkSpeed = 0.f;
	double NextShockwave = 0.;
	bool bConfigured = false;
	bool bFinalBoss = false;
	bool bWasInCombat = false;
	bool bReturning = false;
	// 복귀 중 무적, 공격 차단 태그 소유자
	TWeakObjectPtr<UAbilitySystemComponent> ReturnASC;
	FGameplayAbilitySpecHandle ShockwaveHandle;
	TWeakObjectPtr<class APFCampaignDirector> Director;
};
