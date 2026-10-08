#pragma once

#include "PortFolio/PortFolio.h"

#include "Subsystems/WorldSubsystem.h"

#include "PFWorldSubsystem.generated.h"

class APFCharacter;
class APFCampaignDirector;
class UGameplayAbility;
class UPFGameplayEffectTriggerComponent;

struct FPFJumpBlockRegion
{
    FMatrix WorldToLocal = FMatrix::Identity;
    FVector Extent = FVector::ZeroVector;
};

// 월드 액터 풀, 점프 금지 영역 관리
UCLASS()
class PORTFOLIO_API UPFWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
    void PreparePool(TSubclassOf<AActor> PoolActor, int32 Count);

    AActor* SpawnActor(TSubclassOf<AActor> PoolActor, FVector const& Location, FRotator const& Rotation,
        APFCharacter* ProjectileParent, UGameplayAbility* SourceAbility = nullptr);
    void ReleaseActor(AActor* PoolActor);

    APFCampaignDirector* GetCampaignDirector();
    void RegisterCampaignDirector(APFCampaignDirector* Director);
    void UnregisterCampaignDirector(APFCampaignDirector* Director);

    void RegisterJumpBlockRegion(UPFGameplayEffectTriggerComponent* Region);
    void UnregisterJumpBlockRegion(UPFGameplayEffectTriggerComponent* Region);
    void GetJumpBlockRegions(TArray<FPFJumpBlockRegion>& OutRegions) const;

private:
    bool RegisterPoolableActor(AActor* Actor);
    // 클래스별 대기 액터 풀
    TMap<TSubclassOf<AActor>, TArray<AActor*>> PoolContainer;
    // 현재 월드의 캠페인 관리자
    TWeakObjectPtr<APFCampaignDirector> CampaignDirector;
    bool bCampaignDirectorLookupComplete = false;
    // 월드 내 점프 금지 영역
    TArray<TWeakObjectPtr<UPFGameplayEffectTriggerComponent>> JumpBlockRegions;
};
