#include "GAS/Abilities/Attack/PFGA_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "Character/PFCharacter.h"
#include "Animation/PFAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GAS/PFGameplayTags.h"

UPFGA_BasicAttack::UPFGA_BasicAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Attack);
	SetAssetTags(AssetTags);

	AttackingStateTag = PFGameplayTags::Character_State_Attacking;
	ActivationOwnedTags.AddTag(AttackingStateTag);

	AttackBlockedTag = PFGameplayTags::Character_Block_Attack;
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Dead"));
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
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(Handle) : nullptr;
	if (!AbilitySpec || AbilitySpec->IsActive())
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UPFGA_BasicAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor) || !AvatarActor->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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
	// 몽타주 구독, 공격 참조 정리
	TWeakObjectPtr<UPFAnimInstance> AnimInstanceToReset = BoundAnimInstance;

	UnbindAttackMontageEnd();
	BoundAnimInstance.Reset();
	bStartingAttackMontage = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 취소 시 콤보 초기화
	if (bWasCancelled)
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
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	return Mesh ? Cast<UPFAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
}

// 공격 몽타주 재생, 종료 이벤트 연결
void UPFGA_BasicAttack::PlayAttackMontage(AActor* AvatarActor)
{
	UnbindAttackMontageEnd();
	// 몽타주 교체 중 종료 처리 보류
	bStartingAttackMontage = true;
	ExecuteMontageGC(AvatarActor);
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

// 공격 누름 처리 (파생 클래스 구현)
void UPFGA_BasicAttack::HandleAttackInputPressed()
{
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
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(PFGameplayTags::Character_State_Attack_ComboWindow);
}

// 콤보 입력 구간 설정, 복제
void UPFGA_BasicAttack::SetComboWindowTag(bool bEnabled) const
{
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
