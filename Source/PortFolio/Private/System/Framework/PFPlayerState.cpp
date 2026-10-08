#include "System/Framework/PFPlayerState.h"
#include "Campaign/PFCampaignSubsystem.h"
#include "Campaign/PFCampaignDirector.h"
#include "GameFramework/Controller.h"

#include "AbilitySystemComponent.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFSessionGameState.h"
#include "System/Framework/PFTutorialManager.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"
#include "Character/PFCharacter.h"
#include "Props/PFItem.h"

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

void APFPlayerState::SetPlayerName(const FString& Name)
{
	const FString PreviousName = GetPlayerName();
	Super::SetPlayerName(Name);
	if (GetPlayerName() != PreviousName) OnPlayerInfoChanged.Broadcast();
}

void APFPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	OnPlayerInfoChanged.Broadcast();
}

// 선택 캐릭터 변경 알림
void APFPlayerState::OnRep_CharacterType()
{
	OnPlayerInfoChanged.Broadcast();
}

// 로비 선택, 준비 상태 변경 알림
void APFPlayerState::OnRep_LobbyState()
{
	OnPlayerInfoChanged.Broadcast();
}

// 서버의 로비 캐릭터 선택 반영
void APFPlayerState::SetLobbySelection(ECHARACTER SelectedCharacter, bool bSelected)
{
	if (!HasAuthority() || (SelectedCharacter != CHARACTER_TWINBLAST && SelectedCharacter != CHARACTER_KWANG)) return;
	if (CharacterType == SelectedCharacter && bLobbyCharacterSelected == bSelected) return;
	CharacterType = SelectedCharacter;
	bLobbyCharacterSelected = bSelected;
	bLobbyReady = false;
	OnPlayerInfoChanged.Broadcast();
	ForceNetUpdate();
}

// 서버의 로비 준비 상태 반영
void APFPlayerState::SetLobbyReady(bool bReady)
{
	if (!HasAuthority()) return;
	const bool bNewReady = bReady && bLobbyCharacterSelected;
	if (bLobbyReady == bNewReady) return;
	bLobbyReady = bNewReady;
	OnRep_LobbyState();
	ForceNetUpdate();
}

// 로비 참가 순서 반영
void APFPlayerState::SetLobbySlot(int32 Slot)
{
	if (!HasAuthority() || LobbySlot == Slot) return;
	LobbySlot = Slot;
	OnRep_LobbyState();
	ForceNetUpdate();
}

// 체크포인트 소지품 복원
void APFPlayerState::RestoreCampaignInventory(const TArray<FPFInventorySlot>& Slots, const TArray<int32>& QuickSlots)
{
	if (!HasAuthority()) return;
	InventorySlots = Slots;
	QuickSlotItemIDs = QuickSlots;
	InitializeInventorySlots();
	InitializeQuickSlots();
	bHasGrantedInitialItems = true;
	SanitizeQuickSlots();
	OnRep_InventorySlots();
	ForceNetUpdate();
}

// 체크포인트 전투 상태 정리, 체력과 마나 회복
void APFPlayerState::PrepareCampaignCheckpoint()
{
	if (!HasAuthority()) return;
	if (AbilitySystemComponent->GetOwnerActor() != this) AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());
	InitializeGASStats();
	AbilitySystemComponent->CancelAllAbilities();
	for (FActiveGameplayEffectHandle Handle : AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Effect = AbilitySystemComponent->GetActiveGameplayEffect(Handle);
		if (Effect && Effect->Spec.Def && (Effect->Spec.Def->IsA<UPFGE_ItemCooldown>() || Effect->Spec.Def->IsA<UPFGE_Shield>()))
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
	}
	for (FGameplayTag Tag : { FGameplayTag(PFGameplayTags::Character_State_Dead), FGameplayTag(PFGameplayTags::Character_State_Sprinting),
		FGameplayTag(PFGameplayTags::Character_State_Ultimate) })
		AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagOnly);
	FPFGE_StatGameplayEffects::ApplyHeal(AbilitySystemComponent, AttributeSet->GetMaxHealth() - AttributeSet->GetHealth());
	FPFGE_StatGameplayEffects::ApplyManaRestore(AbilitySystemComponent, AttributeSet->GetMaxMana() - AttributeSet->GetMana());
}

