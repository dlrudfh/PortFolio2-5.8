#include "GAS/Abilities/Ultimate/PFGA_Ultimate_TwinBlast.h"
#include "GAS/PFGameplayTags.h"

#include "Character/TwinBlast/PFTwinBlast.h"
#include "Campaign/PFCampaignDirector.h"
#include "GameFramework/CharacterMovementComponent.h"

UPFGA_Ultimate_TwinBlast::UPFGA_Ultimate_TwinBlast()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Ultimate_Twinblast);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(PFGameplayTags::Character_Block_Ultimate);
	ActivationBlockedTags.AddTag(PFGameplayTags::Character_State_Dead);
	ActivationBlockedTags.AddTag(PFGameplayTags::Character_State_Jumping);
	ActivationBlockedTags.AddTag(PFGameplayTags::Character_State_Falling);
}

bool UPFGA_Ultimate_TwinBlast::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const APFTwinBlast* TwinBlast = Cast<APFTwinBlast>(ActorInfo->AvatarActor.Get());
	return TwinBlast && !APFCampaignDirector::BlocksInput(TwinBlast->GetController())
		&& TwinBlast->IsPlayerCharacter() && !TwinBlast->IsJumpPadFlightActive()
		&& !TwinBlast->GetCharacterMovement()->IsFalling();
}

void UPFGA_Ultimate_TwinBlast::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APFTwinBlast* TwinBlast = ActorInfo
		? Cast<APFTwinBlast>(ActorInfo->AvatarActor.Get())
		: nullptr;
	if (!TwinBlast || !TwinBlast->IsPlayerCharacter())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TwinBlast->ToggleUltimateState();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
