#pragma once

#include "PortFolio/PortFolio.h"

#include "Subsystems/WorldSubsystem.h"

#include "PFWorldSubsystem.generated.h"

class APFCharacter;
class UGameplayAbility;

// 월드 액터 풀 관리 클래스
UCLASS()
class PORTFOLIO_API UPFWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
    void PreparePool(TSubclassOf<AActor> PoolActor, int32 Count);

    AActor* SpawnActor(TSubclassOf<AActor> PoolActor, FVector const& Location, FRotator const& Rotation,
        APFCharacter* ProjectileParent, UGameplayAbility* SourceAbility = nullptr);
    void ReleaseActor(AActor* PoolActor);

private:
    bool RegisterPoolableActor(AActor* Actor);
    // 클래스별 대기 액터 풀
    TMap<TSubclassOf<AActor>, TArray<AActor*>> PoolContainer;
};
