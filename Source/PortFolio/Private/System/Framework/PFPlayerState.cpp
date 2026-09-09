#include "System/Framework/PFPlayerState.h"

#include "AbilitySystemComponent.h"
#include "System/Framework/PFGameInstance.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"
#include "Character/PFPlayer.h"
#include "Props/PFItem.h"

namespace PFPlayerStatePrivate
{
	// 아이템별 쿨타임 태그 조회
	FGameplayTag GetItemCooldownTag(int32 ItemID)
	{
		switch (ItemID)
		{
		case etoi(APFItem::EITEM::ITEM_HPPOTION):
			return PFGameplayTags::Item_Cooldown_HP;
		case etoi(APFItem::EITEM::ITEM_MPPOTION):
			return PFGameplayTags::Item_Cooldown_MP;
		case etoi(APFItem::EITEM::ITEM_SHIELD):
			return PFGameplayTags::Item_Cooldown_Shield;
		case etoi(APFItem::EITEM::ITEM_COIN):
			return PFGameplayTags::Item_Cooldown_Coin;
		default:
			return FGameplayTag();
		}
	}
}

APFPlayerState::APFPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AttributeSet = CreateDefaultSubobject<UPFAttributeSet>(TEXT("AttributeSet"));
	SetNetUpdateFrequency(100.f);

	InitializeInventorySlots();
	InitializeQuickSlots();
}

UAbilitySystemComponent* APFPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 플레이어 스탯, 마나 재생 초기화
void APFPlayerState::InitializeGASStats()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// 최초 한 번 기본 스탯 적용
	if (!bGASStatsInitialized)
	{
		UPFGameInstance* PFGameInstance = Cast<UPFGameInstance>(GetGameInstance());
		constexpr int32 InitialLevel = 1;
		FPFCharacterData* InitialData = PFGameInstance ? PFGameInstance->GetPFCharacterData(InitialLevel) : nullptr;
		if (!InitialData)
		{
			PFLOG(Warning, TEXT("GAS stat initialization failed: level %d data doesn't exist"), InitialLevel);
			return;
		}

		bGASStatsInitialized = FPFGE_StatGameplayEffects::InitializeStats(
			AbilitySystemComponent,
			static_cast<float>(InitialData->Level),
			static_cast<float>(InitialData->CurExp),
			InitialData->MaxHP,
			InitialData->MaxHP,
			InitialData->MaxMP,
			InitialData->MaxMP,
			InitialData->Damage,
			0.f,
			10.f);
	}

	// 마나 재생 효과 유지
	if (bGASStatsInitialized && !AbilitySystemComponent->GetActiveGameplayEffect(ManaRegenEffectHandle))
	{
		ManaRegenEffectHandle = FPFGE_StatGameplayEffects::ApplyManaRegen(AbilitySystemComponent);
	}
}

// 스탯 강화 요청
void APFPlayerState::RequestStatIncrease(EPFStatUpgradeType UpgradeType)
{
	if (HasAuthority())
	{
		ApplyStatIncrease(UpgradeType);
		return;
	}

	Server_IncreaseStat(static_cast<int32>(UpgradeType));
}

// 스탯 강화 종류 검사
bool APFPlayerState::Server_IncreaseStat_Validate(int32 UpgradeTypeIndex)
{
	return UpgradeTypeIndex >= static_cast<int32>(EPFStatUpgradeType::MaxHealth)
		&& UpgradeTypeIndex <= static_cast<int32>(EPFStatUpgradeType::AttackPower);
}

// 서버 스탯 강화 요청 처리
void APFPlayerState::Server_IncreaseStat_Implementation(int32 UpgradeTypeIndex)
{
	ApplyStatIncrease(static_cast<EPFStatUpgradeType>(UpgradeTypeIndex));
}

// 스탯 강화 적용
void APFPlayerState::ApplyStatIncrease(EPFStatUpgradeType UpgradeType)
{
	if (!HasAuthority() || !bGASStatsInitialized || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	if (FPFGE_StatGameplayEffects::ApplyStatUpgrade(AbilitySystemComponent, AttributeSet, UpgradeType))
	{
		ForceNetUpdate();
	}
}

void APFPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || bHasGrantedInitialItems || !GetWorld())
	{
		return;
	}

	const FString LevelName = FPackageName::GetShortName(GetWorld()->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		return;
	}

	GrantInitialInventoryItems();
}

