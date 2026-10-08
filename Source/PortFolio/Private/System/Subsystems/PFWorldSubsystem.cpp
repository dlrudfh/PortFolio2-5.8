#include "System/Subsystems/PFWorldSubsystem.h"

#include "Projectile/Projectile.h"
#include "Campaign/PFCampaignDirector.h"
#include "EngineUtils.h"
#include "GAS/Components/PFGameplayEffectTriggerComponent.h"
#include "System/Framework/PFPoolable.h"
#include "Kismet/GameplayStatics.h"

// 액터의 풀 반환 이벤트 연결
bool UPFWorldSubsystem::RegisterPoolableActor(AActor* Actor)
{
    if (IPFPoolable* Poolable = Cast<IPFPoolable>(Actor))
    {
        Poolable->GetReturnDelegate().AddUObject(this, &UPFWorldSubsystem::ReleaseActor);
        return true;
    }

    return false;
}

// 대기 액터 미리 생성
void UPFWorldSubsystem::PreparePool(TSubclassOf<AActor> PoolActor, int32 Count)
{
    if (!PoolActor) return;

    UWorld* World = GetWorld();

    TArray<AActor*>& Pool = PoolContainer.FindOrAdd(PoolActor);

    Pool.Reserve(Pool.Num() + Count);

    for (int32 i = 0; i < Count; i++)
    {
        AActor* Actor = World->SpawnActor<AActor>(PoolActor);
        if (!Actor) continue;

        if (RegisterPoolableActor(Actor))
        {
            IPFPoolable* Poolable = Cast<IPFPoolable>(Actor);
            Poolable->ReturnToPool();
        }
        else
        {
            return;
        }
    }
}

// 풀 액터 생성, 재사용
AActor* UPFWorldSubsystem::SpawnActor(TSubclassOf<AActor> PoolActor, FVector const& Location, FRotator const& Rotation,
    APFCharacter* ProjectileParent, UGameplayAbility* SourceAbility)
{
    if (!PoolActor) return nullptr;

    TArray<AActor*>& Pool = PoolContainer.FindOrAdd(PoolActor);

    AActor* Actor = nullptr;

    // 대기 액터 재사용 또는 추가 생성
    if (Pool.Num() > 0)
    {
        Actor = Pool.Pop(EAllowShrinking::No);
        Actor->SetActorLocationAndRotation(Location, Rotation);
    }
    else
    {
        const FTransform SpawnTransform(Rotation, Location);
        Actor = GetWorld()->SpawnActorDeferred<AActor>(
            PoolActor,
            SpawnTransform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Actor)
        {
            Actor->SetActorHiddenInGame(true);
            Actor->SetActorEnableCollision(false);
            if (AProjectile* Projectile = Cast<AProjectile>(Actor))
            {
                Projectile->SetParent(ProjectileParent);
                Projectile->SetSourceAbility(SourceAbility);
            }
            Actor = UGameplayStatics::FinishSpawningActor(Actor, SpawnTransform);
            RegisterPoolableActor(Actor);
        }
    }

    if (!Actor) return nullptr;

    if (AProjectile* Projectile = Cast<AProjectile>(Actor))
    {
        Projectile->SetParent(ProjectileParent);
        Projectile->SetSourceAbility(SourceAbility);
    }

    // 설정 완료 후 액터 활성화
    if (IPFPoolable* Poolable = Cast<IPFPoolable>(Actor))
    {
        Poolable->SpawnFromPool();
    }

    return Actor;
}

// 반환된 액터를 풀에 보관
void UPFWorldSubsystem::ReleaseActor(AActor* PoolActor)
{
    PoolContainer.FindOrAdd(PoolActor->GetClass()).AddUnique(PoolActor);
}

// 월드 최초 조회 이후 캠페인 관리자 재사용
APFCampaignDirector* UPFWorldSubsystem::GetCampaignDirector()
{
    if (!bCampaignDirectorLookupComplete)
    {
        bCampaignDirectorLookupComplete = true;
        for (TActorIterator<APFCampaignDirector> It(GetWorld()); It; ++It)
        {
            CampaignDirector = *It;
            break;
        }
    }
    return CampaignDirector.Get();
}

// 새 캠페인 관리자 등록
void UPFWorldSubsystem::RegisterCampaignDirector(APFCampaignDirector* Director)
{
    if (!CampaignDirector.IsValid()) CampaignDirector = Director;
    bCampaignDirectorLookupComplete = true;
}

// 종료된 캠페인 관리자 해제
void UPFWorldSubsystem::UnregisterCampaignDirector(APFCampaignDirector* Director)
{
    if (CampaignDirector.Get() == Director) CampaignDirector.Reset();
}

// 점프 금지 영역 등록
void UPFWorldSubsystem::RegisterJumpBlockRegion(UPFGameplayEffectTriggerComponent* Region)
{
    JumpBlockRegions.AddUnique(Region);
}

// 점프 금지 영역 해제
void UPFWorldSubsystem::UnregisterJumpBlockRegion(UPFGameplayEffectTriggerComponent* Region)
{
    JumpBlockRegions.Remove(Region);
}

// 쿼리별 점프 금지 영역 스냅샷
void UPFWorldSubsystem::GetJumpBlockRegions(TArray<FPFJumpBlockRegion>& OutRegions) const
{
    OutRegions.Reset();
    for (const TWeakObjectPtr<UPFGameplayEffectTriggerComponent>& WeakRegion : JumpBlockRegions)
    {
        const UPFGameplayEffectTriggerComponent* Region = WeakRegion.Get();
        if (Region && Region->IsRegistered() && Region->IsCollisionEnabled()
            && Region->GetGenerateOverlapEvents() && Region->GrantsJumpBlock())
        {
            FPFJumpBlockRegion& Snapshot = OutRegions.AddDefaulted_GetRef();
            Snapshot.WorldToLocal = Region->GetComponentTransform().ToInverseMatrixWithScale();
            Snapshot.Extent = Region->GetUnscaledBoxExtent();
        }
    }
}
