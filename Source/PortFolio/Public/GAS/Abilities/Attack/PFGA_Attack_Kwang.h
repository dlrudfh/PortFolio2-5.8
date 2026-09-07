#pragma once

#include "GAS/Abilities/Attack/PFGA_BasicAttack.h"

#include "PFGA_Attack_Kwang.generated.h"

// 광 기본 공격 어빌리티
UCLASS()
class PORTFOLIO_API UPFGA_Attack_Kwang : public UPFGA_BasicAttack
{
	GENERATED_BODY()

public:
	UPFGA_Attack_Kwang();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void WaitForEvent(AActor* AvatarActor) override;
	virtual void ExecuteMontageGC(AActor* AvatarActor) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void HandleAttackInputPressed() override;

private:
	UFUNCTION()
	void OnComboWindow(FGameplayEventData Payload);
	void TryContinueCombo();
	void AdvanceComboIndex();

private:
	int32 ComboIndex = 0;

	// 콤보 입력 구간 이벤트
	FGameplayTag ComboWindowEventTag;
};
