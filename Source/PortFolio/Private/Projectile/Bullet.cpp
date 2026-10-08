#include "Projectile/Bullet.h"
#include "Campaign/PFCampaignDirector.h"

#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "SceneView.h"
#include "UObject/ConstructorHelpers.h"

ABullet::ABullet()
{
	Speed = 2000.f;

	SetComponent();
	SetParticle();

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BulletMarkFinder(
		TEXT("/Game/GameData/Materials/Decals/M_TwinBlast_BulletMark.M_TwinBlast_BulletMark"));
	BulletMarkMaterial = BulletMarkFinder.Object;
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
	static const TCHAR* const Paths[] =
	{
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Primary/FX/P_TwinBlast_Bullet_Trail_Smoke_Spline.P_TwinBlast_Bullet_Trail_Smoke_Spline'"),
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Nitro/FX/P_TwinBlast_Nitro_HitCharacter.P_TwinBlast_Nitro_HitCharacter'"),
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Nitro/FX/P_TwinBlast_Nitro_HitWorld.P_TwinBlast_Nitro_HitWorld'")
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(PARTICLE_END));
	UPFGameInstanceSubsystem::LoadAssets(Particles, Paths);
}

void ABullet::SpawnFromPool()
{
	if (!HasAuthority())
	{
		return;
	}

	// 이동 대상 복구, 발사 방향으로 이동 재개
	bImpactHandled = false;
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
	Super::UpdatePoolVisuals();
	if (GetNetMode() == NM_DedicatedServer) return;

	if (bIsActive)
	{
		if (!IsValid(TrailComponent))
		{
			TrailComponent = UGameplayStatics::SpawnEmitterAttached(Particles[etoi(TRAIL)], MeshCom, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false, EPSCPoolMethod::None, false);
		}
		if (TrailComponent) TrailComponent->ActivateSystem(true);
	}
	else if (IsValid(TrailComponent))
	{
		TrailComponent->DeactivateImmediate();
	}
}

void ABullet::BeginPlay()
{
	MeshCom->SetStaticMesh(GETMESH(MESH_BULLET));
	Super::BeginPlay();
	if (HasAuthority())
	{
		MovementCom->OnProjectileStop.AddDynamic(this, &ABullet::OnProjectileStop);
	}
}

void ABullet::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	if (!HasAuthority() || !bIsActive || bImpactHandled || !OtherActor || OtherActor == Parent)
	{
		return;
	}

	// 캐릭터 피해, 피격 연출 처리
	if (APFCharacter* HitCharacter = Cast<APFCharacter>(OtherActor))
	{
		if (APFCampaignDirector::AreFriendly(Parent, HitCharacter)) return;
		if (IsValid(Parent) && Parent->IsEnemyCharacter() && HitCharacter->IsEnemyCharacter())
		{
			return;
		}

		bImpactHandled = true;
		if (IsValid(Parent))
		{
			Parent->ApplyAttackDamageTo(HitCharacter, Damage, this, SourceAbility.Get(), &SweepResult);
		}
		FHitResult CharacterHit = SweepResult;
		CharacterHit.ImpactPoint = GetActorLocation();
		CharacterHit.ImpactNormal = -GetActorForwardVector();
		Mutlicast_PlayHitEffect(HITCHARACTER, CharacterHit, 0.f, false);
		ReturnToPool();
	}
}

// 환경 명중 연출, 총알 반환
void ABullet::OnProjectileStop(const FHitResult& Hit)
{
	if (!HasAuthority() || !bIsActive || bImpactHandled)
	{
		return;
	}

	bImpactHandled = true;
	if (Hit.bBlockingHit && !Cast<APFCharacter>(Hit.GetActor()))
	{
		FHitResult SurfaceHit;
		const bool bHasSurface = FindBulletMarkSurface(Hit, SurfaceHit);
		Mutlicast_PlayHitEffect(HITWALL, bHasSurface ? SurfaceHit : Hit, FMath::FRandRange(0.f, 360.f), bHasSurface);
	}

	ReturnToPool();
}