// 인벤토리 슬롯 크기 초기화
void APFPlayerState::InitializeInventorySlots()
{
	if (InventorySlots.Num() != InventorySlotCount)
	{
		InventorySlots.SetNum(InventorySlotCount);
	}
}

// 퀵슬롯 초기화
void APFPlayerState::InitializeQuickSlots()
{
	if (QuickSlotItemIDs.Num() != QuickSlotCount)
	{
		QuickSlotItemIDs.Init(RETURN_ERROR, QuickSlotCount);
	}
}

// 시작 아이템 지급
void APFPlayerState::GrantInitialInventoryItems()
{
	bHasGrantedInitialItems = true;

	AddInventoryItem(etoi(APFItem::EITEM::ITEM_HPPOTION), 10);
	AddInventoryItem(etoi(APFItem::EITEM::ITEM_MPPOTION), MaxInventoryStackCount);
	AddInventoryItem(etoi(APFItem::EITEM::ITEM_SHIELD), 10);
	AddInventoryItem(etoi(APFItem::EITEM::ITEM_COIN), 10);
}

// 선택 캐릭터 저장
void APFPlayerState::SetCharacter(ECHARACTER SelectedCharacter)
{
	if (SelectedCharacter != CHARACTER_TWINBLAST && SelectedCharacter != CHARACTER_KWANG)
	{
		return;
	}

	if (HasAuthority())
	{
		CharacterType = SelectedCharacter;
	}
	else
	{
		Server_SetCharacter(SelectedCharacter);
	}
}

// 서버 선택 캐릭터 반영
void APFPlayerState::Server_SetCharacter_Implementation(ECHARACTER SelectedCharacter)
{
	SetCharacter(SelectedCharacter);
}

// 서버 인벤토리에 아이템 추가
bool APFPlayerState::AddInventoryItem(int32 ItemID, int32 Count)
{
	return HasAuthority() && AddInventoryItemInternal(ItemID, Count);
}

// 기존 묶음 또는 빈 슬롯에 아이템 추가
bool APFPlayerState::AddInventoryItemInternal(int32 ItemID, int32 Count)
{
	if (ItemID < 0 || Count <= 0)
	{
		return false;
	}

	InitializeInventorySlots();

	// 같은 아이템 묶음에 수량 추가
	for (FPFInventorySlot& Slot : InventorySlots)
	{
		if (!Slot.IsEmpty() && Slot.ItemID == ItemID)
		{
			if (Slot.Count >= MaxInventoryStackCount)
			{
				return false;
			}

			const int32 AddableCount = FMath::Min(Count, MaxInventoryStackCount - Slot.Count);
			if (AddableCount <= 0)
			{
				return false;
			}

			Slot.Count += AddableCount;
			SanitizeQuickSlots();
			OnRep_InventorySlots();
			return true;
		}
	}

	// 빈 슬롯에 새 아이템 등록
	for (FPFInventorySlot& Slot : InventorySlots)
	{
		if (Slot.IsEmpty())
		{
			Slot = FPFInventorySlot(ItemID, FMath::Min(Count, MaxInventoryStackCount));
			SanitizeQuickSlots();
			OnRep_InventorySlots();
			return true;
		}
	}

	return false;
}

// 인벤토리 아이템 사용 요청
void APFPlayerState::UseInventoryItem(int32 SlotIndex, APFPlayer* Character)
{
	if (HasAuthority())
	{
		Server_UseInventoryItem_Implementation(SlotIndex, Character);
		return;
	}

	Server_UseInventoryItem(SlotIndex, Character);
}