// 공간이 확보된 경우에만 보급 묶음 지급
bool APFPlayerState::GrantCampaignSupply()
{
	if (!HasAuthority()) return false;
	const TArray<FPFInventorySlot> Before = InventorySlots;
	if (!AddInventoryItemInternal(etoi(APFItem::EITEM::ITEM_HPPOTION), 1)
		|| !AddInventoryItemInternal(etoi(APFItem::EITEM::ITEM_MPPOTION), 1))
	{
		InventorySlots = Before;
		return false;
	}
	OnRep_InventorySlots();
	ForceNetUpdate();
	return true;
}

void APFPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	APFPlayerState* NewState = Cast<APFPlayerState>(NewPlayerState);
	if (!HasAuthority() || !NewState || NewState == this)
	{
		return;
	}

	// 선택 캐릭터, 소지품, 최초 지급 상태 유지
	NewState->CharacterType = CharacterType;
	NewState->bLobbyCharacterSelected = bLobbyCharacterSelected;
	NewState->LobbySlot = LobbySlot;
	NewState->bLobbyReady = false;
	NewState->InventorySlots = InventorySlots;
	NewState->QuickSlotItemIDs = QuickSlotItemIDs;
	NewState->bHasGrantedInitialItems = bHasGrantedInitialItems;

	// 새 ASC에 성장, 전투 수치 복원
	UAbilitySystemComponent* NewASC = NewState->AbilitySystemComponent;
	if (bGASStatsInitialized)
	{
		NewASC->InitAbilityActorInfo(NewState, nullptr);
		NewState->bGASStatsInitialized = FPFGE_StatGameplayEffects::InitializeStats(NewASC, FPFStatValues(*AttributeSet));
		NewASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Dead,
			AttributeSet->GetHealth() <= 0.f ? 1 : 0, EGameplayTagReplicationState::TagOnly);
	}

	// 아이템 쿨타임, 실드의 남은 시간 유지
	if (GetWorld())
	{
		const float WorldTime = GetWorld()->GetTimeSeconds();
		for (const FActiveGameplayEffectHandle Handle : AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Effect = AbilitySystemComponent->GetActiveGameplayEffect(Handle);
			if (!Effect || !Effect->Spec.Def
				|| (!Effect->Spec.Def->IsA<UPFGE_ItemCooldown>() && !Effect->Spec.Def->IsA<UPFGE_Shield>()))
			{
				continue;
			}
			const float RemainingTime = Effect->GetTimeRemaining(WorldTime);
			if (RemainingTime <= 0.f)
			{
				continue;
			}

			FGameplayEffectSpec RestoredSpec(Effect->Spec.Def, NewASC->MakeEffectContext(), Effect->Spec.GetLevel());
			RestoredSpec.DynamicGrantedTags = Effect->Spec.DynamicGrantedTags;
			RestoredSpec.CopySetByCallerMagnitudes(Effect->Spec);
			RestoredSpec.SetStackCount(Effect->Spec.GetStackCount());
			RestoredSpec.SetDuration(RemainingTime, true);
			NewASC->ApplyGameplayEffectSpecToSelf(RestoredSpec);
		}
	}

	NewState->OnRep_InventorySlots();
	NewState->ForceNetUpdate();
}

// 플레이어 스탯, 마나 재생 초기화
void APFPlayerState::InitializeGASStats()
{
	if (!HasAuthority())
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

		bGASStatsInitialized = FPFGE_StatGameplayEffects::InitializeStats(AbilitySystemComponent, FPFStatValues(*InitialData));
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
	if (!CanUseUIInput()) return;
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
	if (!CanUseUIInput()) return;
	if (!bGASStatsInitialized)
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
	EnsureInitialInventoryItems();
}

