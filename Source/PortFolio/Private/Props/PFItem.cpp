#include "Props/PFItem.h"

#include "Character/PFCharacter.h"
#include "GAS/PFGameplayTags.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFTutorialManager.h"
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
	CollisionCom->SetGenerateOverlapEvents(false);

	NiagaraCom = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	NiagaraCom->SetupAttachment(RootComponent);
}

void APFItem::BeginPlay()
{
	Super::BeginPlay();
	OnRep_NiagaraID();
	CollisionCom->SetGenerateOverlapEvents(StoredItemID != RETURN_ERROR);
	CollisionCom->UpdateOverlaps();
}

// 아이템 공통 정의 조회
const FPFItemDefinition* APFItem::GetDefinition(int32 ItemID)
{
	static const FPFItemDefinition Definitions[] =
	{
		{100, 10.f, TEXT("/Game/GameData/Images/Items/Hp.Hp"), PFGameplayTags::Item_Cooldown_HP, NIAGARA_ITEM_HPPOTION},
		{50, 10.f, TEXT("/Game/GameData/Images/Items/Mp.Mp"), PFGameplayTags::Item_Cooldown_MP, NIAGARA_ITEM_MPPOTION},
		{50, 0.f, TEXT("/Game/GameData/Images/Items/Shield.Shield"), PFGameplayTags::Item_Cooldown_Shield, NIAGARA_ITEM_SHIELD},
		{0, 10.f, TEXT("/Game/GameData/Images/Items/Coin.Coin"), PFGameplayTags::Item_Cooldown_Coin, NIAGARA_ITEM_COIN}
	};
	static_assert(UE_ARRAY_COUNT(Definitions) == etoi(ITEM_END));
	return ItemID >= 0 && ItemID < UE_ARRAY_COUNT(Definitions) ? &Definitions[ItemID] : nullptr;
}

// 서버 아이템 데이터, 표시 설정
void APFItem::SetItemData(int ItemID, APFPlayerController* InChestOpener)
{
	if (!HasAuthority() || ItemID < 0) return;
	StoredItemID = ItemID;
	ChestOpener = InChestOpener;
	const FPFItemDefinition* Definition = GetDefinition(ItemID);
	NiagaraID = Definition ? Definition->NiagaraID : NIAGARA_END;

	OnRep_NiagaraID();

	// 설정 완료 후 겹친 플레이어 감지
	if (HasActorBegunPlay())
	{
		CollisionCom->SetGenerateOverlapEvents(true);
		CollisionCom->UpdateOverlaps();
	}
}

// 아이템 표시 이펙트 반영
void APFItem::OnRep_NiagaraID()
{
	NiagaraCom->SetAsset(GETNIAGARA(NiagaraID));
	if (NiagaraCom->GetAsset()) NiagaraCom->Activate(true);
}

// 아이템 표시 이펙트 활성화
void APFItem::ActivateNiagaraEffect()
{
	NiagaraCom->Activate();
}

// 접촉한 플레이어의 아이템 획득 처리
void APFItem::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APFCharacter* Character = Cast<APFCharacter>(OtherActor))
	{
		UseItem(Character);
	}
}

// 인벤토리 추가 후 월드 아이템 제거
void APFItem::UseItem(APFCharacter* Character)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !IsValid(Character) || !Character->IsPlayerCharacter()
		|| !Character->IsPlayerControlled() || StoredItemID == RETURN_ERROR)
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

	StoredItemID = RETURN_ERROR;
	CollisionCom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (APFPlayerController* Controller = Cast<APFPlayerController>(Character->GetController()); Controller && Controller == ChestOpener.Get())
	{
		if (APFTutorialManager* Tutorial = APFTutorialManager::Find(GetWorld()))
		{
			Controller->Client_NotifyTutorialChest(Tutorial, GetFName(), GetActorLocation(), true);
		}
	}
	Destroy();
}


void APFItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APFItem, NiagaraID);
	DOREPLIFETIME(APFItem, StoredItemID);
}
