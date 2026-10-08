#include "Animation/PFAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "UObject/ConstructorHelpers.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/PFGameplayTags.h"
#include "Character/PFCharacter.h"

const TCHAR* const UPFAnimInstance::TwinblastUltimateStart = TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_Start_Montage.Ability_Ultimate_Start_Montage");

const TCHAR* const UPFAnimInstance::TwinblastUltimateEnd = TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_End_Montage.Ability_Ultimate_End_Montage");

UPFAnimInstance::UPFAnimInstance() : CurrentPawnSpeed(0.f), IsLevelStart(true), IsRelaxed(true), IsFPS(false), CurrentDir(IDLE), CurMtg(nullptr), LerpBlend(0.1f)
{
	LerpBlend.SetBlendOption(EAlphaBlendOption::Linear);
	LerpBlend.SetValueRange(0.f, 0.f);
	LerpBlend.Reset();
}

// 게임 스레드에서 상태 태그 캐시 갱신
void UPFAnimInstance::RefreshCachedStateTags()
{
	uint8 StateFlags = 0;

	// 캐릭터, 부착 부모의 ASC 조회
	const APFCharacter* Character = Cast<APFCharacter>(TryGetPawnOwner());
	if (!Character && GetOwningActor())
	{
		Character = Cast<APFCharacter>(GetOwningActor()->GetAttachParentActor());
	}

	const UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem)
	{
		StateFlags |= (AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Jumping)
			|| AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Falling)) ? InAirFlag : 0;
		StateFlags |= AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Attacking) ? AttackFlag : 0;
		StateFlags |= AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Attack_ComboWindow) ? ComboWindowFlag : 0;
		StateFlags |= AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Ultimate) ? UltimateFlag : 0;
		StateFlags |= AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Sprinting) ? SprintFlag : 0;
		StateFlags |= AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Dead) ? DeadFlag : 0;
	}

	// 갱신 중인 상태가 작업 스레드에 일부만 노출되는 문제 방지
	CachedStateFlags.store(StateFlags, std::memory_order_relaxed);
}

// 공중 상태 조회
bool UPFAnimInstance::IsInAir() const
{
	return (CachedStateFlags.load(std::memory_order_relaxed) & InAirFlag) != 0;
}

// 공격 상태 조회
bool UPFAnimInstance::IsOnAttack() const
{
	return (CachedStateFlags.load(std::memory_order_relaxed) & AttackFlag) != 0;
}

// 콤보 입력 구간 조회
bool UPFAnimInstance::IsSaveAttack() const
{
	return (CachedStateFlags.load(std::memory_order_relaxed) & ComboWindowFlag) != 0;
}

// 궁극기 상태 조회
bool UPFAnimInstance::IsUltimate() const
{
	return (CachedStateFlags.load(std::memory_order_relaxed) & UltimateFlag) != 0;
}

// 질주 애니메이션 조건 조회
bool UPFAnimInstance::IsSprint() const
{
	// 서로 다른 갱신 시점의 상태가 섞이는 문제 방지
	const uint8 StateFlags = CachedStateFlags.load(std::memory_order_relaxed);
	return (StateFlags & SprintFlag) != 0 && (StateFlags & (AttackFlag | UltimateFlag)) == 0;
}

// 사망 상태 조회
bool UPFAnimInstance::IsDead() const
{
	return (CachedStateFlags.load(std::memory_order_relaxed) & DeadFlag) != 0;
}

// 등장 몽타주 확인
bool UPFAnimInstance::IsLevelStartMontage(const UAnimMontage* Montage) const
{
	return Montage == Montages[LEVELSTART_GLOBAL];
}

// 인덱스에 해당하는 몽타주 조회
UAnimMontage* UPFAnimInstance::GetMontageByIndex(int32 Index) const
{
	return Montages.IsValidIndex(Index) ? Montages[Index] : nullptr;
}

