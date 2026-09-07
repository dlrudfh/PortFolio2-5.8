#include "Props/PFItem.h"

#include "Character/PFPlayer.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"

APFItem::APFItem() : NiagaraID(NIAGARA_END)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CollisionCom = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionCom"));
	RootComponent = CollisionCom;
	CollisionCom->SetSphereRadius(50.0f);
	CollisionCom->SetCollisionProfileName(TEXT("Item"));
	CollisionCom->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionCom->OnComponentBeginOverlap.AddDynamic(this, &APFItem::OnBeginOverlap);
	CollisionCom->SetGenerateOverlapEvents(true);

	NiagaraCom = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	NiagaraCom->SetupAttachment(RootComponent);
}

// 아이템 데이터 설정 요청
void APFItem::SetItemData(int ItemID)
{
	Server_SetItemData(ItemID);
}

// 아이템 번호 검사
bool APFItem::Server_SetItemData_Validate(int ItemID)
{
	return ItemID >= 0;
}

// 서버 아이템 데이터, 표시 설정
void APFItem::Server_SetItemData_Implementation(int ItemID)
{
	// 아이템 데이터, 이펙트 선택
	StoredItemID = ItemID;

	auto PFGameInstance = Cast<UPFGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	PFCHECK(PFGameInstance);
	ItemData = PFGameInstance->GetPFItemData(ItemID);

	switch (ItemID)
	{
	case etoi(ITEM_HPPOTION):
		NiagaraID = NIAGARA_ITEM_HPPOTION;
		break;
	case etoi(ITEM_MPPOTION):
		NiagaraID = NIAGARA_ITEM_MPPOTION;
		break;
	case etoi(ITEM_SHIELD):
		NiagaraID = NIAGARA_ITEM_SHIELD;
		break;
	case etoi(ITEM_COIN):
		NiagaraID = NIAGARA_ITEM_COIN;
		break;
	default:
		NiagaraID = NIAGARA_END;
		break;
	}

	OnRep_NiagaraID();

	// 설정 완료 후 겹친 플레이어 감지
	if (CollisionCom)
	{
		CollisionCom->SetGenerateOverlapEvents(true);
		CollisionCom->UpdateOverlaps();
	}
}

// 아이템 표시 이펙트 반영
void APFItem::OnRep_NiagaraID()
{
	NiagaraCom->SetAsset(GETNIAGARA(NiagaraID));
}

// 아이템 표시 이펙트 활성화
void APFItem::ActivateNiagaraEffect()
{
	if (NiagaraCom)
	{
		NiagaraCom->Activate();
	}
}

// 접촉한 플레이어의 아이템 획득 처리
void APFItem::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APFPlayer* Character = Cast<APFPlayer>(OtherActor))
	{
		UseItem(Character);
	}
}

// 인벤토리 추가 후 월드 아이템 제거
void APFItem::UseItem(APFPlayer* Character)
{
	if (!HasAuthority() || !Character || StoredItemID == RETURN_ERROR)
	{
		return;
	}

	APFPlayerState* PFPlayerState = Character->GetPlayerState<APFPlayerState>();
	if (!PFPlayerState)
	{
		return;
	}

	if (!PFPlayerState->AddInventoryItem(StoredItemID, 1))
	{
		return;
	}

	CollisionCom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Destroy();
}

// 서버 아이템 획득 처리
void APFItem::Server_UseItem_Implementation(APFPlayer* Character)
{
	UseItem(Character);
}

void APFItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APFItem, NiagaraID);
	DOREPLIFETIME(APFItem, StoredItemID);
}
