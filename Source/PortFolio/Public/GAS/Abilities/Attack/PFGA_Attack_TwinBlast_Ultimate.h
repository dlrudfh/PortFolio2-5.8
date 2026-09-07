#pragma once

#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast.h"
#include "Engine/TimerHandle.h"

#include "PFGA_Attack_TwinBlast_Ultimate.generated.h"

// 트윈블라스트 궁극기 공격 어빌리티
UCLASS()
class PORTFOLIO_API UPFGA_Attack_TwinBlast_Ultimate : public UPFGA_Attack_TwinBlast
{
	GENERATED_BODY()

public:
	UPFGA_Attack_TwinBlast_Ultimate();

protected:
	virtual bool CanActivateTwinBlastAttack(const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual void WaitForEvent(AActor* AvatarActor) override;
	virtual bool TryShoot(APFTwinBlast* TwinBlast) override;
	virtual void HandleAttackInputReleased() override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void FireNextShot();

	static constexpr float ShootInterval = 0.05f;
	float LastShootTime = -BIG_NUMBER;
	// 궁극기 연사 타이머
	FTimerHandle UltimateFireTimerHandle;
};
