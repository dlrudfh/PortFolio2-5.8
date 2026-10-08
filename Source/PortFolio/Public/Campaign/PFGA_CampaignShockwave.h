#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"
#include "PFGA_CampaignShockwave.generated.h"

// 서버 시각에 맞춘 범위 공격 예고
UCLASS()
class PORTFOLIO_API APFCampaignShockwaveMarker : public AActor
{
	GENERATED_BODY()
public:
	APFCampaignShockwaveMarker();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UDecalComponent> Decal;
	UPROPERTY(Replicated) double StartedAt = 0.;
	UPROPERTY() TSoftObjectPtr<class UMaterialInterface> TelegraphMaterial;
	UPROPERTY(Replicated) TObjectPtr<class APFCharacter> Caster;
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	bool bPlayedStrike = false;
};

// 광 지휘관 전용 예고 범위 공격
UCLASS()
class PORTFOLIO_API UPFGA_CampaignShockwave : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UPFGA_CampaignShockwave();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	void Detonate();
	void FinishRecovery();
	void HandleDeath(class APFCharacter* Character);
	FTimerHandle DamageTimer;
	FTimerHandle RecoveryTimer;
	TWeakObjectPtr<class APFCharacter> Caster;
	TWeakObjectPtr<APFCampaignShockwaveMarker> Marker;
	FVector Center = FVector::ZeroVector;
};
