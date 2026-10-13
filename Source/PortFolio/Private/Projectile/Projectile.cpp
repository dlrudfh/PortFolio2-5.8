#include "Projectile/Projectile.h"

#include "Abilities/GameplayAbility.h"
#include "System/Subsystems/PFWorldSubsystem.h"

// 발사 세대, 활성 상태, 이동 정보 일괄 직렬화
bool FPFProjectilePoolState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar.SerializeIntPacked(Generation);
	uint8 Active = bActive ? 1 : 0;
	Ar.SerializeBits(&Active, 1);
	if (Ar.IsLoading()) bActive = Active != 0;
	bool bMovementSuccess = true;
	const bool bMapped = Movement.NetSerialize(Ar, Map, bMovementSuccess);
	bOutSuccess = bMovementSuccess && !Ar.IsError();
	return bMapped;
}

AProjectile::AProjectile() : MeshCom(nullptr), MovementCom(nullptr), CollisionCom(nullptr),  CollisionRadius(10.f), Radius(1.f), HalfHeight(1.f),
Damage(0.f), Speed(0.f), Parent(nullptr)
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
}

void AProjectile::SpawnFromPool()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ReturnTimerHandle);
	++PoolState.Generation;
	if (PoolState.Generation == 0) ++PoolState.Generation;
	PoolState.bActive = true;
	RefreshPoolMovement();
	ApplyPoolState();
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

	PoolState.bActive = false;
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	MovementCom->StopMovementImmediately();
	MovementCom->Deactivate();
	RefreshPoolMovement();
	ApplyPoolState();
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
	if (!HasAuthority()) ApplyPoolState();
}

void AProjectile::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	if (HasAuthority() && bIsActive) RefreshPoolMovement();
}

void AProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectile, PoolState);
}

// 복제된 발사 상태 반영
void AProjectile::OnRep_PoolState()
{
	if (HasActorBegunPlay())
	{
		ApplyPoolState();
	}
}

// 현재 발사의 이동 정보 갱신
void AProjectile::RefreshPoolMovement()
{
	PoolState.Movement.Location = FRepMovement::RebaseOntoZeroOrigin(GetActorLocation(), this);
	PoolState.Movement.Rotation = GetActorRotation();
	PoolState.Movement.LinearVelocity = GetVelocity();
}

// 이전 궤적 정리 후 현재 발사의 위치, 표시 적용
void AProjectile::ApplyPoolState()
{
	const bool bNewGeneration = AppliedPoolGeneration != PoolState.Generation;
	if (bIsActive && (!PoolState.bActive || bNewGeneration))
	{
		bIsActive = false;
		UpdatePoolVisuals();
	}
	if (!HasAuthority() && PoolState.Generation != 0)
	{
		SetActorLocationAndRotation(FRepMovement::RebaseOntoLocalOrigin(PoolState.Movement.Location, this),
			PoolState.Movement.Rotation, false, nullptr, bNewGeneration ? ETeleportType::TeleportPhysics : ETeleportType::None);
	}
	if (PoolState.bActive && !bIsActive)
	{
		bIsActive = true;
		UpdatePoolVisuals();
	}
	AppliedPoolGeneration = PoolState.Generation;
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
