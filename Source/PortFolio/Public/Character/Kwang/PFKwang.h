#pragma once

#include "Character/PFPlayer.h"

#include "PFKwang.generated.h"

class UPFGA_Attack_Kwang;

// 광 플레이어 클래스
UCLASS(meta=(PrioritizeCategories="PFCharacter Camera UI Chest GAS"))
class PORTFOLIO_API APFKwang : public APFPlayer
{
	GENERATED_BODY()

	friend class UPFGA_Attack_Kwang;

	enum class PARTICLE
	{
		SWORDTRAIL,
		PARTICLE_END
	};
	using enum APFKwang::PARTICLE;

	enum class SOUND
	{
		SLASH,
		HIT,
		SOUND_END
	};
	using enum APFKwang::SOUND;

public:
	APFKwang();

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
	void AttackStart();
	UFUNCTION()
	void AttackEnd();
	void ProcessSwordHits();
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void PostHitProcessing() override;

private:
	// 검 궤적 이펙트
	UParticleSystemComponent* SwordTrail;
	bool bSwordHitDetectionActive;
	// 이전 프레임의 검 위치
	FVector PreviousSwordBaseLocation;
	FVector PreviousSwordTipLocation;
	// 공격 중 중복 피격 방지
	TSet<AActor*> HitActorsDuringAttack;
	const float SwordTraceRadius = 12.f;
};
