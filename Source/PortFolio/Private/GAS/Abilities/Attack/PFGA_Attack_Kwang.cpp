#include "GAS/Abilities/Attack/PFGA_Attack_Kwang.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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

	MontageGCTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Character.Attack.Kwang.Normal.Montage"));
	ComboWindowEventTag = PFGameplayTags::Character_Event_Attack_ComboWindow;
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

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const APFKwang* Kwang = Cast<APFKwang>(AvatarActor);
	if (!Kwang)
	{
		return false;
	}

	const UCharacterMovementComponent* CharacterMovement = Kwang ? Kwang->GetCharacterMovement() : nullptr;
	return !(Kwang->IsPlayerCharacter() && CharacterMovement && CharacterMovement->IsFalling() && Kwang->IsSprinting());
}

void UPFGA_Attack_Kwang::WaitForEvent(AActor* AvatarActor)
{
	(void)AvatarActor;

	// 콤보 입력 구간 이벤트 연결
	UAbilityTask_WaitGameplayEvent* ComboWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ComboWindowEventTag, nullptr, false, true);
	ComboWindowTask->EventReceived.AddDynamic(this, &UPFGA_Attack_Kwang::OnComboWindow);
	ComboWindowTask->ReadyForActivation();

}

void UPFGA_Attack_Kwang::ExecuteMontageGC(AActor* AvatarActor)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bIsKwangAvatar = AvatarActor
		&& AvatarActor->IsA<APFKwang>();
	if (!AbilitySystem || !bIsKwangAvatar)
	{
		return;
	}

	if (ComboIndex < etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA) || ComboIndex > etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKD))
	{
		ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA);
	}

	// 콤보 번호를 몽타주 Cue로 전달
	FGameplayCueParameters CueParameters;
	CueParameters.RawMagnitude = static_cast<float>(ComboIndex);
	AbilitySystem->ExecuteGameplayCue(MontageGCTag, CueParameters);
	AdvanceComboIndex();
}

void UPFGA_Attack_Kwang::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA);
	SetComboWindowTag(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 콤보 입력 구간 열기
void UPFGA_Attack_Kwang::OnComboWindow(FGameplayEventData Payload)
{
	(void)Payload;
	SetComboWindowTag(true);
	TryContinueCombo();
}

void UPFGA_Attack_Kwang::HandleAttackInputPressed()
{
	TryContinueCombo();
}

// 유지된 입력으로 다음 콤보 실행
void UPFGA_Attack_Kwang::TryContinueCombo()
{
	if (!IsComboWindowOpen())
	{
		return;
	}

	APFCharacter* Attacker = Cast<APFCharacter>(GetAvatarActorFromActorInfo());
	if (!Attacker || !IsAttackInputHeld())
	{
		return;
	}

	SetComboWindowTag(false);
	PlayAttackMontage(Attacker);
}

// 다음 콤보 몽타주 선택
void UPFGA_Attack_Kwang::AdvanceComboIndex()
{
	switch (ComboIndex)
	{
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA):
		ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKB);
		break;
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKB):
		ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKC);
		break;
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKC):
		ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKD);
		break;
	default:
		ComboIndex = etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA);
		break;
	}
}