// 공격 몽타주 추적, 이전 인스턴스 노티파이 제외
void UPFAnimInstance::TrackAttackMontage(UAnimMontage* Montage, FName AttackNotify, FName AttackEndNotify)
{
	CurMtg = Montage;
	FAnimMontageInstance* AttackInstance = GetActiveInstanceForMontage(CurMtg);
	AttackMontageInstanceID = AttackInstance ? AttackInstance->GetInstanceID() : INDEX_NONE;

	// 이전 공격 몽타주의 남은 노티파이 제외
	for (FAnimNotifyEventReference& EventReference : NotifyQueue.AnimNotifies)
	{
		const FAnimNotifyEvent* Notify = EventReference.GetNotify();
		const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
			EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
		if (Notify && !Notify->Notify && !Notify->NotifyStateClass && MontageContext
			&& MontageContext->MontageInstanceID != AttackMontageInstanceID
			&& (Notify->NotifyName == TEXT("SaveAttack") || Notify->NotifyName == TEXT("ResetCombo")
				|| Notify->NotifyName == AttackNotify || Notify->NotifyName == AttackEndNotify))
		{
			EventReference.SetNotify(nullptr);
		}
	}

	Set_Lerp(LerpVal, true, 10.f);
	RelaxTime = 3.f;
}

// 진행 중인 공격의 종료 처리 보류
bool UPFAnimInstance::FinishAttackMontage()
{
	const FAnimMontageInstance* AttackInstance = GetMontageInstanceForID(AttackMontageInstanceID);
	if (AttackInstance && AttackInstance->IsValid()
		&& (AttackInstance->IsActive() || AttackInstance->GetWeight() > 0.f))
	{
		return false;
	}
	AttackMontageInstanceID = INDEX_NONE;
	return true;
}

// 몽타주 목록의 인덱스 조회
int32 UPFAnimInstance::GetMontageIndex(const UAnimMontage* Montage) const
{
	const int32 Index = Montages.IndexOfByKey(Montage);
	return Index == INDEX_NONE ? Montages.Num() : Index;
}

// 몽타주 경로 목록 로드
void UPFAnimInstance::LoadMontages(TConstArrayView<const TCHAR*> Paths, int32 RequiredCount)
{
	Montages.SetNum(Paths.Num());
	for (int32 Index = 0; Index < Paths.Num(); ++Index)
	{
		ConstructorHelpers::FObjectFinder<UAnimMontage> Montage(Paths[Index]);
		Montages[Index] = Montage.Object;
		if (!Montage.Succeeded())
		{
			if (Index < RequiredCount)
			{
				PFLOG(Fatal, TEXT("Montage load failed: %s"), Paths[Index]);
			}
			else
			{
				PFLOG(Warning, TEXT("Montage load failed: %s"), Paths[Index]);
			}
		}
	}
}

// 파생 애니메이션의 몽타주 상태 연결
void UPFAnimInstance::HandleMontageStarted(UAnimMontage*)
{
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

// 서버, 로컬 예측 공격 이벤트 전달
void UPFAnimInstance::SendAttackGameplayEvent(const FGameplayTag& EventTag)
{
	APawn* AvatarPawn = TryGetPawnOwner();
	if (!AvatarPawn)
	{
		return;
	}
	if (!AvatarPawn->HasAuthority())
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AvatarPawn);
		const UGameplayAbility* Ability = AbilitySystem ? AbilitySystem->GetAnimatingAbility() : nullptr;
		if (!AvatarPawn->IsLocallyControlled() || !Ability || !Ability->IsActive()
			|| Ability->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::LocalPredicted
			|| !Ability->GetAssetTags().HasTag(PFGameplayTags::Character_Ability_Attack)) return;
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
	OnMontageStarted.AddUniqueDynamic(this, &UPFAnimInstance::HandleMontageStarted);

	RefreshCachedStateTags();

	// 등장 몽타주 시작 이벤트 연결
	if (APFCharacter* Character = Cast<APFCharacter>(TryGetPawnOwner()))
	{
		OnMontageStarted.AddUniqueDynamic(Character, &APFCharacter::OnLevelStartMontageStarted);
	}
}
