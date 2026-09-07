#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast_Ultimate.h"

#include "Character/TwinBlast/PFTwinBlast.h"
#include "GAS/PFGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPFGA_Attack_TwinBlast_Ultimate::UPFGA_Attack_TwinBlast_Ultimate()
{
	const FGameplayTag UltimateState = FGameplayTag::RequestGameplayTag(FName("Character.State.Ultimate"));
	ActivationBlockedTags.RemoveTag(UltimateState);
	ActivationRequiredTags.AddTag(UltimateState);

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.RemoveTag(PFGameplayTags::Character_Ability_Attack_Twinblast_NormalAttack);
	AssetTags.AddTag(PFGameplayTags::Character_Ability_Attack_Twinblast_UltAttack);
	SetAssetTags(AssetTags);

	MontageGCTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Character.Attack.Twinblast.Ultimate.Montage"));
	AttackGCTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Character.Attack.Twinblast.Ultimate.Shoot"));
	LeftMuzzleSocket = FName("Muzzle_04");
	RightMuzzleSocket = FName("Muzzle_03");
}

bool UPFGA_Attack_TwinBlast_Ultimate::CanActivateTwinBlastAttack(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const APFTwinBlast* TwinBlast = ActorInfo ? Cast<APFTwinBlast>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!TwinBlast)
	{
		return false;
	}

	if (TwinBlast->GetMana() < 1.f)
	{
		return false;
	}

	const UWorld* World = TwinBlast->GetWorld();
	return World && World->GetTimeSeconds() - LastShootTime >= ShootInterval;
}

void UPFGA_Attack_TwinBlast_Ultimate::WaitForEvent(AActor* AvatarActor)
{
	APFTwinBlast* TwinBlast = static_cast<APFTwinBlast*>(AvatarActor);
	UWorld* World = TwinBlast->GetWorld();

	// 첫 발사 후 연사 예약
	LastShootTime = World->GetTimeSeconds();
	StartShoot(TwinBlast, nullptr, TwinBlast->GetAimPoint());
	if (IsActive())
	{
		World->GetTimerManager().SetTimer(
			UltimateFireTimerHandle,
			this,
			&UPFGA_Attack_TwinBlast_Ultimate::FireNextShot,
			ShootInterval,
			true,
			ShootInterval);
	}
}

bool UPFGA_Attack_TwinBlast_Ultimate::TryShoot(APFTwinBlast* TwinBlast)
{
	return TwinBlast->TryUseMana(1.f);
}

void UPFGA_Attack_TwinBlast_Ultimate::HandleAttackInputReleased()
{
	FinishAbility(false);
}

void UPFGA_Attack_TwinBlast_Ultimate::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 연사 타이머 해제
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UltimateFireTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 입력 유지 중 궁극기 연사
void UPFGA_Attack_TwinBlast_Ultimate::FireNextShot()
{
	if (!IsAttackInputHeld())
	{
		FinishAbility(false);
		return;
	}

	APFTwinBlast* TwinBlast = Cast<APFTwinBlast>(GetAvatarActorFromActorInfo());
	UWorld* World = TwinBlast ? TwinBlast->GetWorld() : nullptr;
	if (!TwinBlast || !World)
	{
		FinishAbility(true);
		return;
	}

	LastShootTime = World->GetTimeSeconds();
	StartShoot(TwinBlast, nullptr, TwinBlast->GetAimPoint());
}
