#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/PFAnimInst_TwinBlast.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/PFGameplayTags.h"
#include "Projectile/Bullet.h"
#include "System/Subsystems/PFWorldSubsystem.h"

UPFGA_Attack_TwinBlast::UPFGA_Attack_TwinBlast()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRetriggerInstancedAbility = false;

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Attack_Twinblast_NormalAttack);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Ultimate")));

	MontageGCTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Character.Attack.Twinblast.Normal.Montage"));
	AttackGCTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Character.Attack.Twinblast.Normal.Shoot"));
	ShootEventTag = PFGameplayTags::Character_Event_Attack_Shoot;
	ComboWindowEventTag = PFGameplayTags::Character_Event_Attack_ComboWindow;
	LeftMuzzleSocket = FName("Muzzle_02");
	RightMuzzleSocket = FName("Muzzle_01");
}

bool UPFGA_Attack_TwinBlast::CanActivateAbility(
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

	return CanActivateTwinBlastAttack(ActorInfo);
}

// 트윈블라스트 공격 조건 확인
bool UPFGA_Attack_TwinBlast::CanActivateTwinBlastAttack(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const APFTwinBlast* TwinBlast = ActorInfo ? Cast<APFTwinBlast>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!TwinBlast)
	{
		return false;
	}
	return !(TwinBlast->IsPlayerCharacter() && TwinBlast->GetMovementComponent()->IsFalling() && TwinBlast->IsSprinting());
}

void UPFGA_Attack_TwinBlast::WaitForEvent(AActor*)
{
	// 발사, 콤보 이벤트 연결
	UAbilityTask_WaitGameplayEvent* ShootEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ShootEventTag, nullptr, false, true);
	ShootEventTask->EventReceived.AddDynamic(this, &UPFGA_Attack_TwinBlast::OnShoot);
	ShootEventTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* ComboWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ComboWindowEventTag, nullptr, false, true);
	ComboWindowTask->EventReceived.AddDynamic(this, &UPFGA_Attack_TwinBlast::OnComboWindow);
	ComboWindowTask->ReadyForActivation();

}

// 기본 발사 허용
bool UPFGA_Attack_TwinBlast::TryShoot(APFTwinBlast*)
{
	return true;
}

// 발사 이벤트 처리
void UPFGA_Attack_TwinBlast::OnShoot(FGameplayEventData Payload)
{
	(void)Payload;
	if (!IsActive() || bShotFired)
	{
		return;
	}
	APFTwinBlast* TwinBlast = Cast<APFTwinBlast>(GetAvatarActorFromActorInfo());
	FVector AimPoint;
	if (!IsValid(TwinBlast) || !TwinBlast->TryGetAttackAim(AimPoint))
	{
		FinishAbility(true);
		return;
	}
	StartShoot(TwinBlast, AimPoint);
}

// 총구에서 조준점으로 발사
void UPFGA_Attack_TwinBlast::StartShoot(APFTwinBlast* TwinBlast, const FVector& AimPoint)
{
	APFCharacter* Shooter = TwinBlast;
	if (!Shooter || !Shooter->HasAuthority())
	{
		FinishAbility(true);
		return;
	}

	UWorld* World = Shooter->GetWorld();
	if (!World)
	{
		FinishAbility(true);
		return;
	}

	if (!TryShoot(TwinBlast))
	{
		FinishAbility(true);
		return;
	}

	// 총구, 발사 방향 계산
	const bool bShootLeft = TwinBlast->ShootLeft;
	const FName SocketName = bShootLeft ? LeftMuzzleSocket : RightMuzzleSocket;

	USkeletalMeshComponent* Mesh = Shooter->GetMesh();
	const FVector MuzzleLocation = Mesh->GetSocketLocation(SocketName);
	FRotator BulletRotation = Mesh->GetSocketRotation(SocketName);
	if (!TwinBlast->IsPlayerCharacter() || TwinBlast->GetCurrentControlMode() != TOPVIEW)
	{
		const FVector AimDirection = (AimPoint - MuzzleLocation).GetSafeNormal();
		BulletRotation = AimDirection.IsNearlyZero() ? Shooter->GetActorRotation() : AimDirection.Rotation();
	}

	// 풀에서 총알 생성, 피해 출처 전달
	if (UPFWorldSubsystem* Pool = World->GetSubsystem<UPFWorldSubsystem>())
	{
		AActor* PooledActor = Pool->SpawnActor(ABullet::StaticClass(), MuzzleLocation, BulletRotation, Shooter, this);
		if (!Cast<ABullet>(PooledActor))
		{
			PFLOG(Warning, TEXT("Bullet Failed"));
			FinishAbility(true);
			return;
		}
	}

	// 발사 연출, 다음 총구 방향 반영
	ExecuteShootGC(bShootLeft);
	TwinBlast->ShootLeft = !TwinBlast->ShootLeft;

	bShotFired = true;
	TryContinueCombo();
}

// 콤보 입력 구간 열기
void UPFGA_Attack_TwinBlast::OnComboWindow(FGameplayEventData Payload)
{
	(void)Payload;
	SetComboWindowTag(true);
	TryContinueCombo();
}

void UPFGA_Attack_TwinBlast::HandleAttackInputPressed()
{
	TryContinueCombo();
}

// 발사 후 다음 공격 연결
void UPFGA_Attack_TwinBlast::TryContinueCombo()
{
	if (!bShotFired || !IsComboWindowOpen())
	{
		return;
	}

	APFCharacter* Shooter = Cast<APFCharacter>(GetAvatarActorFromActorInfo());
	if (!Shooter || !IsAttackInputHeld())
	{
		return;
	}

	bShotFired = false;
	SetComboWindowTag(false);
	PlayAttackMontage(Shooter);
}

void UPFGA_Attack_TwinBlast::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	bShotFired = false;
	SetComboWindowTag(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPFGA_Attack_TwinBlast::ExecuteMontageGC(AActor* AvatarActor)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	APFTwinBlast* TwinBlast = Cast<APFTwinBlast>(AvatarActor);
	if (!TwinBlast || !AbilitySystem)
	{
		return;
	}

	// 발사할 총구 방향을 몽타주 Cue로 전달
	FGameplayCueParameters GCParameters;
	GCParameters.RawMagnitude = TwinBlast->ShootLeft ? 1.f : 0.f;
	AbilitySystem->ExecuteGameplayCue(MontageGCTag, GCParameters);
}

// 총구 방향을 발사 Cue로 전달
void UPFGA_Attack_TwinBlast::ExecuteShootGC(int Var) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	FGameplayCueParameters GCParameters;
	GCParameters.RawMagnitude = Var ? 1.f : 0.f;
	AbilitySystem->ExecuteGameplayCue(AttackGCTag, GCParameters);
}
