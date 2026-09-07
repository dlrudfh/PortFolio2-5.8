
#pragma once

#include "PortFolio/PortFolio.h"

#include "Blueprint/UserWidget.h"
#include "System/Framework/PFPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "PFPlayerController.generated.h"

// 플레이어 입력, 창 관리 클래스
UCLASS()
class PORTFOLIO_API APFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
	virtual void OnPossess(APawn* aPawn) override;

	ECHARACTER GetCharacter() { return GetPlayerState<APFPlayerState>()->GetCharacter(); }
	void SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_ReloadCharacter();
	void ToggleInventory();
	void ToggleStats();

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	void CreateUI();
	void OpenMenu();
	void BindInventoryWidget();
	void UpdateWindowInputMode();

	void UseQuickSlot1();

	void UseQuickSlot2();

	void UseQuickSlot3();

	void UseQuickSlot4();

	void UseQuickSlotByIndex(int32 QuickSlotIndex);

private:

	// 인벤토리 창
	UPROPERTY()
	class UPFInventoryWidget* InventoryWidget;

	// 스탯 창
	UPROPERTY()
	class UPFStatWidget* StatWidget = nullptr;
};