// 서버 아이템 사용, 수량 차감
void APFPlayerState::Server_UseInventoryItem_Implementation(int32 SlotIndex, APFPlayer* Character)
{
	APFPlayer* CurrentPlayer = Cast<APFPlayer>(GetPawn());
	if (!HasAuthority() || !IsValid(CurrentPlayer) || Character != CurrentPlayer
		|| CurrentPlayer->GetPlayerState<APFPlayerState>() != this || CurrentPlayer->IsDeadCharacter())
	{
		return;
	}

	InitializeInventorySlots();

	if (!InventorySlots.IsValidIndex(SlotIndex) || !Character)
	{
		return;
	}

	FPFInventorySlot& Slot = InventorySlots[SlotIndex];
	if (Slot.IsEmpty())
	{
		return;
	}

	const int32 UsedItemID = Slot.ItemID;

	if (GetItemCooldownRemaining(UsedItemID) > 0.f)
	{
		return;
	}

	if (!CanUseInventoryItem(UsedItemID))
	{
		return;
	}

	const FActiveGameplayEffectHandle CooldownHandle = StartItemCooldown(UsedItemID);
	if (!CooldownHandle.IsValid())
	{
		return;
	}

	// 아이템 종류별 효과 적용
	bool bItemEffectApplied = false;
	switch (UsedItemID)
	{
	case etoi(APFItem::EITEM::ITEM_HPPOTION):
		bItemEffectApplied = Character->GetHP(10.f);
		break;

	case etoi(APFItem::EITEM::ITEM_MPPOTION):
		bItemEffectApplied = Character->GetMP(10.f);
		break;

	case etoi(APFItem::EITEM::ITEM_SHIELD):
		bItemEffectApplied = Character->GetShield();
		break;

	case etoi(APFItem::EITEM::ITEM_COIN):
		bItemEffectApplied = Character->GetCoin(10.f);
		break;

	default:
		break;
	}

	if (!bItemEffectApplied)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(CooldownHandle);
		return;
	}

	// 수량 차감, 소진 슬롯 정리
	Slot.Count -= 1;
	if (Slot.Count <= 0)
	{
		Slot.Clear();
	}

	SanitizeQuickSlots();
	OnRep_InventorySlots();
}

// 퀵슬롯 등록 요청
void APFPlayerState::AssignQuickSlot(int32 QuickSlotIndex, int32 ItemID)
{
	if (HasAuthority())
	{
		Server_AssignQuickSlot_Implementation(QuickSlotIndex, ItemID);
		return;
	}

	Server_AssignQuickSlot(QuickSlotIndex, ItemID);
}

// 서버 퀵슬롯 등록, 해제
void APFPlayerState::Server_AssignQuickSlot_Implementation(int32 QuickSlotIndex, int32 ItemID)
{
	InitializeQuickSlots();

	if (!QuickSlotItemIDs.IsValidIndex(QuickSlotIndex))
	{
		return;
	}

	if (ItemID == RETURN_ERROR)
	{
		QuickSlotItemIDs[QuickSlotIndex] = RETURN_ERROR;
		OnRep_QuickSlotItemIDs();
		return;
	}

	if (FindInventorySlotIndexByItemID(ItemID) == INDEX_NONE)
	{
		return;
	}

	QuickSlotItemIDs[QuickSlotIndex] = ItemID;
	OnRep_QuickSlotItemIDs();
}

// 퀵슬롯 사용 요청
void APFPlayerState::UseQuickSlot(int32 QuickSlotIndex, APFPlayer* Character)
{
	if (HasAuthority())
	{
		Server_UseQuickSlot_Implementation(QuickSlotIndex, Character);
		return;
	}

	Server_UseQuickSlot(QuickSlotIndex, Character);
}

// 퀵슬롯의 인벤토리 아이템 사용
void APFPlayerState::Server_UseQuickSlot_Implementation(int32 QuickSlotIndex, APFPlayer* Character)
{
	APFPlayer* CurrentPlayer = Cast<APFPlayer>(GetPawn());
	if (!HasAuthority() || !IsValid(CurrentPlayer) || Character != CurrentPlayer
		|| CurrentPlayer->GetPlayerState<APFPlayerState>() != this || CurrentPlayer->IsDeadCharacter())
	{
		return;
	}

	InitializeQuickSlots();

	if (!QuickSlotItemIDs.IsValidIndex(QuickSlotIndex) || !Character)
	{
		return;
	}

	const int32 QuickSlotItemID = QuickSlotItemIDs[QuickSlotIndex];
	if (QuickSlotItemID == RETURN_ERROR)
	{
		return;
	}

	const int32 InventorySlotIndex = FindInventorySlotIndexByItemID(QuickSlotItemID);
	if (InventorySlotIndex == INDEX_NONE)
	{
		QuickSlotItemIDs[QuickSlotIndex] = RETURN_ERROR;
		OnRep_QuickSlotItemIDs();
		return;
	}

	Server_UseInventoryItem_Implementation(InventorySlotIndex, Character);
}

// 인벤토리 슬롯 이동 요청
void APFPlayerState::MoveInventorySlot(int32 FromIndex, int32 ToIndex)
{
	if (HasAuthority())
	{
		Server_MoveInventorySlot_Implementation(FromIndex, ToIndex);
		return;
	}

	Server_MoveInventorySlot(FromIndex, ToIndex);
}