// 게임 진입 후 시작 아이템 한 번 지급
void APFPlayerState::EnsureInitialInventoryItems()
{
	if (!HasAuthority() || bHasGrantedInitialItems)
	{
		return;
	}

	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!Session || !Session->bInitialized || Session->Phase != EPFSessionPhase::Playing)
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
	const bool bCampaign = UPFCampaignSubsystem::IsCampaign(GetWorld());
	AddInventoryItemInternal(etoi(APFItem::EITEM::ITEM_HPPOTION), bCampaign ? 3 : 10);
	AddInventoryItemInternal(etoi(APFItem::EITEM::ITEM_MPPOTION), bCampaign ? 3 : MaxInventoryStackCount);
	AddInventoryItemInternal(etoi(APFItem::EITEM::ITEM_SHIELD), bCampaign ? 1 : 10);
	if (!bCampaign) AddInventoryItemInternal(etoi(APFItem::EITEM::ITEM_COIN), 10);
	OnRep_InventorySlots();
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
		if (CharacterType != SelectedCharacter)
		{
			CharacterType = SelectedCharacter;
			OnRep_CharacterType();
		}
	}
	else
	{
		Server_SetCharacter(SelectedCharacter);
	}
}

// 서버 선택 캐릭터 반영
void APFPlayerState::Server_SetCharacter_Implementation(ECHARACTER SelectedCharacter)
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (Session && Session->bInitialized && Session->Phase != EPFSessionPhase::Menu) return;
	if (GetPawn() && APFCampaignDirector::BlocksInput(Cast<AController>(GetOwner()))) return;
	SetCharacter(SelectedCharacter);
}

// 서버 인벤토리에 아이템 추가
bool APFPlayerState::AddInventoryItem(int32 ItemID, int32 Count)
{
	if (!HasAuthority() || !AddInventoryItemInternal(ItemID, Count)) return false;
	OnRep_InventorySlots();
	return true;
}

// 판매 아이템 가격
int32 APFPlayerState::GetShopItemPrice(int32 ItemID)
{
	const FPFItemDefinition* Definition = APFItem::GetDefinition(ItemID);
	return Definition ? Definition->Price : 0;
}

// 아이템을 담을 슬롯 조회
int32 APFPlayerState::FindAvailableInventorySlot(int32 ItemID) const
{
	const int32 Existing = FindInventorySlotIndexByItemID(ItemID);
	if (Existing != INDEX_NONE)
	{
		return InventorySlots[Existing].Count < MaxInventoryStackCount ? Existing : INDEX_NONE;
	}
	return InventorySlots.IndexOfByPredicate([](const FPFInventorySlot& Slot) { return Slot.IsEmpty(); });
}

// 구매 상태, 잔액, 인벤토리 공간 확인
EPFShopPurchaseResult APFPlayerState::GetShopPurchaseResult(int32 ItemID) const
{
	const int32 Price = GetShopItemPrice(ItemID);
	if (Price <= 0) return EPFShopPurchaseResult::InvalidItem;
	const APFCharacter* Character = Cast<APFCharacter>(GetPawn());
	if (!Character || !Character->IsPlayerCharacter()
		|| !Character->IsPlayerControlled() || Character->GetPlayerState<APFPlayerState>() != this
		|| !CanUseUIInput())
	{
		return EPFShopPurchaseResult::Unavailable;
	}
	if (AttributeSet->GetCoin() < Price)
	{
		return EPFShopPurchaseResult::InsufficientFunds;
	}
	return FindAvailableInventorySlot(ItemID) != INDEX_NONE ? EPFShopPurchaseResult::Success : EPFShopPurchaseResult::InventoryFull;
}

// 서버에 아이템 한 개 구매 요청
void APFPlayerState::PurchaseShopItem(int32 ItemID)
{
	if (!CanUseUIInput()) return;
	if (HasAuthority()) Server_PurchaseShopItem_Implementation(ItemID);
	else Server_PurchaseShopItem(ItemID);
}

