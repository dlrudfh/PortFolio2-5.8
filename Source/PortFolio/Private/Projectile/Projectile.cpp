#include "Projectile/Projectile.h"

#include "Abilities/GameplayAbility.h"
#include "System/Subsystems/PFWorldSubsystem.h"

AProjectile::AProjectile() : MeshCom(nullptr), MovementCom(nullptr), CollisionCom(nullptr),  CollisionRadius(10.f), Radius(1.f), HalfHeight(1.f),
Damage(0.f), Speed(0.f), Parent(nullptr)
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetActorEnableCollision(false);
}

void AProjectile::SpawnFromPool()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ReturnTimerHandle);
	bIsActive = true;
	UpdatePoolVisuals();
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);
	ForceNetUpdate();
}

void AProjectile::ReturnToPool()
{
	if (!HasAuthority())
	{
		return;
	}

	// 반환 타이머, 피해 출처 해제
	GetWorldTimerManager().ClearTimer(ReturnTimerHandle);
	SourceAbility.Reset();

	bIsActive = false;
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	MovementCom->StopMovementImmediately();
	MovementCom->Deactivate();
	UpdatePoolVisuals();
	ForceNetUpdate();
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

	// 이동, 충돌은 서버에서만 처리
	MovementCom->StopMovementImmediately();
	MovementCom->Deactivate();
	SetActorEnableCollision(false);
	CollisionCom->SetGenerateOverlapEvents(HasAuthority());
	if (HasAuthority())
	{
		CollisionCom->OnComponentBeginOverlap.AddDynamic(this, &AProjectile::OnBeginOverlap);
	}
	UpdatePoolVisuals();
}

void AProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectile, bIsActive);
}

// 복제된 활성 상태 반영
void AProjectile::OnRep_IsActive()
{
	if (HasActorBegunPlay())
	{
		UpdatePoolVisuals();
	}
}

// 풀 활성 상태에 따른 표시 갱신
void AProjectile::UpdatePoolVisuals()
{
	// 반환 상태가 계속 복제되도록 메시만 숨김
	SetActorHiddenInGame(false);
	MeshCom->SetVisibility(bIsActive, true);
}

// 충돌 대상별 투사체 처리
void AProjectile::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsActive || !OtherActor || OtherActor == Parent)
	{
		return;
	}

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
