#include "GAS/Abilities/Ultimate/PFGA_Ultimate_TwinBlast.h"

#include "Character/TwinBlast/PFTwinBlast.h"
#include "GameFramework/CharacterMovementComponent.h"

UPFGA_Ultimate_TwinBlast::UPFGA_Ultimate_TwinBlast()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.Ability.Ultimate.Twinblast")));
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.Block.Ultimate")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Jumping")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Falling")));
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

	const APFTwinBlast* TwinBlast = ActorInfo
		? Cast<APFTwinBlast>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UCharacterMovementComponent* CharacterMovementComponent = TwinBlast
		? TwinBlast->GetCharacterMovement()
		: nullptr;
	return TwinBlast && TwinBlast->IsPlayerCharacter() && CharacterMovementComponent && !CharacterMovementComponent->IsFalling();
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
	if (!TwinBlast || !TwinBlast->IsPlayerCharacter() || !TwinBlast->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TwinBlast->ToggleUltimateState();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
