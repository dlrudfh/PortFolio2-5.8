#pragma once

#include "PortFolio/PortFolio.h"
#include "Abilities/GameplayAbility_CharacterJump.h"
#include "PFGA_CharacterJump.generated.h"

// 캐릭터 점프 어빌리티
UCLASS()
class PORTFOLIO_API UPFGA_CharacterJump : public UGameplayAbility_CharacterJump
{
	GENERATED_BODY()

public:
	UPFGA_CharacterJump();

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;
};
