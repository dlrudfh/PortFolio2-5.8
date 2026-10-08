
#pragma once

#include "PortFolio/PortFolio.h"
#include "Blueprint/UserWidget.h"
#include "System/Framework/PFSessionGameState.h"
#include "PFTitle.generated.h"


// 타이틀, 캐릭터 선택 위젯
UCLASS()
class UPFTitle : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	void StartSession();
	void ShowCharacterSelection();
	void ShowJoinFailed();
	void SetSessionBusy(bool bBusy, bool bEnteringGame = false);
	void SetSessionNotice(const FText& Message);
	void HandleDummyClicked(class APFDummy* Dummy);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void StartTutorial();
	UFUNCTION()
	void CreateSessionNameInput();
	UFUNCTION()
	void JoinSessionNameInput();
	void ShowSessionNameInput(bool bCreateSession);
	UFUNCTION()
	void InputComplete(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void ExitGame();
	UFUNCTION()
	void SelectStory();
	UFUNCTION()
	void SelectVersus();
	UFUNCTION()
	void SelectOnePlayer();
	UFUNCTION()
	void SelectTwoPlayers();
	UFUNCTION()
	void SelectThreePlayers();
	UFUNCTION()
	void SelectFourPlayers();
	UFUNCTION()
	void SetupNext();
	UFUNCTION()
	void GoBack();
	UFUNCTION()
	void ConfirmRoom();
	UFUNCTION()
	void LeaveLobby();
	UFUNCTION()
	void AddTestPlayer();
	UFUNCTION()
	void OpenMapSelection();
	UFUNCTION()
	void CloseMapSelection();
	UFUNCTION()
	void SelectMap3();
	UFUNCTION()
	void SelectMap4();
	UFUNCTION()
	void SelectMap5();
	UFUNCTION()
	void SelectMap6();
	UFUNCTION()
	void SelectMap7();
	UFUNCTION()
	void SelectMap8();
	UFUNCTION()
	void SelectTutorialMap();
	UFUNCTION()
	void SelectTwinBlast();
	UFUNCTION()
	void SelectKwang();
	UFUNCTION()
	void CloseCharacterSelection();
	void SelectMap(FName MapName);
	void SelectCharacter(ECHARACTER Character);
	void RefreshSetup();
	void RefreshLobby();
	void RebuildDummies(const TArray<class APFPlayerState*>& Players, bool bVersus);
	void UpdateDummyLayout();
	void UpdateNameplates(const TArray<class APFPlayerState*>& Players, bool bVersus);
	void ShowPage(uint8 Page);

private:
	// 타이틀 메뉴 영역
	UPROPERTY()
	class UCanvasPanel* TitleUI;
	UPROPERTY()
	class UButton* CreateSessionButton;
	UPROPERTY()
	class UButton* JoinSessionButton;
	UPROPERTY()
	class UButton* ExitGameButton;
	UPROPERTY()
	class UButton* TutorialButton;
	UPROPERTY()
	class UTextBlock* TutorialText;
	// 세션 이름 입력창
	UPROPERTY()
	class UEditableTextBox* SessionNameInputBox;
	UPROPERTY()
	class UTextBlock* ChooseCharacter;
	// 연결 종료, 세션 복구 안내
	UPROPERTY()
	class UTextBlock* SessionNoticeText;
	TWeakObjectPtr<class AStaticMeshActor> CharacterSelectBackdrop;
	FIntPoint BackdropViewportSize = FIntPoint::ZeroValue;
	TWeakObjectPtr<class ACameraActor> SelectionCamera;
	float BaseCameraWidth = 0.f;
	UPROPERTY()
	TArray<TObjectPtr<class APFDummy>> SelectionDummies;
	TArray<TWeakObjectPtr<class APFPlayerState>> DummyPlayers;
	TArray<FVector> DummyNameplateAnchors;
	FIntPoint DummyViewportSize = FIntPoint::ZeroValue;
	UPROPERTY()
	class UCanvasPanel* RoomSetupPanel = nullptr;
	UPROPERTY()
	class UCanvasPanel* RoomNamePanel = nullptr;
	UPROPERTY()
	class UCanvasPanel* LobbyPanel = nullptr;
	UPROPERTY()
	class UCanvasPanel* MapSelectPanel = nullptr;
	UPROPERTY()
	class UCanvasPanel* CharacterSelectPanel = nullptr;
	UPROPERTY()
	class UButton* LobbyActionButton = nullptr;
	UPROPERTY()
	class UButton* LobbyLeaveButton = nullptr;
	UPROPERTY()
	class UButton* LobbyTestButton = nullptr;
	UPROPERTY()
	class UButton* MapPreviewButton = nullptr;
	UPROPERTY()
	class UTextBlock* LobbyActionText = nullptr;
	UPROPERTY()
	class UTextBlock* LobbyActionCheck = nullptr;
	UPROPERTY()
	class UTextBlock* LobbyRoomText = nullptr;
	UPROPERTY()
	class UTextBlock* LobbyModeText = nullptr;
	UPROPERTY()
	class UImage* MapPreviewImage = nullptr;
	UPROPERTY()
	class UTextBlock* MapPreviewName = nullptr;
	// 전체 지도 이미지 목록
	UPROPERTY()
	class UPFMinimapAtlas* MinimapAtlas = nullptr;
	UPROPERTY()
	TArray<TObjectPtr<class UWidget>> Nameplates;
	UPROPERTY()
	TArray<TObjectPtr<class UTextBlock>> PlayerNames;
	UPROPERTY()
	TArray<TObjectPtr<class UTextBlock>> PlayerChecks;
	EPFSessionMode PendingMode = EPFSessionMode::Story;
	int32 PendingCapacity = 2;
	int32 PreviewPlayerCount = 0;
	uint8 CurrentPage = 0;
	bool bDummiesAreVersus = false;

	bool IsCreateSession = true;
	bool bSessionBusy = false;
	ESlateVisibility SessionVisibility = ESlateVisibility::SelfHitTestInvisible;
	FString SessionName;
};
