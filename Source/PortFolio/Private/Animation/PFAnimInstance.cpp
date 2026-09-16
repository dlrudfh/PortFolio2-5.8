#include "Animation/PFAnimInstance.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/PFGameplayTags.h"
#include "Character/PFCharacter.h"

UPFAnimInstance::UPFAnimInstance() : CurrentPawnSpeed(0.f), IsLevelStart(true), IsRelaxed(true), IsFPS(false), CurrentDir(IDLE), CurMtg(nullptr), LerpBlend(0.1f)
{
	LerpBlend.SetBlendOption(EAlphaBlendOption::Linear);
	LerpBlend.SetValueRange(0.f, 0.f);
	LerpBlend.Reset();
}

// 게임 스레드에서 상태 태그 캐시 갱신
void UPFAnimInstance::RefreshCachedStateTags()
{
	check(IsInGameThread());
	CachedStateTags.Reset();

	// 캐릭터, 부착 부모의 ASC 조회
	const APFCharacter* Character = Cast<APFCharacter>(TryGetPawnOwner());
	if (!Character && GetOwningActor())
	{
		Character = Cast<APFCharacter>(GetOwningActor()->GetAttachParentActor());
	}

	const UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem)
	{
		AbilitySystem->GetOwnedGameplayTags(CachedStateTags);
	}
}

// 공중 상태 조회
bool UPFAnimInstance::IsInAir() const
{
	return CachedStateTags.HasTag(PFGameplayTags::Character_State_Jumping) || CachedStateTags.HasTag(PFGameplayTags::Character_State_Falling);
}

// 공격 상태 조회
bool UPFAnimInstance::IsOnAttack() const
{
	return CachedStateTags.HasTag(PFGameplayTags::Character_State_Attacking);
}

// 콤보 입력 구간 조회
bool UPFAnimInstance::IsSaveAttack() const
{
	return CachedStateTags.HasTag(PFGameplayTags::Character_State_Attack_ComboWindow);
}

// 궁극기 상태 조회
bool UPFAnimInstance::IsUltimate() const
{
	return CachedStateTags.HasTag(PFGameplayTags::Character_State_Ultimate);
}

// 질주 애니메이션 조건 조회
bool UPFAnimInstance::IsSprint() const
{
	return CachedStateTags.HasTag(PFGameplayTags::Character_State_Sprinting) && !IsOnAttack() && !IsUltimate();
}

// 사망 상태 조회
bool UPFAnimInstance::IsDead() const
{
	return CachedStateTags.HasTag(PFGameplayTags::Character_State_Dead);
}

// 등장 몽타주 확인
bool UPFAnimInstance::IsLevelStartMontage(const UAnimMontage* Montage) const
{
	return Montages.IsValidIndex(LEVELSTART_GLOBAL) && Montage == Montages[LEVELSTART_GLOBAL];
}

void UPFAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	RefreshCachedStateTags();

	if (IsDead())
	{
		return;
	}

	// 혼합값, 대기 자세 갱신
	LerpBlend.Update(DeltaSeconds);
	LerpVal = LerpBlend.GetBlendedValue();

	RelaxTime -= DeltaSeconds;
	if (IsFPS)
	{
		IsRelaxed = false;
	}
	else if (RelaxTime < 0.f)
	{
		IsRelaxed = true;
	}
	else
	{
		IsRelaxed = false;
	}

	// 이동 속도 갱신
	APawn* Pawn = TryGetPawnOwner();
	if (Pawn)
	{
		CurrentPawnSpeed = Pawn->GetVelocity().Size();
	}
}

// 콤보 입력 구간 시작 알림
void UPFAnimInstance::AnimNotify_SaveAttack()
{
	SendAttackGameplayEvent(PFGameplayTags::Character_Event_Attack_ComboWindow);
}

// 콤보 초기화 알림
void UPFAnimInstance::AnimNotify_ResetCombo()
{
	ResetAttackCombo();
}

// 공격 혼합, 콤보 초기화
void UPFAnimInstance::ResetAttackCombo()
{
	Set_Lerp(LerpVal, false, 10.f);
	SendAttackGameplayEvent(PFGameplayTags::Character_Event_Attack_ComboReset);
}

// 서버 공격 이벤트 전달
void UPFAnimInstance::SendAttackGameplayEvent(const FGameplayTag& EventTag)
{
	APawn* AvatarPawn = TryGetPawnOwner();
	if (!AvatarPawn || !AvatarPawn->HasAuthority())
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = AvatarPawn;
	EventData.Target = AvatarPawn;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AvatarPawn, EventTag, EventData);
}

// 사망 시 몽타주 중단
void UPFAnimInstance::Dead()
{
	Montage_Stop(0.f);
}

// 사망 연출 종료 알림
void UPFAnimInstance::AnimNotify_DeathEnd()
{
	DeathEnd.Broadcast();
}

// 소유 캐릭터의 네트워크 역할 조회
ENetRole UPFAnimInstance::CheckCharacterType()
{
	if (APawn* Pawn = TryGetPawnOwner())
	{
		return Pawn->GetLocalRole();
	}
	{
		return ROLE_None;
	}
}

// 애니메이션 혼합 보간 설정
void UPFAnimInstance::Set_Lerp(float TLerpVal, bool TLerpIncrease, float TLerpCoefficient)
{
	LerpVal = TLerpVal;
	const float TargetValue = TLerpIncrease ? 1.f : 0.f;
	const float BlendTime = TLerpCoefficient > 0.f ? 1.f / TLerpCoefficient : 0.f;
	LerpBlend.SetBlendTime(BlendTime);
	LerpBlend.SetValueRange(TLerpVal, TargetValue);
	LerpBlend.Reset();
}

void UPFAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RefreshCachedStateTags();

	// 등장 몽타주 시작 이벤트 연결
	if (APFCharacter* Character = Cast<APFCharacter>(TryGetPawnOwner()))
	{
		OnMontageStarted.AddUniqueDynamic(Character, &APFCharacter::OnLevelStartMontageStarted);
	}
}
