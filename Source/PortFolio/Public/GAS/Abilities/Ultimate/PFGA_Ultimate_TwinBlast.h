#pragma once

#include "Abilities/GameplayAbility.h"

#include "PFGA_Ultimate_TwinBlast.generated.h"

// 트윈블라스트 궁극기 전환 어빌리티
UCLASS()
class PORTFOLIO_API UPFGA_Ultimate_TwinBlast : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPFGA_Ultimate_TwinBlast();

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
