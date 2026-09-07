#pragma once

#include "PortFolio/PortFolio.h"

#include "Animation/PFAnimInstance.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Abilities/GameplayAbility.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

#include "PFCharacter.generated.h"

class USoundBase;
class USoundConcurrency;

// 공통 캐릭터 클래스
UCLASS(Abstract, meta=(PrioritizeCategories="PFCharacter UI GAS"))
class PORTFOLIO_API APFCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APFCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION()
	virtual void SetMesh() PURE_VIRTUAL(APFCharacter::SetMesh);
	UFUNCTION()
	virtual void SetParticle() PURE_VIRTUAL(APFCharacter::SetParticle);
	UFUNCTION()
	virtual void SetSound() PURE_VIRTUAL(APFCharacter::SetSound);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	bool ApplyAttackDamageTo(APFCharacter* Target, float Damage, AActor* Attacker,
		const UGameplayAbility* AttackAbility = nullptr, const FHitResult* HitResult = nullptr);

	UFUNCTION(BlueprintPure, Category = "Animation")
	virtual float GetAimPitch() const;

	float GetDamage() const;
	UPFAttributeSet* GetAttributeSet() const { return AttributeSet; }
	bool IsDeadCharacter() const;
	bool HasStateTag(const FGameplayTag& StateTag) const;
	virtual bool IsAttackCommandActive() const;

	UFUNCTION()
	void OnLevelStartMontageStarted(UAnimMontage* Montage);

protected:
	virtual void InitAbilityActorInfo();
	void GiveAttackAbility();
	FGameplayAbilitySpecHandle GetOrGiveAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel);

	void AddTag(FName TagName, int Value);
	void SetTag(FName TagName, bool Value);
	virtual void SetBlockTags(bool bBlocked);
	bool IsMovementBlocked() const;
	bool HasAirborneTag() const;
	void UpdateAirborneTag();
	void InitGASStats();
	void BindAttributeDelegates();
	void SetReplicatedStateTag(const FGameplayTag& StateTag, bool bEnabled);
	void BindStateTagEvents();
	void UnbindStateTagEvents();
	void DeadTagChanged(FGameplayTag StateTag, int32 NewCount);
	virtual void UltimateTagChanged(FGameplayTag StateTag, int32 NewCount);
	virtual void PostHitProcessing();

	virtual void SetHPBar();
	void TickHPBar();
	virtual void Attack();
	virtual TSubclassOf<UGameplayAbility> GetAttackAbilityClass() const;

	void Dead();

	UFUNCTION()
	virtual void OnRep_FinalDir();

	UFUNCTION()
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// 머리 위 체력바
	UPROPERTY(VisibleAnywhere, Category = UI)
	class UWidgetComponent* OtherHPBar;
	bool bOtherHPBarBound = false;

	UPROPERTY(EditAnywhere)
	float WalkSpeed = 400.f;

	bool IsAttacking;
	// 태그 이벤트를 구독한 ASC
	TWeakObjectPtr<UAbilitySystemComponent> BoundStateASC;
	// 태그 이벤트 구독 핸들
	FDelegateHandle DeadStateTagHandle;
	FDelegateHandle UltimateStateTagHandle;
	// 플레이어 리스폰 타이머
	FTimerHandle PlayerRespawnTimerHandle;

	// 캐릭터 애니메이션 인스턴스
	UPROPERTY()
	UPFAnimInstance* PFAnim;

	UPROPERTY(ReplicatedUsing = OnRep_FinalDir)
	EPFDirection FinalDir;

	// 연출용 파티클, 사운드
	UPROPERTY()
	TArray<class UParticleSystem*> Particles;
	UPROPERTY()
	TArray<class USoundBase*> Sounds;

	// 어빌리티, 효과, 태그 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	class UAbilitySystemComponent* ASC;
	// GAS 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	UPFAttributeSet* AttributeSet;
	// 마나 재생 효과 핸들
	FActiveGameplayEffectHandle OwnedManaRegenEffectHandle;
	bool bOwnedGASStatsInitialized = false;
	bool bAttributeDelegatesBound = false;

	// 기본 공격 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> AttackAbilityClass;

};
