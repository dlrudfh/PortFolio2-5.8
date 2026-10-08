#pragma once

#include "Projectile/Projectile.h"
#include "Bullet.generated.h"

UENUM()
enum class PARTICLE_BULLET
{
	TRAIL,
	HITCHARACTER,
	HITWALL,
	PARTICLE_END
};

// 총알 클래스
UCLASS(meta=(PrioritizeCategories="Attack Mesh Movement Collision"))
class PORTFOLIO_API ABullet : public AProjectile
{
	GENERATED_BODY()
	
	using enum PARTICLE_BULLET;

public:	
	ABullet();

protected:
	virtual void SetComponent() override;
	virtual void SetParticle() override;

	virtual void SpawnFromPool() override;
	virtual void UpdatePoolVisuals() override;

private:
	virtual void BeginPlay() override;

	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	UFUNCTION()
	void OnProjectileStop(const FHitResult& Hit);

	bool FindBulletMarkSurface(const FHitResult& Hit, FHitResult& SurfaceHit) const;

	UFUNCTION(NetMulticast, Reliable)
	void Mutlicast_PlayHitEffect(PARTICLE_BULLET particleIndex, const FHitResult& Hit, float DecalRoll, bool bSpawnBulletMark);

	// 현재 발사의 궤적 이펙트
	UPROPERTY(Transient)
	TObjectPtr<class UParticleSystemComponent> TrailComponent = nullptr;

	// 공통 탄흔 머티리얼
	UPROPERTY()
	TObjectPtr<class UMaterialInterface> BulletMarkMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|BulletMark", meta = (ClampMin = "100.0", Units = "cm"))
	float BulletMarkFadeDistance = 1000.f;

	bool bImpactHandled = false;
};
