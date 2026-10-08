#include "GAS/Abilities/Attack/PFGA_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "Character/PFCharacter.h"
#include "Campaign/PFCampaignDirector.h"
#include "Animation/PFAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GAS/PFGameplayTags.h"

UPFGA_BasicAttack::UPFGA_BasicAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bServerRespectsRemoteAbilityCancellation = false;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Attack);
	SetAssetTags(AssetTags);

	AttackingStateTag = PFGameplayTags::Character_State_Attacking;
	ActivationOwnedTags.AddTag(AttackingStateTag);

	AttackBlockedTag = PFGameplayTags::Character_Block_Attack;
	DeadStateTag = PFGameplayTags::Character_State_Dead;
	ComboResetEventTag = PFGameplayTags::Character_Event_Attack_ComboReset;
	ActivationBlockedTags.AddTag(AttackingStateTag);
	ActivationBlockedTags.AddTag(AttackBlockedTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
}

bool UPFGA_BasicAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const APFCharacter* Character = Cast<APFCharacter>(ActorInfo->AvatarActor.Get());
	if (Character && APFCampaignDirector::BlocksInput(Character->GetController())) return false;
	if (Character && Character->IsJumpPadFlightActive()) return false;
	const UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
	return !AbilitySpec->IsActive();
}

void UPFGA_BasicAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	APFCharacter* Character = Cast<APFCharacter>(AvatarActor);
	UPFAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!Character || !AnimInstance)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 종료 조건 등록, 공격 연출 준비
	if (Character->HasAuthority()) Character->BreakShrubConcealmentForAttack();
	AttackSequence = 0;
	SetComboWindowTag(false);
	WaitForCommonEndConditions();
	BoundAnimInstance = AnimInstance;

	PlayAttackMontage(AvatarActor);
	if (!IsActive())
	{
		return;
	}

	WaitForEvent(AvatarActor);
}

void UPFGA_BasicAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	SetComboWindowTag(false);
	// 몽타주 구독, 공격 참조 정리
	TWeakObjectPtr<UPFAnimInstance> AnimInstanceToReset = BoundAnimInstance;
	const bool bStopMontage = bWasCancelled || (ActorInfo && !ActorInfo->IsNetAuthority() && RemoteInstanceEnded);

	UnbindAttackMontageEnd();
	BoundAnimInstance.Reset();
	bStartingAttackMontage = false;
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
		AbilitySystem && AbilitySystem->GetAnimatingAbility() == this)
	{
		if (bStopMontage) AbilitySystem->CurrentMontageStop(0.1f);
		AbilitySystem->ClearAnimatingAbility(this);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 취소 시 콤보 초기화
	if (bStopMontage)
	{
		if (UPFAnimInstance* AnimInstance = AnimInstanceToReset.Get())
		{
			AnimInstance->ResetAttackCombo();
		}
	}
}

void UPFGA_BasicAttack::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	HandleAttackInputPressed();
}

void UPFGA_BasicAttack::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	HandleAttackInputReleased();
}

// 캐릭터 애니메이션 조회
UPFAnimInstance* UPFGA_BasicAttack::GetAnimInstance(const APFCharacter* Character) const
{
	return Character ? Cast<UPFAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
}

// 공격 몽타주 재생, 종료 이벤트 연결
void UPFGA_BasicAttack::PlayAttackMontage(AActor* AvatarActor)
{
	UnbindAttackMontageEnd();
	// 몽타주 교체 중 종료 처리 보류
	bStartingAttackMontage = true;
	++AttackSequence;
	if (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted)
	{
		UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
		UAnimMontage* Montage = GetAttackMontage(Cast<APFCharacter>(AvatarActor));
		if (!Montage || AbilitySystem->PlayMontage(this, GetCurrentActivationInfo(), Montage, 1.f) <= 0.f)
		{
			bStartingAttackMontage = false;
			CancelCurrentAbility();
			return;
		}
	}
	else
	{
		ExecuteMontageGC(AvatarActor);
	}
	bStartingAttackMontage = false;

	if (!IsActive())
	{
		return;
	}

	// 새 몽타주 인스턴스의 종료 이벤트 연결
	if (UPFAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		ActiveAttackMontage = AnimInstance->GetCurrentActiveMontage();
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(ActiveAttackMontage.Get()))
		{
			ActiveAttackMontageInstanceID = MontageInstance->GetInstanceID();
			MontageInstance->OnMontageEnded.BindUObject(this, &UPFGA_BasicAttack::OnAttackMontageEnded, ActiveAttackMontageInstanceID);
		}
	}

	if (ActiveAttackMontageInstanceID == INDEX_NONE)
	{
		CancelCurrentAbility();
	}
}

// 공격 몽타주 종료 이벤트 해제
void UPFGA_BasicAttack::UnbindAttackMontageEnd()
{
	if (UPFAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(ActiveAttackMontageInstanceID))
		{
			if (MontageInstance->OnMontageEnded.IsBoundToObject(this))
			{
				MontageInstance->OnMontageEnded.Unbind();
			}
		}
	}
	ActiveAttackMontage.Reset();
	ActiveAttackMontageInstanceID = INDEX_NONE;
}

