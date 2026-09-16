#include "System/Subsystems/PFWorldSubsystem.h"

#include "Projectile/Projectile.h"
#include "GAS/Components/PFGameplayEffectTriggerComponent.h"
#include "System/Framework/PFPoolable.h"
#include "Kismet/GameplayStatics.h"

// 액터의 풀 반환 이벤트 연결
bool UPFWorldSubsystem::RegisterPoolableActor(AActor* Actor)
{
    if (!Actor)
    {
        return false;
    }

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
    if (!World) return;

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
        PFLOG(Warning, TEXT("Found PoolActor"));
        Actor = Pool.Pop();
        Actor->SetActorLocation(Location);
        Actor->SetActorRotation(Rotation);
    }
    else
    {
        PFLOG(Warning, TEXT("Poolactor Doesn't Exist!"));
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
    if (!PoolActor) return;

    PoolContainer.FindOrAdd(PoolActor->GetClass()).AddUnique(PoolActor);
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
            Snapshot.Transform = Region->GetComponentTransform();
            Snapshot.Extent = Region->GetUnscaledBoxExtent();
        }
    }
}
