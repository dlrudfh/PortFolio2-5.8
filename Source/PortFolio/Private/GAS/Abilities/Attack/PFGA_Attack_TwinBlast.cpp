#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/PFAnimInst_TwinBlast.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/PFGameplayTags.h"
#include "Projectile/Bullet.h"
#include "System/Subsystems/PFWorldSubsystem.h"

UPFGA_Attack_TwinBlast::UPFGA_Attack_TwinBlast()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Attack_Twinblast_NormalAttack);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(PFGameplayTags::Character_State_Ultimate);

	MontageGCTag = PFGameplayTags::GameplayCue_Character_Attack_Twinblast_Normal_Montage;
	AttackGCTag = PFGameplayTags::GameplayCue_Character_Attack_Twinblast_Normal_Shoot;
	ShootEventTag = PFGameplayTags::Character_Event_Attack_Shoot;
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
	const APFTwinBlast* TwinBlast = Cast<APFTwinBlast>(ActorInfo->AvatarActor.Get());
	if (!TwinBlast)
	{
		return false;
	}
	return !(TwinBlast->IsPlayerCharacter() && TwinBlast->GetMovementComponent()->IsFalling() && TwinBlast->IsSprinting());
}

void UPFGA_Attack_TwinBlast::WaitForEvent(AActor* AvatarActor)
{
	UAbilityTask_WaitGameplayEvent* ShootEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ShootEventTag, nullptr, false, true);
	ShootEventTask->EventReceived.AddDynamic(this, &UPFGA_Attack_TwinBlast::OnShoot);
	ShootEventTask->ReadyForActivation();
	Super::WaitForEvent(AvatarActor);
}

bool UPFGA_Attack_TwinBlast::PrepareNextCombo()
{
	if (!bShotFired)
	{
		return false;
	}
	bShotFired = false;
	return true;
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
	if (TwinBlast && !TwinBlast->HasAuthority())
	{
		ExecuteShootGC(bCurrentShotLeft);
		bShotFired = true;
		TryContinueCombo();
		return;
	}
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
	UWorld* World = Shooter->GetWorld();
	if (!TryShoot(TwinBlast))
	{
		FinishAbility(true);
		return;
	}

	// 총구, 발사 방향 계산
	const bool bShootLeft = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted
		? bCurrentShotLeft : TwinBlast->ShootLeft;
	const FName SocketName = bShootLeft ? LeftMuzzleSocket : RightMuzzleSocket;

	USkeletalMeshComponent* Mesh = Shooter->GetMesh();
	const FVector MuzzleLocation = Mesh->GetSocketLocation(SocketName);
	FRotator BulletRotation = Mesh->GetSocketRotation(SocketName);
	if (!TwinBlast->IsPlayerCharacter() || TwinBlast->GetCurrentControlMode() != TOPVIEW)
	{
		const FVector AimDirection = (AimPoint - MuzzleLocation).GetSafeNormal();
		BulletRotation = AimDirection.IsNearlyZero() ? Shooter->GetActorRotation() : AimDirection.Rotation();
	}

	// 탄환과 같은 충돌 기준으로 총구 겹침, 몸에서 총구까지의 장애물 확인
	const UPrimitiveComponent* BulletCollision = Cast<UPrimitiveComponent>(GetDefault<ABullet>()->GetRootComponent());
	FCollisionQueryParams MuzzleQueryParams(SCENE_QUERY_STAT(TwinBlastMuzzleObstruction), false, Shooter);
	MuzzleQueryParams.bIgnoreTouches = true;
	TArray<AActor*> AttachedActors;
	Shooter->GetAttachedActors(AttachedActors, true, true);
	MuzzleQueryParams.AddIgnoredActors(AttachedActors);
	const FCollisionShape BulletShape = BulletCollision->GetCollisionShape();
	const FName BulletProfile = BulletCollision->GetCollisionProfileName();
	const FQuat BulletQuat = BulletRotation.Quaternion();
	FVector MuzzleTraceStart = Shooter->GetActorLocation();
	MuzzleTraceStart.Z = MuzzleLocation.Z;
	const bool bMuzzleBlocked = World->OverlapBlockingTestByProfile(
		MuzzleLocation, BulletQuat, BulletProfile, BulletShape, MuzzleQueryParams)
		|| World->SweepTestByProfile(MuzzleTraceStart, MuzzleLocation, BulletQuat, BulletProfile, BulletShape, MuzzleQueryParams);

	// 총구가 열려 있을 때만 탄환 생성, 피해 출처 전달
	if (UPFWorldSubsystem* Pool = World->GetSubsystem<UPFWorldSubsystem>(); !bMuzzleBlocked && Pool)
	{
		AActor* PooledActor = Pool->SpawnActor(ABullet::StaticClass(), MuzzleLocation, BulletRotation, Shooter, this);
		if (!PooledActor)
		{
			PFLOG(Warning, TEXT("Bullet Failed"));
			FinishAbility(true);
			return;
		}
	}

	// 발사 연출, 다음 총구 방향 반영
	ExecuteShootGC(bShootLeft);
	TwinBlast->ShootLeft = !bShootLeft;

	bShotFired = true;
	TryContinueCombo();
}




void UPFGA_Attack_TwinBlast::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	bShotFired = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UPFGA_Attack_TwinBlast::GetAttackMontage(APFCharacter* Character)
{
	const APFTwinBlast* TwinBlast = static_cast<APFTwinBlast*>(Character);
	UPFAnimInstance* AnimInstance = GetAnimInstance(Character);
	bCurrentShotLeft = AttackSequence == 1 ? TwinBlast->ShootLeft : !bCurrentShotLeft;
	const int32 MontageIndex = bCurrentShotLeft
		? etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::LEFTATTACK) : etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::RIGHTATTACK);
	return AnimInstance->GetMontageByIndex(MontageIndex);
}

void UPFGA_Attack_TwinBlast::ExecuteMontageGC(AActor* AvatarActor)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	APFTwinBlast* TwinBlast = static_cast<APFTwinBlast*>(AvatarActor);

	// 발사할 총구 방향을 몽타주 Cue로 전달
	FGameplayCueParameters GCParameters;
	GCParameters.RawMagnitude = TwinBlast->ShootLeft ? 1.f : 0.f;
	AbilitySystem->ExecuteGameplayCue(MontageGCTag, GCParameters);
}

// 총구 방향을 발사 Cue로 전달
void UPFGA_Attack_TwinBlast::ExecuteShootGC(int Var) const
{
	APFTwinBlast* TwinBlast = Cast<APFTwinBlast>(GetAvatarActorFromActorInfo());
	if (TwinBlast && !TwinBlast->HasAuthority())
	{
		TwinBlast->PlayShotEffects(Var != 0);
		return;
	}
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	FGameplayCueParameters GCParameters;
	GCParameters.RawMagnitude = Var ? 1.f : 0.f;
	GCParameters.NormalizedMagnitude = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted ? 1.f : 0.f;
	AbilitySystem->ExecuteGameplayCue(AttackGCTag, GCParameters);
}