// 공격 누름 처리
void UPFGA_BasicAttack::HandleAttackInputPressed()
{
	TryContinueCombo();
}

// 콤보 입력 구간 이벤트 연결
void UPFGA_BasicAttack::WaitForEvent(AActor*)
{
	UAbilityTask_WaitGameplayEvent* ComboWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, PFGameplayTags::Character_Event_Attack_ComboWindow, nullptr, false, true);
	ComboWindowTask->EventReceived.AddDynamic(this, &UPFGA_BasicAttack::OnComboWindow);
	ComboWindowTask->ReadyForActivation();
}

// 콤보 입력 구간 열기
void UPFGA_BasicAttack::OnComboWindow(FGameplayEventData)
{
	SetComboWindowTag(true);
	TryContinueCombo();
}

// 다음 콤보 준비
bool UPFGA_BasicAttack::PrepareNextCombo()
{
	return true;
}

// 유지된 입력으로 다음 콤보 실행
void UPFGA_BasicAttack::TryContinueCombo()
{
	APFCharacter* Attacker = Cast<APFCharacter>(GetAvatarActorFromActorInfo());
	if (!IsComboWindowOpen() || !Attacker || !IsAttackInputHeld() || !PrepareNextCombo())
	{
		return;
	}

	SetComboWindowTag(false);
	PlayAttackMontage(Attacker);
}

// 공격 해제 처리 (파생 클래스 구현)
void UPFGA_BasicAttack::HandleAttackInputReleased()
{
}

// 공격 입력 유지 여부 조회
bool UPFGA_BasicAttack::IsAttackInputHeld() const
{
	const APFCharacter* Character = Cast<APFCharacter>(GetAvatarActorFromActorInfo());
	return Character && Character->IsAttackCommandActive();
}

// 콤보 입력 구간 조회
bool UPFGA_BasicAttack::IsComboWindowOpen() const
{
	return bComboWindowOpen;
}

// 콤보 입력 구간 설정, 복제
void UPFGA_BasicAttack::SetComboWindowTag(bool bEnabled)
{
	bComboWindowOpen = bEnabled;
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (AbilitySystem && GetAvatarActorFromActorInfo() && GetAvatarActorFromActorInfo()->HasAuthority())
	{
		AbilitySystem->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Attack_ComboWindow,
			bEnabled ? 1 : 0, EGameplayTagReplicationState::TagOnly);
	}
}

// 공격 어빌리티 종료
void UPFGA_BasicAttack::FinishAbility(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	if (bWasCancelled)
	{
		CancelCurrentAbility();
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

// 콤보 종료, 취소 조건 구독
void UPFGA_BasicAttack::WaitForCommonEndConditions()
{
	// 콤보 초기화 이벤트 연결
	UAbilityTask_WaitGameplayEvent* ComboResetTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ComboResetEventTag, nullptr, true, true);
	ComboResetTask->EventReceived.AddDynamic(this, &UPFGA_BasicAttack::OnComboReset);
	ComboResetTask->ReadyForActivation();

	// 공격 차단, 사망 태그 구독
	UAbilityTask_WaitGameplayTagAdded* AttackBlockedTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this, AttackBlockedTag, nullptr, true);
	AttackBlockedTask->Added.AddDynamic(this, &UPFGA_BasicAttack::OnCancellationTagAdded);
	AttackBlockedTask->ReadyForActivation();

	UAbilityTask_WaitGameplayTagAdded* DeadStateTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this, DeadStateTag, nullptr, true);
	DeadStateTask->Added.AddDynamic(this, &UPFGA_BasicAttack::OnCancellationTagAdded);
	DeadStateTask->ReadyForActivation();
}

// 현재 공격 취소
void UPFGA_BasicAttack::CancelCurrentAbility()
{
	if (!IsActive())
	{
		return;
	}

	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

// 콤보 초기화 시 공격 종료
void UPFGA_BasicAttack::OnComboReset(FGameplayEventData Payload)
{
	(void)Payload;
	FinishAbility(false);
}

// 차단, 사망 시 공격 취소
void UPFGA_BasicAttack::OnCancellationTagAdded()
{
	CancelCurrentAbility();
}

// 몽타주 종료에 맞춰 공격 정리
void UPFGA_BasicAttack::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID)
{
	if (!IsActive() || bStartingAttackMontage || MontageInstanceID != ActiveAttackMontageInstanceID
		|| Montage != ActiveAttackMontage.Get())
	{
		return;
	}

	TWeakObjectPtr<UPFAnimInstance> AnimInstance = BoundAnimInstance;
	ActiveAttackMontage.Reset();
	if (bInterrupted)
	{
		CancelCurrentAbility();
		return;
	}

	if (UPFAnimInstance* ValidAnimInstance = AnimInstance.Get())
	{
		ValidAnimInstance->ResetAttackCombo();
	}
	FinishAbility(false);
}
