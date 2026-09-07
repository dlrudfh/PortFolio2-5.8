#pragma once

#include "GAS/Abilities/Attack/PFGA_BasicAttack.h"

#include "PFGA_Attack_TwinBlast.generated.h"

class APFTwinBlast;
class APFEnemyTwinblast;
class APFCharacter;
class UPFAnimInst_TwinBlast;

// 트윈블라스트 기본 공격 어빌리티
UCLASS()
class PORTFOLIO_API UPFGA_Attack_TwinBlast : public UPFGA_BasicAttack
{
	GENERATED_BODY()

public:
	UPFGA_Attack_TwinBlast();

protected:
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CanActivateTwinBlastAttack(const FGameplayAbilityActorInfo* ActorInfo) const;

	virtual void WaitForEvent(AActor* AvatarActor) override;
	virtual void ExecuteMontageGC(AActor* AvatarActor) override;
	virtual void ExecuteShootGC(int Var) const;

	virtual bool TryShoot(APFTwinBlast* TwinBlast);
	void StartShoot(APFTwinBlast* TwinBlast, APFEnemyTwinblast* EnemyTwinBlast, const FVector& AimPoint);
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void HandleAttackInputPressed() override;

private:
	UFUNCTION()
	void OnShoot(FGameplayEventData Payload);
	UFUNCTION()
	void OnComboWindow(FGameplayEventData Payload);
	void TryContinueCombo();


protected:
	// 발사 연출 Cue 태그
	FGameplayTag AttackGCTag;
	// 좌우 총구 소켓
	FName LeftMuzzleSocket = NAME_None;
	FName RightMuzzleSocket = NAME_None;

private:
	// 발사, 콤보 이벤트 태그
	FGameplayTag ShootEventTag;
	FGameplayTag ComboWindowEventTag;
	bool bShotFired = false;
};
