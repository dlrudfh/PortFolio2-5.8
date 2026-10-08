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

	virtual void ExecuteMontageGC(AActor* AvatarActor) override;
	virtual UAnimMontage* GetAttackMontage(APFCharacter* Character) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void AdvanceComboIndex();

private:
	int32 ComboIndex = 0;
};
