#include "GAS/Abilities/Movement/PFGA_CharacterJump.h"
#include "GAS/PFGameplayTags.h"
#include "Character/PFCharacter.h"
#include "Campaign/PFCampaignDirector.h"

UPFGA_CharacterJump::UPFGA_CharacterJump()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Jump);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(PFGameplayTags::Character_Block_Jump);
}

bool UPFGA_CharacterJump::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	const APFCharacter* Character = ActorInfo ? Cast<APFCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Character && APFCampaignDirector::BlocksInput(Character->GetController())) return false;
	if (!Character || Character->IsJumpPadFlightActive())
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UPFGA_CharacterJump::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