// 충돌한 메시의 실제 표면 확인
bool ABullet::FindBulletMarkSurface(const FHitResult& Hit, FHitResult& SurfaceHit) const
{
	UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	if (!IsValid(HitComponent) || !HitComponent->bReceivesDecals || Normal.IsNearlyZero())
	{
		return false;
	}
	if (const UBodySetup* BodySetup = HitComponent->GetBodySetup())
	{
		const ECollisionTraceFlag TraceFlag = BodySetup->CollisionTraceFlag == CTF_UseDefault
			? UPhysicsSettings::Get()->DefaultShapeComplexity : BodySetup->CollisionTraceFlag;
		if (TraceFlag == CTF_UseSimpleAsComplex)
		{
			return false;
		}
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(TwinBlastBulletMark), true);
	const FVector TraceStart = Hit.ImpactPoint + Normal * 30.f;
	const FVector TraceEnd = Hit.ImpactPoint - Normal * 30.f;
	if (!HitComponent->LineTraceComponent(SurfaceHit, TraceStart, TraceEnd, TraceParams)
		|| SurfaceHit.bStartPenetrating || SurfaceHit.ImpactNormal.IsNearlyZero())
	{
		return false;
	}

	if (Cast<UInstancedStaticMeshComponent>(HitComponent) && Hit.Item != INDEX_NONE && SurfaceHit.Item != Hit.Item)
	{
		return false;
	}

	SurfaceHit.bBlockingHit = true;
	return true;
}

// 피격 이펙트, 표면 탄흔 재생
void ABullet::Mutlicast_PlayHitEffect_Implementation(PARTICLE_BULLET particleIndex, const FHitResult& Hit,
	float DecalRoll, bool bSpawnBulletMark)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Particles[etoi(particleIndex)], Hit.ImpactPoint,
		GetActorRotation(), FVector(0.5f));
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	if (!bSpawnBulletMark || particleIndex != HITWALL || !IsValid(BulletMarkMaterial) || Normal.IsNearlyZero())
	{
		return;
	}

	const FVector DecalLocation = Hit.ImpactPoint + Normal * 0.1f;
	const FVector DecalSize(1.f, 10.f, 10.f);
	FRotator DecalRotation = Normal.Rotation();
	DecalRotation.Roll = DecalRoll;
	UDecalComponent* Decal = nullptr;
	if (UPrimitiveComponent* HitComponent = Hit.GetComponent(); IsValid(HitComponent))
	{
		Decal = UGameplayStatics::SpawnDecalAttached(BulletMarkMaterial, DecalSize, HitComponent, NAME_None,
			DecalLocation, DecalRotation, EAttachLocation::KeepWorldPosition, 0.f);
	}
	else
	{
		Decal = UGameplayStatics::SpawnDecalAtLocation(this, BulletMarkMaterial, DecalSize,
			DecalLocation, DecalRotation, 0.f);
	}

	if (IsValid(Decal))
	{
		// 현재 화면의 투영 크기를 거리 페이드 기준으로 환산
		float ProjectionScale = 1.f;
		const APlayerController* LocalController = UGameplayStatics::GetPlayerController(this, 0);
		const ULocalPlayer* LocalPlayer = LocalController ? LocalController->GetLocalPlayer() : nullptr;
		FSceneViewProjectionData ProjectionData;
		if (LocalPlayer && LocalPlayer->ViewportClient
			&& LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData)
			&& ProjectionData.IsPerspectiveProjection())
		{
			ProjectionScale = FMath::Max(static_cast<float>(ProjectionData.ProjectionMatrix.M[0][0])
				* ProjectionData.GetConstrainedViewRect().Width() / 1200.f, UE_SMALL_NUMBER);
		}
		const float DecalRadius = static_cast<float>(Decal->GetTransformIncludingDecalSize().GetScale3D().GetAbsMax());
		Decal->SetFadeScreenSize(DecalRadius * ProjectionScale / FMath::Max(BulletMarkFadeDistance, 100.f));
		Decal->SetFadeOut(2.f, 3.f, false);
	}
}
