#include "Projectile/Bullet.h"

#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Particles/ParticleSystemComponent.h"

ABullet::ABullet()
{
	Speed = 2000.f;

	SetComponent();
	SetParticle();
}

void ABullet::SetComponent()
{
	PrimaryActorTick.bCanEverTick = false;

	// 충돌, 메시 구성
	CollisionCom = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCom"));
	CollisionCom->SetCapsuleSize(CollisionRadius, HalfHeight);
	CollisionCom->SetCollisionProfileName(TEXT("Bullet"));
	RootComponent = CollisionCom;

	MeshCom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshCom"));
	MeshCom->SetupAttachment(RootComponent);

	MeshCom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshCom->SetVisibility(false);
	MeshCom->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	SetActorScale3D(FVector(Radius, Radius, Radius));

	// 총알 이동 설정
	MovementCom = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementCom"));
	MovementCom->UpdatedComponent = CollisionCom;
	MovementCom->InitialSpeed = Speed;
	MovementCom->MaxSpeed = Speed;
	MovementCom->bRotationFollowsVelocity = false;
	MovementCom->ProjectileGravityScale = 0.f;
	MovementCom->bSweepCollision = true;
	MovementCom->bAutoActivate = false;
	MovementCom->Deactivate();

}

void ABullet::SetParticle()
{
	// 총알 궤적, 피격 이펙트 로드
	Particles.SetNum(etoi(PARTICLE_END));

	Particles[etoi(TRAIL)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/"
		"Abilities/Primary/FX/P_TwinBlast_Bullet_Trail_Smoke_Spline.P_TwinBlast_Bullet_Trail_Smoke_Spline'"));
	if (!Particles[etoi(TRAIL)])
	{
		PFLOG(Warning, TEXT("Trail Failed"));
	}

	Particles[etoi(HITCHARACTER)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/"
		"Particles/Abilities/Nitro/FX/P_TwinBlast_Nitro_HitCharacter.P_TwinBlast_Nitro_HitCharacter'"));
	if (!Particles[etoi(HITCHARACTER)])
	{
		PFLOG(Warning, TEXT("HitCharacter Failed"));
	}

	Particles[etoi(HITWALL)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/"
		"Particles/Abilities/Nitro/FX/P_TwinBlast_Nitro_HitWorld.P_TwinBlast_Nitro_HitWorld'"));
	if (!Particles[etoi(HITWALL)])
	{
		PFLOG(Warning, TEXT("HitWall Failed"));
	}
}

void ABullet::SpawnFromPool()
{
	if (!HasAuthority())
	{
		return;
	}

	// 이동 대상 복구, 발사 방향으로 이동 재개
	MovementCom->SetUpdatedComponent(CollisionCom);
	MovementCom->StopMovementImmediately();
	MovementCom->Velocity = GetActorForwardVector() * MovementCom->InitialSpeed;
	MovementCom->UpdateComponentVelocity();
	MovementCom->Activate(true);

	Super::SpawnFromPool();
	if (!bIsActive)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		ReturnTimerHandle,
		this,
		&ABullet::ReturnToPool,
		2.0f,
		false
	);
}

void ABullet::UpdatePoolVisuals()
{
	if (IsValid(TrailComponent))
	{
		TrailComponent->DestroyComponent();
		TrailComponent = nullptr;
	}

	Super::UpdatePoolVisuals();
	if (bIsActive && GetNetMode() != NM_DedicatedServer)
	{
		TrailComponent = UGameplayStatics::SpawnEmitterAttached(Particles[etoi(TRAIL)], MeshCom, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
}

void ABullet::BeginPlay()
{
	MeshCom->SetStaticMesh(GETMESH(MESH_BULLET));
	Super::BeginPlay();
}

void ABullet::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	if (!HasAuthority() || !bIsActive || !OtherActor || OtherActor == Parent)
	{
		return;
	}

	if (!OtherComp)
	{
		return;
	}

	// 캐릭터 피해, 피격 연출 처리
	if (APFCharacter* HitCharacter = Cast<APFCharacter>(OtherActor))
	{
		if (IsValid(Parent) && Parent->IsEnemyCharacter() && HitCharacter->IsEnemyCharacter())
		{
			return;
		}

		if (IsValid(Parent))
		{
			Parent->ApplyAttackDamageTo(HitCharacter, Damage, this, SourceAbility.Get(), &SweepResult);
		}
		Mutlicast_PlayHitEffect(HITCHARACTER, GetActorLocation());
		ReturnToPool();
		return;
	}

	// 환경 충돌 연출, 총알 반환
	const FName CollisionName = OtherComp->GetCollisionProfileName();
	if (CollisionName == FName(TEXT("Enemy")))
	{
		OtherActor->Destroy();
	}
	else if (CollisionName == FName(TEXT("Wall")))
	{
		Mutlicast_PlayHitEffect(HITWALL, GetActorLocation());
	}

	ReturnToPool();
}

// 피격 이펙트 재생
void ABullet::Mutlicast_PlayHitEffect_Implementation(PARTICLE_BULLET particleIndex, FVector HitLocation)
{
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Particles[etoi(particleIndex)], HitLocation, GetActorRotation(), FVector(0.5f));
}
