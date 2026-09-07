#pragma once

#include "Character/PFEnemy.h"
#include "GameplayCueInterface.h"

#include "PFEnemyTwinblast.generated.h"

class UPFGA_Attack_TwinBlast;

// 트윈블라스트 적 클래스
UCLASS(meta=(PrioritizeCategories="Enemy PFCharacter UI GAS"))
class PORTFOLIO_API APFEnemyTwinblast : public APFEnemy, public IGameplayCueInterface
{
	GENERATED_BODY()

	friend class UPFGA_Attack_TwinBlast;

	enum class PARTICLE
	{
		MUZZLELEFT,
		MUZZLERIGHT,
		PARTICLE_END
	};
	using enum APFEnemyTwinblast::PARTICLE;

	enum class SOUND
	{
		SHOOT,
		HIT,
		SOUND_END
	};
	using enum APFEnemyTwinblast::SOUND;

public:
	APFEnemyTwinblast();

	virtual void PostInitializeComponents() override;

private:
	virtual void SetMesh() override;

	virtual void SetParticle() override;

	virtual void SetSound() override;

	virtual bool ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const override;

	UFUNCTION()
	void GameplayCue_Character_Attack_Twinblast_Normal_Montage(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	UFUNCTION()
	void GameplayCue_Character_Attack_Twinblast_Normal_Shoot(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted) override;

	virtual void PostHitProcessing() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	bool bShootLeft = true;
};