// 서버 가격 검증, 코인 차감, 아이템 지급
void APFPlayerState::Server_PurchaseShopItem_Implementation(int32 ItemID)
{
	EPFShopPurchaseResult Result = GetShopPurchaseResult(ItemID);
	if (Result == EPFShopPurchaseResult::Success)
	{
		const int32 SlotIndex = FindAvailableInventorySlot(ItemID);
		if (!bGASStatsInitialized || !FPFGE_StatGameplayEffects::TryApplyCoinCost(AbilitySystemComponent, GetShopItemPrice(ItemID)))
		{
			Result = EPFShopPurchaseResult::Failed;
		}
		else
		{
			AddInventoryItemToSlot(SlotIndex, ItemID, 1);
			OnRep_InventorySlots();
			ForceNetUpdate();
		}
	}
	Client_ShopPurchaseResult(ItemID, Result);
}

// 로컬 상점에 구매 결과 전달
void APFPlayerState::Client_ShopPurchaseResult_Implementation(int32 ItemID, EPFShopPurchaseResult Result)
{
	OnShopPurchaseResult.Broadcast(ItemID, Result);
}

// 기존 묶음 또는 빈 슬롯에 아이템 추가
bool APFPlayerState::AddInventoryItemInternal(int32 ItemID, int32 Count)
{
	if (ItemID < 0 || Count <= 0)
	{
		return false;
	}

	const int32 SlotIndex = FindAvailableInventorySlot(ItemID);
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	AddInventoryItemToSlot(SlotIndex, ItemID, Count);
	return true;
}

// 검증된 슬롯에 수량 추가
void APFPlayerState::AddInventoryItemToSlot(int32 SlotIndex, int32 ItemID, int32 Count)
{
	FPFInventorySlot& Slot = InventorySlots[SlotIndex];
	const int32 CurrentCount = Slot.IsEmpty() ? 0 : Slot.Count;
	Slot = FPFInventorySlot(ItemID, CurrentCount + FMath::Min(Count, MaxInventoryStackCount - CurrentCount));
}

// 인벤토리 아이템 사용 요청
void APFPlayerState::UseInventoryItem(int32 SlotIndex, APFCharacter* Character)
{
	if (!CanUseUIInput()) return;
	if (HasAuthority())
	{
		Server_UseInventoryItem_Implementation(SlotIndex, Character);
		return;
	}

	Server_UseInventoryItem(SlotIndex, Character);
}

// 소유 플레이어의 아이템 사용 상태 확인
bool APFPlayerState::IsInventoryUser(const APFCharacter* Character) const
{
	return IsValid(Character) && Character == GetPawn()
		&& Character->IsPlayerCharacter() && Character->IsPlayerControlled()
		&& Character->GetPlayerState<APFPlayerState>() == this && CanUseUIInput();
}

// 소유 컨트롤러의 공통 UI 조작 상태 확인
bool APFPlayerState::CanUseUIInput() const
{
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwner());
	return Player && Player->CanUseUIInput();
}

