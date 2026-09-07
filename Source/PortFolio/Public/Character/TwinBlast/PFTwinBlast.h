
#pragma once

#include "Character/PFPlayer.h"
#include "PFTwinBlast.generated.h"

class UPFGA_Attack_TwinBlast;
class UPFGA_Ultimate_TwinBlast;

// 트윈블라스트 플레이어 클래스
UCLASS(meta=(PrioritizeCategories="UltGun PFCharacter Camera UI Chest GAS"))
class PORTFOLIO_API APFTwinBlast : public APFPlayer
{
	GENERATED_BODY()

	friend class UPFGA_Attack_TwinBlast;
	friend class UPFGA_Ultimate_TwinBlast;

	enum class PARTICLE
	{
		MUZZLELEFT,
		MUZZLERIGHT,
		ULTMUZZLELEFT,
		ULTMUZZLERIGHT,
		ULTACTIVATE,
		ULTSHOULDER,
		PARTICLE_END
	};
	using enum APFTwinBlast::PARTICLE;

	enum class SOUND
	{
		SHOOT,
		ULTSHOOT,
		HIT,
		SOUND_END
	};
	using enum APFTwinBlast::SOUND;

public:
	APFTwinBlast();
	bool IsUltimateActive() const;
	
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	virtual void InitAbilityActorInfo() override;
	void GiveUltimateAttackAbility();

	virtual void SetDir() override;
	virtual void SetMesh() override;
	virtual void SetParticle() override;
	virtual void SetSound() override;
	virtual void Jump() override;

	virtual void Attack() override;
	virtual TSubclassOf<UGameplayAbility> GetAttackAbilityClass() const override;
	void ToggleUltimateState();
	virtual void UltimateTagChanged(FGameplayTag StateTag, int32 NewCount) override;
	void ApplyUltimateState();

	UFUNCTION()
	void GameplayCue_Character_Attack_Twinblast_Normal_Montage(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Character_Attack_Twinblast_Ultimate_Montage(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Character_Attack_Twinblast_Normal_Shoot(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Character_Attack_Twinblast_Ultimate_Shoot(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	void StartUltShoulderEffect();
	void StopUltShoulderEffect();
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted);
	virtual void PostHitProcessing() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
	UPROPERTY(Replicated)
	bool ShootLeft;

	const float UltSpeed = 200.f;

	// 궁극기 총
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UltGun", meta = (AllowPrivateAccess = "true"))
	class AUltGun* UltGun;

	// 궁극기 어깨 이펙트
	UPROPERTY(Transient)
	class UParticleSystemComponent* UltShoulderEffect;

	// 궁극기 공격 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayAbility> UltimateAttackAbilityClass;
};