// 서버 인벤토리 슬롯 교환
void APFPlayerState::Server_MoveInventorySlot_Implementation(int32 FromIndex, int32 ToIndex)
{
	InitializeInventorySlots();

	if (!InventorySlots.IsValidIndex(FromIndex) || !InventorySlots.IsValidIndex(ToIndex) || FromIndex == ToIndex)
	{
		return;
	}

	if (InventorySlots[FromIndex].IsEmpty())
	{
		return;
	}

	Swap(InventorySlots[FromIndex], InventorySlots[ToIndex]);
	OnRep_InventorySlots();
}

// 아이템이 든 슬롯 탐색
int32 APFPlayerState::FindInventorySlotIndexByItemID(int32 ItemID) const
{
	for (int32 SlotIndex = 0; SlotIndex < InventorySlots.Num(); ++SlotIndex)
	{
		if (!InventorySlots[SlotIndex].IsEmpty() && InventorySlots[SlotIndex].ItemID == ItemID)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

// 아이템 효과 적용 가능 여부 확인
bool APFPlayerState::CanUseInventoryItem(int32 ItemID) const
{
	if (!AttributeSet)
	{
		return false;
	}

	switch (ItemID)
	{
	case etoi(APFItem::EITEM::ITEM_HPPOTION):
		return AttributeSet->GetHealth() < AttributeSet->GetMaxHealth();

	case etoi(APFItem::EITEM::ITEM_MPPOTION):
		return AttributeSet->GetMana() < AttributeSet->GetMaxMana();

	case etoi(APFItem::EITEM::ITEM_SHIELD):
	case etoi(APFItem::EITEM::ITEM_COIN):
		return true;

	default:
		return false;
	}
}

// 아이템 총수량 조회
int32 APFPlayerState::GetInventoryItemCount(int32 ItemID) const
{
	int32 TotalCount = 0;
	for (const FPFInventorySlot& Slot : InventorySlots)
	{
		if (!Slot.IsEmpty() && Slot.ItemID == ItemID)
		{
			TotalCount += Slot.Count;
		}
	}

	return TotalCount;
}

// 아이템 효과의 남은 쿨타임 조회
float APFPlayerState::GetItemCooldownRemaining(int32 ItemID) const
{
	if (!AbilitySystemComponent)
	{
		return 0.f;
	}

	const FGameplayTag CooldownTag = PFPlayerStatePrivate::GetItemCooldownTag(ItemID);
	if (!CooldownTag.IsValid())
	{
		return 0.f;
	}

	// 쿨타임 태그의 활성 효과 조회
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(CooldownTag);
	const FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<float> RemainingTimes = AbilitySystemComponent->GetActiveEffectsTimeRemaining(CooldownQuery);

	float MaxRemainingTime = 0.f;
	for (const float RemainingTime : RemainingTimes)
	{
		MaxRemainingTime = FMath::Max(MaxRemainingTime, RemainingTime);
	}

	return MaxRemainingTime;
}

// 소진된 아이템의 퀵슬롯 연결 해제
void APFPlayerState::SanitizeQuickSlots()
{
	InitializeQuickSlots();

	bool bChanged = false;
	for (int32& QuickSlotItemID : QuickSlotItemIDs)
	{
		if (QuickSlotItemID != RETURN_ERROR && FindInventorySlotIndexByItemID(QuickSlotItemID) == INDEX_NONE)
		{
			QuickSlotItemID = RETURN_ERROR;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnRep_QuickSlotItemIDs();
	}
}

// 아이템 쿨타임 시작
FActiveGameplayEffectHandle APFPlayerState::StartItemCooldown(int32 ItemID)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayTag CooldownTag = PFPlayerStatePrivate::GetItemCooldownTag(ItemID);
	return FPFGE_StatGameplayEffects::ApplyItemCooldown(
		AbilitySystemComponent, CooldownTag, ItemCooldownDuration);
}

// 인벤토리 변경 알림
void APFPlayerState::OnRep_InventorySlots()
{
	OnInventoryChanged.Broadcast();
}

// 퀵슬롯 변경 알림
void APFPlayerState::OnRep_QuickSlotItemIDs()
{
	OnInventoryChanged.Broadcast();
}

void APFPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 선택 캐릭터, 소지품 복제 등록
	DOREPLIFETIME(APFPlayerState, CharacterType);
	DOREPLIFETIME(APFPlayerState, InventorySlots);
	DOREPLIFETIME(APFPlayerState, QuickSlotItemIDs);
}
