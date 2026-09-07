#include "Projectile/Projectile.h"

#include "Abilities/GameplayAbility.h"
#include "System/Subsystems/PFWorldSubsystem.h"

AProjectile::AProjectile() : MeshCom(nullptr), MovementCom(nullptr), CollisionCom(nullptr),  CollisionRadius(10.f), Radius(1.f), HalfHeight(1.f),
Damage(0.f), Speed(0.f), Parent(nullptr)
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void AProjectile::SpawnFromPool()
{
	// 표시, 충돌 활성화
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void AProjectile::ReturnToPool()
{
	// 반환 타이머, 피해 출처 해제
	GetWorldTimerManager().ClearTimer(ReturnTimerHandle);
	SourceAbility.Reset();

	PFLOG(Warning, TEXT("Bullet Returned"));
	// 투사체 비활성화, 풀 반환 알림
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	MovementCom->StopMovementImmediately();
	MovementCom->Deactivate();

	OnReturnToPool.Broadcast(this);
}

// 발사 캐릭터, 피해량 설정
void AProjectile::SetParent(APFCharacter* Character)
{
	Parent = Character;
	Damage = IsValid(Parent) ? Parent->GetDamage() : 0.f;
}

// 피해 출처 어빌리티 설정
void AProjectile::SetSourceAbility(UGameplayAbility* InSourceAbility)
{
	SourceAbility = InSourceAbility;
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 충돌 이벤트 연결
	CollisionCom->SetGenerateOverlapEvents(true);
	CollisionCom->OnComponentBeginOverlap.AddDynamic(this, &AProjectile::OnBeginOverlap);
}

// 충돌 대상별 투사체 처리
void AProjectile::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	if (OtherComp)
	{
		FName CollisionName = OtherComp->GetCollisionProfileName();
		if (CollisionName == FName(TEXT("Wall")))
		{
			ReturnToPool();
		}
		else if (CollisionName == FName(TEXT("Enemy")))
		{
			OtherActor->Destroy();
		}
		else if (CollisionName == FName(TEXT("PFCharacter")))
		{
			AController* Controller = nullptr;
			if (Parent)
			{
				Controller = Parent->GetController();
			}
			FDamageEvent DamageEvent;
			OtherActor->TakeDamage(Damage, DamageEvent, Controller, this);
		}
	}
}
