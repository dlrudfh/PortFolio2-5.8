#include "GAS/Abilities/Attack/PFGA_Attack_Kwang.h"

#include "AbilitySystemComponent.h"
#include "Animation/PFAnimInst_Kwang.h"
#include "Character/Kwang/PFKwang.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "GAS/PFGameplayTags.h"

UPFGA_Attack_Kwang::UPFGA_Attack_Kwang()
{
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Attack_Kwang);
	SetAssetTags(AssetTags);

	MontageGCTag = PFGameplayTags::GameplayCue_Character_Attack_Kwang_Normal_Montage;
	ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA);
}

bool UPFGA_Attack_Kwang::CanActivateAbility(
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

	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	const APFKwang* Kwang = Cast<APFKwang>(AvatarActor);
	if (!Kwang)
	{
		return false;
	}

	const UCharacterMovementComponent* CharacterMovement = Kwang->GetCharacterMovement();
	return !(Kwang->IsPlayerCharacter() && CharacterMovement->IsFalling() && Kwang->IsSprinting());
}


void UPFGA_Attack_Kwang::ExecuteMontageGC(AActor* AvatarActor)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	// 콤보 번호를 몽타주 Cue로 전달
	FGameplayCueParameters CueParameters;
	CueParameters.RawMagnitude = static_cast<float>(ComboIndex);
	AbilitySystem->ExecuteGameplayCue(MontageGCTag, CueParameters);
	AdvanceComboIndex();
}

UAnimMontage* UPFGA_Attack_Kwang::GetAttackMontage(APFCharacter* Character)
{
	UPFAnimInstance* AnimInstance = GetAnimInstance(Character);
	UAnimMontage* Montage = AnimInstance->GetMontageByIndex(ComboIndex);
	AdvanceComboIndex();
	return Montage;
}

void UPFGA_Attack_Kwang::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}




// 다음 콤보 몽타주 선택
void UPFGA_Attack_Kwang::AdvanceComboIndex()
{
	using enum UPFAnimInst_Kwang::MTGIDX_K;
	ComboIndex = ComboIndex >= etoi(ATTACKA) && ComboIndex < etoi(ATTACKD) ? ComboIndex + 1 : etoi(ATTACKA);
}
