#pragma once

#include "PortFolio/PortFolio.h"

#include "Props/PFItem.h"

#include "PFChest.generated.h"

// 보상 상자 클래스
UCLASS(meta=(PrioritizeCategories="PFChest Chest"))
class PORTFOLIO_API APFChest : public AActor
{
	GENERATED_BODY()

public:	
	enum class ECHESTSTATE
	{
		CHEST_CLOSED,
		CHEST_OPENED,
		CHEST_EMPTY,
		CHEST_END
	}; using enum ECHESTSTATE;

	APFChest();
	void ChestOpen();
	UFUNCTION(Server, Reliable)
	void Server_ChestOpen();

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	UFUNCTION()
	void OnCharacterBeginOverlap(UPrimitiveComponent* OverlappedCom, AActor* OtherActor, UPrimitiveComponent* OtherCom, int32 OteryBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnCharacterEndOverlap(UPrimitiveComponent* OverlappedCom, AActor* OtherActor, UPrimitiveComponent* OtherCom, int32 OteryBodyIndex);

	UFUNCTION()
	void OnRep_CurrentState();
	void HandleChestEmpty();
	void HandleChestDestroy();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// 상호작용 범위
	UPROPERTY(VisibleAnywhere, Category = Chest)
	USphereComponent* Trigger;
	UPROPERTY(VisibleAnywhere, Category = Chest)
	UStaticMeshComponent* Chest;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentState)
	int CurrentState;
	UPROPERTY(EditAnywhere, Category = "PFChest")
	float Radius;

	UPROPERTY(EditAnywhere, Category = "PFChest")
	int Reward;

	// 빈 상자 전환 예약
	FTimerHandle EmptyStateTimerHandle;
	// 상자 제거 예약
	FTimerHandle DestroyTimerHandle;
};
