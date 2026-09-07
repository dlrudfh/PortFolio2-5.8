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

private:
	virtual void BeginPlay() override;

	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	UFUNCTION(NetMulticast, Reliable)
	void Mutlicast_PlayHitEffect(PARTICLE_BULLET particleIndex);
};
