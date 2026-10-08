#pragma once

#include "PortFolio/PortFolio.h"

#include "Net/UnrealNetwork.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "GameFramework/PlayerState.h"

#include "PFPlayerState.generated.h"

UENUM()
enum class EPFShopPurchaseResult : uint8
{
	Success,
	Unavailable,
	InvalidItem,
	InsufficientFunds,
	InventoryFull,
	Failed
};

// 인벤토리 슬롯 정보
USTRUCT(BlueprintType)
struct FPFInventorySlot
{
	GENERATED_BODY()

public:
	FPFInventorySlot() : ItemID(RETURN_ERROR), Count(0) {}
	FPFInventorySlot(int32 InItemID, int32 InCount) : ItemID(InItemID), Count(InCount) {}

	bool IsEmpty() const { return ItemID == RETURN_ERROR || Count <= 0; }

	void Clear()
	{
		ItemID = RETURN_ERROR;
		Count = 0;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Count;
};

DECLARE_MULTICAST_DELEGATE(FOnInventoryChangedDelegate);
DECLARE_MULTICAST_DELEGATE(FOnPlayerInfoChangedDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShopPurchaseResultDelegate, int32, EPFShopPurchaseResult);

// 플레이어 스탯, 소지품 관리 클래스
UCLASS()
class PORTFOLIO_API APFPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	static constexpr int32 InventorySlotCount = 25;

	static constexpr int32 MaxInventoryStackCount = 9999;

	static constexpr float ItemCooldownDuration = 10.f;

	static constexpr int32 QuickSlotCount = 4;

	APFPlayerState();
	virtual void BeginPlay() override;
	virtual void SetPlayerName(const FString& Name) override;
	virtual void OnRep_PlayerName() override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	void RestoreCampaignInventory(const TArray<FPFInventorySlot>& Slots, const TArray<int32>& QuickSlots);
	void PrepareCampaignCheckpoint();
	bool GrantCampaignSupply();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UPFAttributeSet* GetAttributeSet() const { return AttributeSet; }
	void InitializeGASStats();
	void RequestStatIncrease(EPFStatUpgradeType UpgradeType);
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_IncreaseStat(int32 UpgradeTypeIndex);

	ECHARACTER GetCharacter() const { return CharacterType; }
	bool HasLobbySelection() const { return bLobbyCharacterSelected; }
	bool IsLobbyReady() const { return bLobbyReady; }
	int32 GetLobbySlot() const { return LobbySlot; }
	void SetLobbySelection(ECHARACTER SelectedCharacter, bool bSelected);
	void SetLobbyReady(bool bReady);
	void SetLobbySlot(int32 Slot);
	void EnsureInitialInventoryItems();

	void SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_SetCharacter(ECHARACTER SelectedCharacter);

	bool AddInventoryItem(int32 ItemID, int32 Count = 1);
	static int32 GetShopItemPrice(int32 ItemID);
	EPFShopPurchaseResult GetShopPurchaseResult(int32 ItemID) const;
	void PurchaseShopItem(int32 ItemID);
	UFUNCTION(Server, Reliable)
	void Server_PurchaseShopItem(int32 ItemID);
	UFUNCTION(Client, Reliable)
	void Client_ShopPurchaseResult(int32 ItemID, EPFShopPurchaseResult Result);

	void UseInventoryItem(int32 SlotIndex, class APFCharacter* Character);
	UFUNCTION(Server, Reliable)
	void Server_UseInventoryItem(int32 SlotIndex, class APFCharacter* Character);

	void AssignQuickSlot(int32 QuickSlotIndex, int32 ItemID);
	UFUNCTION(Server, Reliable)
	void Server_AssignQuickSlot(int32 QuickSlotIndex, int32 ItemID);

	void UseQuickSlot(int32 QuickSlotIndex, class APFCharacter* Character);
	UFUNCTION(Server, Reliable)
	void Server_UseQuickSlot(int32 QuickSlotIndex, class APFCharacter* Character);

	void MoveInventorySlot(int32 FromIndex, int32 ToIndex);
	UFUNCTION(Server, Reliable)
	void Server_MoveInventorySlot(int32 FromIndex, int32 ToIndex);

	UFUNCTION()
	void OnRep_InventorySlots();

	UFUNCTION()
	void OnRep_QuickSlotItemIDs();

	const TArray<FPFInventorySlot>& GetInventorySlots() const { return InventorySlots; }

	const TArray<int32>& GetQuickSlotItemIDs() const { return QuickSlotItemIDs; }

	int32 GetInventoryItemCount(int32 ItemID) const;

	float GetItemCooldownRemaining(int32 ItemID) const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void InitializeInventorySlots();
	UFUNCTION()
	void OnRep_CharacterType();
	UFUNCTION()
	void OnRep_LobbyState();

	void InitializeQuickSlots();

	void GrantInitialInventoryItems();

	bool AddInventoryItemInternal(int32 ItemID, int32 Count);
	void AddInventoryItemToSlot(int32 SlotIndex, int32 ItemID, int32 Count);
	void UseInventoryItemInternal(int32 SlotIndex, class APFCharacter* Character);

	int32 FindInventorySlotIndexByItemID(int32 ItemID) const;
	int32 FindAvailableInventorySlot(int32 ItemID) const;

	bool IsInventoryUser(const class APFCharacter* Character) const;
	bool CanUseUIInput() const;
	bool CanUseInventoryItem(int32 ItemID) const;

	void SanitizeQuickSlots();

	FActiveGameplayEffectHandle StartItemCooldown(int32 ItemID);

private:
	UPROPERTY(ReplicatedUsing = OnRep_CharacterType)
	ECHARACTER CharacterType = CHARACTER_TWINBLAST;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	bool bLobbyCharacterSelected = false;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	bool bLobbyReady = false;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	int32 LobbySlot = INDEX_NONE;

	// 인벤토리 슬롯 목록
	UPROPERTY(ReplicatedUsing = OnRep_InventorySlots)
	TArray<FPFInventorySlot> InventorySlots;

	// 퀵슬롯 아이템 연결
	UPROPERTY(ReplicatedUsing = OnRep_QuickSlotItemIDs)
	TArray<int32> QuickSlotItemIDs;

public:
	// 이름, 선택 캐릭터 변경 이벤트
	FOnPlayerInfoChangedDelegate OnPlayerInfoChanged;

	// 소지품 변경 이벤트
	FOnInventoryChangedDelegate OnInventoryChanged;
	// 소유 플레이어의 구매 결과
	FOnShopPurchaseResultDelegate OnShopPurchaseResult;

private:
	// 플레이어가 유지하는 ASC
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	class UAbilitySystemComponent* AbilitySystemComponent;

	// GAS 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	UPFAttributeSet* AttributeSet;

	// 마나 재생 효과 핸들
	FActiveGameplayEffectHandle ManaRegenEffectHandle;

	bool bGASStatsInitialized = false;

	void ApplyStatIncrease(EPFStatUpgradeType UpgradeType);

	bool bHasGrantedInitialItems = false;
};
