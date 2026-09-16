#pragma once

#include "PortFolio/PortFolio.h"

#include "Character/PFCharacter.h"
#include "System/Framework/PFGameInstance.h"
#include "NiagaraComponent.h"
#include "GameFramework/Actor.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"

#include "PFItem.generated.h"

// 획득용 아이템 클래스
UCLASS(meta=(PrioritizeCategories="Component"))
class PORTFOLIO_API APFItem : public AActor
{
	GENERATED_BODY()
	
public:
	enum class EITEM
	{
		ITEM_HPPOTION = 0,
		ITEM_MPPOTION,
		ITEM_SHIELD,
		ITEM_COIN,
		ITEM_END
	};
	using enum APFItem::EITEM;

public:	
	APFItem();
	void SetItemData(int ItemID);
	void UseItem(APFCharacter* Character);
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetItemData(int ItemID);

	UFUNCTION()
	void OnRep_NiagaraID();

	UFUNCTION(BlueprintCallable, Category = "Item")
	void ActivateNiagaraEffect();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION(Server, Reliable)
	void Server_UseItem(APFCharacter* Character);

private:
	// 아이템 데이터 참조
	FPFItemData* ItemData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component", meta = (AllowPrivateAccess = "true"))
	USphereComponent* CollisionCom;

	// 아이템 표시 이펙트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component", meta = (AllowPrivateAccess = "true"))
	UNiagaraComponent* NiagaraCom;

	UPROPERTY(ReplicatedUsing = OnRep_NiagaraID)
	ENIAGARAID NiagaraID;


	UPROPERTY(Replicated)
	int32 StoredItemID = RETURN_ERROR;
};
