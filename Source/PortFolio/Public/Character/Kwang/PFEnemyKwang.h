#pragma once

#include "Character/PFEnemy.h"
#include "GameplayCueInterface.h"

#include "PFEnemyKwang.generated.h"

// 광 적 클래스
UCLASS(meta=(PrioritizeCategories="Enemy PFCharacter UI GAS"))
class PORTFOLIO_API APFEnemyKwang : public APFEnemy, public IGameplayCueInterface
{
	GENERATED_BODY()

	enum class PARTICLE
	{
		SWORDTRAIL,
		PARTICLE_END
	};
	using enum APFEnemyKwang::PARTICLE;

	enum class SOUND
	{
		SLASH,
		HIT,
		SOUND_END
	};
	using enum APFEnemyKwang::SOUND;

public:
	APFEnemyKwang();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

private:
	virtual void SetMesh() override;
	virtual void SetParticle() override;
	virtual void SetSound() override;

	UFUNCTION()
	void GameplayCue_Character_Attack_Kwang_Normal_Montage(
		EGameplayCueEvent::Type EventType,
		const FGameplayCueParameters& Parameters);

	UFUNCTION()
	void SwordAttackStart();
	UFUNCTION()
	void SwordAttackEnd();
	void ProcessSwordHits();
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void PostHitProcessing() override;

private:
	// 검 궤적 이펙트
	class UParticleSystemComponent* SwordTrail = nullptr;
	bool bSwordHitDetectionActive = false;
	// 이전 프레임의 검 위치
	FVector PreviousSwordBaseLocation = FVector::ZeroVector;
	FVector PreviousSwordTipLocation = FVector::ZeroVector;
	// 공격 중 중복 피격 방지
	TSet<AActor*> HitActorsDuringAttack;
	const float SwordTraceRadius = 12.f;
};
