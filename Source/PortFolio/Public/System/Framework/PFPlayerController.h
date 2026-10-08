
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
	virtual void OnPossess(APawn* aPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_Pawn() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void SeamlessTravelFrom(APlayerController* OldPC) override;
	virtual bool TryGetCombatAim(FVector& OutAimPoint) override;
	void SetChest(class APFChest* Chest = nullptr);
	void CaptureCharacterState();
	void RestoreCharacterState();
	void BindCharacterHUD();
	void ResetGameplayInput();
	void SetCampaignWaiting(bool bWaiting);
	bool IsCampaignWaiting() const { return bCampaignWaiting; }
	void SetCampaignViewState(const FPFCharacterSharedStateSnapshot& Snapshot);
	bool GetCampaignGuide(FVector& Location) const;
	class UPFTutorialWidget* AttachTutorial(class APFTutorialManager* Manager);
	void DetachTutorial(class APFTutorialManager* Manager);
	void GetTutorialWindows(class UPFInventoryWidget*& OutInventory, class UPFStatWidget*& OutStats, bool& bOutMenuOpen) const;
	void RequestTutorialBot();
	UFUNCTION(Client, Reliable)
	void Client_NotifyTutorialChest(class APFTutorialManager* Manager, FName ItemName, FVector ItemLocation, bool bCollected);
	UFUNCTION(Client, Reliable)
	void Client_NotifyTutorialBot(class APFTutorialManager* Manager, class APFCharacter* Bot, bool bDefeated);

	ECHARACTER GetCharacter() { return GetPlayerState<APFPlayerState>()->GetCharacter(); }
	void SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_SetCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_RequestInitialSpawn(ECHARACTER SelectedCharacter);
	UFUNCTION(Client, Reliable)
	void Client_PrepareMapPresentation(bool bCampaign);
	UFUNCTION(Client, Reliable)
	void Client_StartRespawnCountdown(double RespawnEndServerTime, float RespawnDuration);
	UFUNCTION(Client, Reliable)
	void Client_StopRespawnCountdown();
	void ToggleInventory();
	void ToggleStats();
	void ToggleShop();
	bool IsShopOpen() const;
	bool CanUseUIInput() const;
	bool CanUseSettingsInput() const;
	bool CanUseCampaignResultInput() const;
	void OpenMenu();
	FString GetUsername() const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	void BeginMapPresentation();
	bool PrepareMapEntryView();
	void UpdateMapPresentation(float DeltaTime);
	void UpdateScreenFade(float DeltaTime);
	void ReleaseScreenFade();
	void UpdateUIAvailability();
	void UpdateCampaignPresentation();
	void ReleaseCampaignPresentation();
	void SkipCampaignCamera();
	UFUNCTION(Server, Reliable)
	void Server_SkipCampaignCamera();
	class APFCharacter* GetControlledCharacter() const;
	bool IsCurrentCharacter(const class APFCharacter* ControlledPawn) const;
	bool CanUseGameplayInput(const class APFCharacter* ControlledPawn) const;
	void BindControlledCharacter();
	void UnbindControlledCharacter();
	void HandleCharacterReady(class APFCharacter* ControlledPawn);
	void HandleCharacterDied(class APFCharacter* ControlledPawn);
	void HandleCharacterLanded(class APFCharacter* ControlledPawn);
	void RefreshPawnOverlaps();
	void UpdateCrosshairVisibility();
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
	void ChangeTestMap();
	void SpawnTestTwinblastEnemy();
	void SpawnTestKwangEnemy();
	FVector CalculateAimPoint(const class APFCharacter* ControlledPawn, bool* bOutCharacterTargeted = nullptr) const;

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
	void Server_ChangeTestMap(class APFCharacter* ControlledPawn);
	UFUNCTION(Server, Reliable)
	void Server_SpawnTestEnemy(class APFCharacter* ControlledPawn, bool bSpawnTwinblast);
	UFUNCTION(Server, Reliable)
	void Server_RequestTutorialBot(class APFTutorialManager* Manager, class APFCharacter* ControlledPawn);
	UFUNCTION(Client, Reliable)
	void Client_RestoreCharacterState(class APFCharacter* ControlledPawn, FPFCharacterSharedStateSnapshot Snapshot);

	void CreateUI();
	void SynchronizeUsername();
	UFUNCTION(Server, Reliable)
	void Server_SetUsername(const FString& NewUsername);
	void BindInventoryWidget();
	void UpdateWindowInputMode();
	void ReleaseGameplayInputForWindow();
	void CloseShop();

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
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> MapLoadingCamera;
	TArray<TWeakObjectPtr<class UStreamableRenderAsset>> MapRenderAssets;
	TArray<TWeakObjectPtr<class UPrimitiveComponent>> MapPrimitives;
	float MapReadyTime = 0.f;
	float MapFadeInRemaining = 0.f;
	float ScreenFadeOpacity = 0.f;
	// 화면 전체 페이드 덮개
	TSharedPtr<class SBorder> ScreenFadeWidget;
	TWeakObjectPtr<class UGameViewportClient> ScreenFadeViewport;
	bool bMapFadePending = false;
	bool bMapStreamingPrimed = false;
	bool bMapPresentationReady = false;
	bool bMapRoleConfirmed = false;
	bool bMapUsesCampaign = false;
	UPROPERTY(Replicated)
	bool bCampaignWaiting = false;
	UPROPERTY(Transient)
	TObjectPtr<class UPFCampaignWidget> CampaignWidget;
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> CampaignCamera;
	TWeakObjectPtr<AActor> CampaignPreviousView;
	TWeakObjectPtr<class APFCampaignDirector> PresentedCampaign;
	UPROPERTY(Replicated) FVector CampaignGuide = FVector::ZeroVector;
	UPROPERTY(Replicated) bool bHasCampaignGuide = false;
	float CampaignGuideRemaining = 0.f;
	bool bCampaignInputLocked = false;
	bool bCampaignCameraActive = false;
	bool bCampaignResultVisible = false;
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
	UPROPERTY(Transient)
	class UPFMinimapWidget* MinimapWidget = nullptr;
	UPROPERTY(Transient)
	class UPFTutorialWidget* TutorialWidget = nullptr;
	// 현재 맵의 튜토리얼 관리자
	TWeakObjectPtr<class APFTutorialManager> TutorialManager;
	TWeakObjectPtr<class UPFAttributeSet> HUDAttributeSet;

	friend class APFGameMode;

	ECHARACTER InitialSpawnCharacter = CHARACTER_TWINBLAST;
	bool bInitialSpawnReady = false;
	bool bInitialSpawnRequested = false;
	bool bInitialSpawnInProgress = false;
	bool bInitialSpawnComplete = false;

	// 인벤토리 창
	UPROPERTY()
	TSubclassOf<class UPFInventoryWidget> InventoryWidgetClass;
	UPROPERTY()
	class UPFInventoryWidget* InventoryWidget;

	// 스탯 창
	UPROPERTY()
	TSubclassOf<class UPFStatWidget> StatWidgetClass;
	UPROPERTY()
	class UPFStatWidget* StatWidget = nullptr;

	// 상점 창
	UPROPERTY(Transient)
	class UPFShopWidget* ShopWidget = nullptr;

	// 게임 메뉴
	UPROPERTY()
	TSubclassOf<class UPFMenuWidget> MenuWidgetClass;
	UPROPERTY(Transient)
	class UPFMenuWidget* MenuWidget = nullptr;

	// 부활 대기 UI
	UPROPERTY()
	TSubclassOf<class UPFRespawnWidget> RespawnWidgetClass;
	UPROPERTY(Transient)
	class UPFRespawnWidget* RespawnWidget = nullptr;
};
