#pragma once

#include "PortFolio/PortFolio.h"

#include "System/Framework/PFPoolable.h"
#include "Character/PFCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Projectile.generated.h"

// 공통 투사체 클래스
UCLASS(meta=(PrioritizeCategories="Attack Mesh Movement Collision"))
class PORTFOLIO_API AProjectile : public AActor, public IPFPoolable
{
	GENERATED_BODY()

public:
	AProjectile();

	virtual void SpawnFromPool() override;
	virtual void ReturnToPool() override;

	void SetParent(class APFCharacter* character);
	void SetSourceAbility(class UGameplayAbility* InSourceAbility);

	virtual FReturnToPoolDelegate& GetReturnDelegate() override
	{
		return OnReturnToPool;
	}
	// 풀 반환 이벤트
	FReturnToPoolDelegate OnReturnToPool;

protected:
	virtual void SetComponent() PURE_VIRTUAL(AProjectile::SetComponent);
	virtual void SetParticle() PURE_VIRTUAL(AProjectile::SetParticle);
	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	UStaticMeshComponent* MeshCom;

	// 투사체 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = Movement)
	UProjectileMovementComponent* MovementCom;

	UPROPERTY(VisibleAnywhere, Category = Collision)
	UCapsuleComponent* CollisionCom;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Attack, Meta = (AllowPrivateAccess = true))
	float CollisionRadius;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Attack, Meta = (AllowPrivateAccess = true))
	float Radius;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Attack, Meta = (AllowPrivateAccess = true))
	float HalfHeight;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Attack, Meta = (AllowPrivateAccess = true))
	float Damage;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Attack, Meta = (AllowPrivateAccess = true))
	float Speed;


	UPROPERTY()
	TArray<class UParticleSystem*> Particles;

	// 발사 캐릭터
	class APFCharacter* Parent;
	// 피해 출처 어빌리티
	TWeakObjectPtr<class UGameplayAbility> SourceAbility;

	// 풀 반환 예약
	FTimerHandle ReturnTimerHandle;
};