// 서버 아이템 사용, 수량 차감
void APFPlayerState::Server_UseInventoryItem_Implementation(int32 SlotIndex, APFCharacter* Character)
{
	if (!IsInventoryUser(Character) || !InventorySlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	UseInventoryItemInternal(SlotIndex, Character);
}

// 검증된 슬롯의 아이템 사용
void APFPlayerState::UseInventoryItemInternal(int32 SlotIndex, APFCharacter* Character)
{
	FPFInventorySlot& Slot = InventorySlots[SlotIndex];
	if (Slot.IsEmpty())
	{
		return;
	}

	const int32 UsedItemID = Slot.ItemID;
	const FPFItemDefinition* Definition = APFItem::GetDefinition(UsedItemID);

	if (GetItemCooldownRemaining(UsedItemID) > 0.f)
	{
		return;
	}

	if (!Definition || !CanUseInventoryItem(UsedItemID))
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
	FGameplayTag ItemUseCueTag;
	switch (UsedItemID)
	{
	case etoi(APFItem::EITEM::ITEM_HPPOTION):
		bItemEffectApplied = Character->RestoreHealth(Definition->Amount);
		ItemUseCueTag = PFGameplayTags::GameplayCue_Item_Use_HP;
		break;

	case etoi(APFItem::EITEM::ITEM_MPPOTION):
		bItemEffectApplied = Character->RestoreMana(Definition->Amount);
		ItemUseCueTag = PFGameplayTags::GameplayCue_Item_Use_MP;
		break;

	case etoi(APFItem::EITEM::ITEM_SHIELD):
		bItemEffectApplied = Character->GrantShield();
		break;

	case etoi(APFItem::EITEM::ITEM_COIN):
		bItemEffectApplied = Character->AddCoin(Definition->Amount);
		ItemUseCueTag = PFGameplayTags::GameplayCue_Item_Use_Coin;
		break;

	default:
		break;
	}

	if (!bItemEffectApplied)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(CooldownHandle);
		return;
	}

	// 사용에 성공한 아이템의 일회성 연출 재생
	if (ItemUseCueTag.IsValid())
	{
		AbilitySystemComponent->ExecuteGameplayCue(ItemUseCueTag, AbilitySystemComponent->MakeEffectContext());
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
	if (!CanUseUIInput()) return;
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
	if (!CanUseUIInput()) return;
	if (APFTutorialManager::Find(GetWorld()) && ItemID != RETURN_ERROR
		&& ItemID != etoi(APFItem::ITEM_SHIELD) && ItemID != etoi(APFItem::ITEM_COIN)) return;

	if (!QuickSlotItemIDs.IsValidIndex(QuickSlotIndex) || QuickSlotItemIDs[QuickSlotIndex] == ItemID)
	{
		return;
	}

	if (ItemID != RETURN_ERROR && FindInventorySlotIndexByItemID(ItemID) == INDEX_NONE)
	{
		return;
	}

	QuickSlotItemIDs[QuickSlotIndex] = ItemID;
	OnRep_QuickSlotItemIDs();
}

// 퀵슬롯 사용 요청
void APFPlayerState::UseQuickSlot(int32 QuickSlotIndex, APFCharacter* Character)
{
	if (!CanUseUIInput()) return;
	if (HasAuthority())
	{
		Server_UseQuickSlot_Implementation(QuickSlotIndex, Character);
		return;
	}

	Server_UseQuickSlot(QuickSlotIndex, Character);
}

// 퀵슬롯의 인벤토리 아이템 사용
void APFPlayerState::Server_UseQuickSlot_Implementation(int32 QuickSlotIndex, APFCharacter* Character)
{
	if (!IsInventoryUser(Character) || !QuickSlotItemIDs.IsValidIndex(QuickSlotIndex))
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

	UseInventoryItemInternal(InventorySlotIndex, Character);
}

// 인벤토리 슬롯 이동 요청
void APFPlayerState::MoveInventorySlot(int32 FromIndex, int32 ToIndex)
{
	if (!CanUseUIInput()) return;
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
	if (!CanUseUIInput()) return;
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
	const FPFItemDefinition* Definition = APFItem::GetDefinition(ItemID);
	if (!Definition || !Definition->CooldownTag.IsValid())
	{
		return 0.f;
	}

	// 쿨타임 태그의 활성 효과 조회
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(Definition->CooldownTag);
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
	for (int32& QuickSlotItemID : QuickSlotItemIDs)
	{
		if (QuickSlotItemID != RETURN_ERROR && FindInventorySlotIndexByItemID(QuickSlotItemID) == INDEX_NONE)
		{
			QuickSlotItemID = RETURN_ERROR;
		}
	}
}

// 아이템 쿨타임 시작
FActiveGameplayEffectHandle APFPlayerState::StartItemCooldown(int32 ItemID)
{
	const FPFItemDefinition* Definition = APFItem::GetDefinition(ItemID);
	return Definition ? FPFGE_StatGameplayEffects::ApplyItemCooldown(
		AbilitySystemComponent, Definition->CooldownTag, ItemCooldownDuration) : FActiveGameplayEffectHandle();
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
	DOREPLIFETIME(APFPlayerState, bLobbyCharacterSelected);
	DOREPLIFETIME(APFPlayerState, bLobbyReady);
	DOREPLIFETIME(APFPlayerState, LobbySlot);
	DOREPLIFETIME(APFPlayerState, InventorySlots);
	DOREPLIFETIME(APFPlayerState, QuickSlotItemIDs);
}
