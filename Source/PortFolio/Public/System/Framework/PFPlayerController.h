
#pragma once

#include "PortFolio/PortFolio.h"

#include "Blueprint/UserWidget.h"
#include "System/Framework/PFPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Character/PFCharacterControlTypes.h"
#include "Character/PFCombatAimProvider.h"
#include "PFPlayerController.generated.h"

// 플레이어 입력, 창 관리 클래스
UCLASS()
class PORTFOLIO_API APFPlayerController : public APlayerController, public IPFCombatAimProvider
{
	GENERATED_BODY()

public:
	APFPlayerController();
	virtual void PostInitializeComponents() override;
	virtual void OnPossess(APawn* aPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_Pawn() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual bool TryGetCombatAim(FVector& OutAimPoint) override;
	void SetChest(class APFChest* Chest = nullptr);
	void CaptureCharacterState();
	void RestoreCharacterState();

	ECHARACTER GetCharacter() { return GetPlayerState<APFPlayerState>()->GetCharacter(); }
	void SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_RequestInitialSpawn(ECHARACTER SelectedCharacter);
	UFUNCTION(Client, Reliable)
	void Client_StartRespawnCountdown(double RespawnEndServerTime, float RespawnDuration);
	UFUNCTION(Client, Reliable)
	void Client_StopRespawnCountdown();
	void ToggleInventory();
	void ToggleStats();
	void OpenMenu();
	FString GetUsername() const;

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	class APFCharacter* GetControlledCharacter() const;
	bool IsCurrentCharacter(const class APFCharacter* ControlledPawn) const;
	void BindControlledCharacter();
	void UnbindControlledCharacter();
	void HandleCharacterReady(class APFCharacter* ControlledPawn);
	void HandleCharacterDied(class APFCharacter* ControlledPawn);
	void HandleCharacterLanded(class APFCharacter* ControlledPawn);
	void BindCharacterHUD();
	void RefreshPawnOverlaps();
	void UpdateCharacterControl();
	void JumpStart();
	void JumpEnd();
	void UpDown(float Value);
	void LeftRight(float Value);
	void LookUp(float Value);
	void Turn(float Value);
	void AttackStart();
	void AttackEnd();
	void Ultimate();
	void Sprint();
	void ViewpointFix();
	void ViewChange();
	void Interaction();
	void ChangeCharacter();
	void SpawnTestTwinblastEnemy();
	void SpawnTestKwangEnemy();
	FVector CalculateAimPoint(bool* bOutCharacterTargeted = nullptr) const;

	UFUNCTION(Server, Unreliable)
	void Server_UpdateAimPoint(class APFCharacter* ControlledPawn, FVector NewAimPoint);
	UFUNCTION(Server, Reliable)
	void Server_SetDir(class APFCharacter* ControlledPawn, EPFDirection NewDirection);
	UFUNCTION(Server, Reliable)
	void Server_SetControlMode(class APFCharacter* ControlledPawn, ECONTROLMODE NewControlMode);
	UFUNCTION(Server, Reliable)
	void Server_Sprint(class APFCharacter* ControlledPawn);
	UFUNCTION(Server, Reliable)
	void Server_ViewpointFix(class APFCharacter* ControlledPawn);
	UFUNCTION(Server, Reliable)
	void Server_Interaction(class APFCharacter* ControlledPawn);
	UFUNCTION(Server, Reliable)
	void Server_ChangeCharacter(class APFCharacter* ControlledPawn);
	UFUNCTION(Server, Reliable)
	void Server_SpawnTestEnemy(class APFCharacter* ControlledPawn, bool bSpawnTwinblast);
	UFUNCTION(Client, Reliable)
	void Client_RestoreCharacterState(class APFCharacter* ControlledPawn, FPFCharacterSharedStateSnapshot Snapshot);

	void CreateUI();
	void SynchronizeUsername();
	UFUNCTION(Server, Reliable)
	void Server_SetUsername(const FString& NewUsername);
	void BindInventoryWidget();
	void UpdateWindowInputMode();

	void UseQuickSlot1();

	void UseQuickSlot2();

	void UseQuickSlot3();

	void UseQuickSlot4();

	void UseQuickSlotByIndex(int32 QuickSlotIndex);

private:
	// 조종 중인 캐릭터
	UPROPERTY(Transient)
	TWeakObjectPtr<class APFCharacter> ControlledCharacter;
	UPROPERTY(Transient)
	TWeakObjectPtr<class APFChest> NearestChest;
	UPROPERTY(Transient)
	TWeakObjectPtr<class APFCharacter> PendingViewCharacter;
	FPFCharacterSharedStateSnapshot SavedViewState;
	bool bHasSavedViewState = false;
	bool bPendingViewRestore = false;
	bool JumpButtonHeld = false;
	EPFDirection UpDownDir = IDLE;
	EPFDirection LeftRightDir = IDLE;
	EPFDirection LastMovementDirection = IDLE;
	FVector AimPoint = FVector::ZeroVector;
	// 플레이어 부활 타이머
	FTimerHandle PlayerRespawnTimerHandle;
	FTimerHandle UsernameSyncTimerHandle;

	// 로컬 캐릭터 HUD, 조준점
	UPROPERTY()
	TSubclassOf<class UUserWidget> SelfWidgetClass;
	UPROPERTY(Transient)
	class UPFCharacterWidget* SelfHPBar = nullptr;
	UPROPERTY(Transient)
	class UPFCrosshairWidget* CrosshairWidget = nullptr;
	TWeakObjectPtr<class UPFAttributeSet> HUDAttributeSet;

	friend class APFGameMode;

	ECHARACTER InitialSpawnCharacter = CHARACTER_TWINBLAST;
	bool bInitialSpawnReady = false;
	bool bInitialSpawnRequested = false;
	bool bInitialSpawnInProgress = false;
	bool bInitialSpawnComplete = false;

	// 인벤토리 창
	UPROPERTY()
	class UPFInventoryWidget* InventoryWidget;

	// 스탯 창
	UPROPERTY()
	class UPFStatWidget* StatWidget = nullptr;

	// 게임 메뉴
	UPROPERTY(Transient)
	class UPFMenuWidget* MenuWidget = nullptr;

	// 부활 대기 UI
	UPROPERTY(Transient)
	class UPFRespawnWidget* RespawnWidget = nullptr;
};
