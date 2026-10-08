#pragma once

#include "PortFolio/PortFolio.h"

#include "Character/PFCharacter.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "NiagaraComponent.h"
#include "GameFramework/Actor.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"

#include "PFItem.generated.h"

// 아이템 가격, 효과, 표시 정보
struct FPFItemDefinition
{
	int32 Price;
	float Amount;
	const TCHAR* IconPath;
	FGameplayTag CooldownTag;
	ENIAGARAID NiagaraID;
};

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
	static const FPFItemDefinition* GetDefinition(int32 ItemID);
	void SetItemData(int ItemID, class APFPlayerController* InChestOpener = nullptr);
	void UseItem(APFCharacter* Character);

	UFUNCTION()
	void OnRep_NiagaraID();

	UFUNCTION(BlueprintCallable, Category = "Item")
	void ActivateNiagaraEffect();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	// 보상 상자를 연 플레이어
	TWeakObjectPtr<class APFPlayerController> ChestOpener;

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
