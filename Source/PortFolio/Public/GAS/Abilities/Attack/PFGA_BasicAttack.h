#pragma once

#include "PortFolio/PortFolio.h"

#include "Abilities/GameplayAbility.h"

#include "PFGA_BasicAttack.generated.h"

class APFCharacter;
class UPFAnimInstance;
class UAnimMontage;

// 공통 기본 공격 어빌리티
UCLASS(Abstract)
class PORTFOLIO_API UPFGA_BasicAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPFGA_BasicAttack();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	UPFAnimInstance* GetAnimInstance(const APFCharacter* Character) const;

	virtual void WaitForEvent(AActor* AvatarActor) PURE_VIRTUAL(UPFGA_BasicAttack::WaitForEvent);
	virtual void ExecuteMontageGC(AActor* AvatarActor) PURE_VIRTUAL(UPFGA_BasicAttack::ExecuteMontageGC);
	void PlayAttackMontage(AActor* AvatarActor);
	virtual void HandleAttackInputPressed();
	virtual void HandleAttackInputReleased();
	bool IsAttackInputHeld() const;
	bool IsComboWindowOpen() const;
	void SetComboWindowTag(bool bEnabled) const;

	void FinishAbility(bool bWasCancelled);

private:
	void WaitForCommonEndConditions();
	void CancelCurrentAbility();

	UFUNCTION()
	void OnComboReset(FGameplayEventData Payload);
	UFUNCTION()
	void OnCancellationTagAdded();
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID);
	void UnbindAttackMontageEnd();

protected:
	// 공격 몽타주 Cue 태그
	FGameplayTag MontageGCTag;

private:
	FGameplayTag AttackingStateTag;
	FGameplayTag ComboResetEventTag;
	FGameplayTag AttackBlockedTag;
	FGameplayTag DeadStateTag;
	// 종료 이벤트를 구독한 애니메이션
	TWeakObjectPtr<UPFAnimInstance> BoundAnimInstance;
	// 현재 공격 몽타주
	TWeakObjectPtr<UAnimMontage> ActiveAttackMontage;
	// 공격 몽타주 인스턴스 식별자
	int32 ActiveAttackMontageInstanceID = INDEX_NONE;
	bool bStartingAttackMontage = false;
};
