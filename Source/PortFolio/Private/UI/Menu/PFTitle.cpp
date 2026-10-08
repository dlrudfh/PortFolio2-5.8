#include "UI/Menu/PFTitle.h"

#include "Character/PFDummy.h"
#include "EngineUtils.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFLobbyPlayerController.h"
#include "System/Framework/PFPlayerState.h"
#include "UI/HUD/PFMinimapAtlas.h"
#include "Types/SlateEnums.h" 
#include "Components/Button.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "System/Framework/PFPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Internationalization/Text.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"

void UPFTitle::NativeConstruct()
{
	Super::NativeConstruct();

	// 메뉴 버튼, 입력 이벤트 연결
	TitleUI = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TitleUI")));
	PFCHECK(TitleUI);
	CreateSessionButton = Cast<UButton>(GetWidgetFromName(TEXT("CreateSession")));
	PFCHECK(CreateSessionButton);
	TutorialButton = Cast<UButton>(GetWidgetFromName(TEXT("TutorialEntryButton")));
	TutorialText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TutorialEntryText")));
	PFCHECK(TutorialButton);
	JoinSessionButton = Cast<UButton>(GetWidgetFromName(TEXT("JoinSession")));
	PFCHECK(JoinSessionButton);
	ExitGameButton = Cast<UButton>(GetWidgetFromName(TEXT("ExitGame")));
	PFCHECK(ExitGameButton);
	SessionNameInputBox = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("SessionNameInput")));
	PFCHECK(SessionNameInputBox);
	SessionNameInputBox->OnTextCommitted.AddUniqueDynamic(this, &UPFTitle::InputComplete);
	ChooseCharacter = Cast<UTextBlock>(GetWidgetFromName(TEXT("ChooseCharacter")));
	SessionNoticeText = Cast<UTextBlock>(GetWidgetFromName(TEXT("SessionNotice")));
	RoomSetupPanel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("RoomSetupPanel")));
	RoomNamePanel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("RoomNamePanel")));
	LobbyPanel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("LobbyPanel")));
	MapSelectPanel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("MapSelectPanel")));
	CharacterSelectPanel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("CharacterSelectPanel")));
	PFCHECK(RoomSetupPanel && RoomNamePanel && LobbyPanel && MapSelectPanel && CharacterSelectPanel);
	LobbyActionButton = Cast<UButton>(GetWidgetFromName(TEXT("LobbyActionButton")));
	LobbyLeaveButton = Cast<UButton>(GetWidgetFromName(TEXT("LobbyLeaveButton")));
	LobbyTestButton = Cast<UButton>(GetWidgetFromName(TEXT("LobbyTestButton")));
	MapPreviewButton = Cast<UButton>(GetWidgetFromName(TEXT("MapPreviewButton")));
	LobbyActionText = Cast<UTextBlock>(GetWidgetFromName(TEXT("LobbyActionText")));
	LobbyActionCheck = Cast<UTextBlock>(GetWidgetFromName(TEXT("LobbyActionCheck")));
	LobbyRoomText = Cast<UTextBlock>(GetWidgetFromName(TEXT("LobbyRoomText")));
	LobbyModeText = Cast<UTextBlock>(GetWidgetFromName(TEXT("LobbyModeText")));
	MapPreviewImage = Cast<UImage>(GetWidgetFromName(TEXT("MapPreviewImage")));
	MapPreviewName = Cast<UTextBlock>(GetWidgetFromName(TEXT("MapPreviewName")));
	PFCHECK(LobbyActionButton && LobbyLeaveButton && LobbyTestButton && LobbyActionText && LobbyActionCheck);

	const TPair<const TCHAR*, const TCHAR*> ButtonBindings[] =
	{
		{ TEXT("CreateSession"), TEXT("CreateSessionNameInput") }, { TEXT("JoinSession"), TEXT("JoinSessionNameInput") },
		{ TEXT("ExitGame"), TEXT("ExitGame") }, { TEXT("TutorialEntryButton"), TEXT("StartTutorial") },
		{ TEXT("StoryModeButton"), TEXT("SelectStory") }, { TEXT("VersusModeButton"), TEXT("SelectVersus") },
		{ TEXT("PlayerCount1Button"), TEXT("SelectOnePlayer") }, { TEXT("PlayerCount2Button"), TEXT("SelectTwoPlayers") },
		{ TEXT("PlayerCount3Button"), TEXT("SelectThreePlayers") }, { TEXT("PlayerCount4Button"), TEXT("SelectFourPlayers") },
		{ TEXT("SetupBackButton"), TEXT("GoBack") }, { TEXT("SetupNextButton"), TEXT("SetupNext") },
		{ TEXT("RoomBackButton"), TEXT("GoBack") }, { TEXT("RoomConfirmButton"), TEXT("ConfirmRoom") },
		{ TEXT("LobbyActionButton"), TEXT("StartSession") }, { TEXT("LobbyLeaveButton"), TEXT("LeaveLobby") },
		{ TEXT("LobbyTestButton"), TEXT("AddTestPlayer") },
		{ TEXT("MapPreviewButton"), TEXT("OpenMapSelection") }, { TEXT("MapCloseButton"), TEXT("CloseMapSelection") },
		{ TEXT("Map3Button"), TEXT("SelectMap3") }, { TEXT("Map4Button"), TEXT("SelectMap4") },
		{ TEXT("Map5Button"), TEXT("SelectMap5") }, { TEXT("Map6Button"), TEXT("SelectMap6") },
		{ TEXT("Map7Button"), TEXT("SelectMap7") }, { TEXT("Map8Button"), TEXT("SelectMap8") },
		{ TEXT("TutorialMapButton"), TEXT("SelectTutorialMap") },
		{ TEXT("CharacterTwinBlastButton"), TEXT("SelectTwinBlast") }, { TEXT("CharacterKwangButton"), TEXT("SelectKwang") },
		{ TEXT("CharacterCloseButton"), TEXT("CloseCharacterSelection") }
	};
	for (const auto& Binding : ButtonBindings)
	{
		UButton* Button = Cast<UButton>(GetWidgetFromName(FName(Binding.Key)));
		PFCHECK(Button);
		FScriptDelegate Callback;
		Callback.BindUFunction(this, FName(Binding.Value));
		Button->OnClicked.AddUnique(Callback);
	}

	// 전체 지도 이미지, 참가자 이름표 연결
	MinimapAtlas = LoadObject<UPFMinimapAtlas>(nullptr, TEXT("/Game/GameData/UI/Minimap/DA_MinimapAtlas.DA_MinimapAtlas"));
	if (MinimapAtlas)
	{
		for (const FPFMinimapEntry& Entry : MinimapAtlas->Maps)
		{
			const FString ImageName = Entry.LevelName == TEXT("Tutorial") ? TEXT("TutorialMapImage") : Entry.LevelName.ToString() + TEXT("Image");
			if (UImage* Image = Cast<UImage>(GetWidgetFromName(FName(ImageName))))
			{
				Image->SetBrushFromTexture(Entry.Texture, false);
				Image->SetRenderTransformPivot(FVector2D(.5f, .5f));
				Image->SetRenderScale(FVector2D(-1.f, 1.f));
			}
		}
	}
	Nameplates.Reset();
	PlayerNames.Reset();
	PlayerChecks.Reset();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UWidget* Plate = GetWidgetFromName(FName(FString::Printf(TEXT("Nameplate%d"), Index)));
		Nameplates.Add(Plate);
		PlayerNames.Add(Cast<UTextBlock>(GetWidgetFromName(FName(FString::Printf(TEXT("PlayerName%d"), Index)))));
		PlayerChecks.Add(Cast<UTextBlock>(GetWidgetFromName(FName(FString::Printf(TEXT("PlayerCheck%d"), Index)))));
		if (Plate)
		{
			Plate->SetVisibility(ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(Plate->Slot))
			{
				PlateSlot->SetAnchors(FAnchors(0.f, 0.f));
				PlateSlot->SetAlignment(FVector2D(.5f, 1.f));
			}
		}
		if (PlayerChecks.Last()) PlayerChecks.Last()->SetText(FText::FromString(TEXT("\u2713")));
	}
	LobbyActionCheck->SetText(FText::FromString(TEXT("\u2713")));
	LobbyActionCheck->SetVisibility(ESlateVisibility::Hidden);
	ShowPage(0);
	RefreshSetup();
	SetSessionNotice(FText::GetEmpty());

	SetIsFocusable(true);

	// 화면 크기를 따라갈 배경판 연결
	CharacterSelectBackdrop.Reset();
	BackdropViewportSize = FIntPoint::ZeroValue;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("CharacterSelectBackdrop")))
		{
			CharacterSelectBackdrop = *It;
			break;
		}
	}

	// 캐릭터 선택 카메라 연결
	FString TargetName = "DummyCamera";

	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		ACameraActor* Cam = *It;
		if (Cam->ActorHasTag(FName(*TargetName)))
		{
			SelectionCamera = Cam;
			BaseCameraWidth = Cam->GetCameraComponent()->OrthoWidth;
			break;
		}
	}
}

void UPFTitle::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshLobby();
	if (CurrentPage != 3) return;

	AStaticMeshActor* Backdrop = CharacterSelectBackdrop.Get();
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!Backdrop || !LocalPlayer || !LocalPlayer->ViewportClient) return;

	FViewport* Viewport = LocalPlayer->ViewportClient->Viewport;
	if (!Viewport) return;

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0 || ViewportSize == BackdropViewportSize) return;

	UStaticMeshComponent* MeshComponent = Backdrop->GetStaticMeshComponent();
	UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	FSceneViewProjectionData ProjectionData;
	if (!Mesh || !LocalPlayer->GetProjectionData(Viewport, ProjectionData) || ProjectionData.IsPerspectiveProjection()) return;

	// 실제 직교 투영 범위에 배경의 가로, 세로를 각각 맞춤
	const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
	const double ProjectedWidth = FMath::Abs(ProjectionData.ProjectionMatrix.M[0][0]) * MeshSize.X;
	const double ProjectedHeight = FMath::Abs(ProjectionData.ProjectionMatrix.M[1][1]) * MeshSize.Y;
	if (ProjectedWidth <= UE_SMALL_NUMBER || ProjectedHeight <= UE_SMALL_NUMBER) return;

	FVector BackdropScale = Backdrop->GetActorScale3D();
	BackdropScale.X = 2.0 / ProjectedWidth;
	BackdropScale.Y = 2.0 / ProjectedHeight;
	Backdrop->SetActorScale3D(BackdropScale);
	BackdropViewportSize = ViewportSize;
}

void UPFTitle::NativeDestruct()
{
	for (APFDummy* Dummy : SelectionDummies)
		if (IsValid(Dummy)) Dummy->Destroy();
	SelectionDummies.Reset();
	DummyPlayers.Reset();
	DummyNameplateAnchors.Reset();
	PreviewPlayerCount = 0;
	Super::NativeDestruct();
}

// 싱글플레이 튜토리얼 시작
void UPFTitle::StartTutorial()
{
	if (bSessionBusy || CurrentPage != 0) return;
	if (UPFGameInstance* GameInstance = GetGameInstance<UPFGameInstance>())
	{
		GameInstance->StartTutorial();
	}
}

// 방 모드, 인원 설정 표시
void UPFTitle::CreateSessionNameInput()
{
	if (bSessionBusy || CurrentPage != 0) return;
	IsCreateSession = true;
	ShowPage(1);
	RefreshSetup();
}

// 참가할 세션 이름 입력
void UPFTitle::JoinSessionNameInput()
{
	ShowSessionNameInput(false);
}

// 세션 이름 입력창 표시
void UPFTitle::ShowSessionNameInput(bool bCreateSession)
{
	if (bSessionBusy || !SessionNameInputBox) return;
	IsCreateSession = bCreateSession;
	ShowPage(2);
	if (UTextBlock* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("RoomDialogTitle"))))
		Title->SetText(FText::FromString(bCreateSession ? TEXT("Create Session") : TEXT("Join Session")));
	if (UTextBlock* ConfirmText = Cast<UTextBlock>(GetWidgetFromName(TEXT("RoomConfirmButtonLabel"))))
		ConfirmText->SetText(FText::FromString(bCreateSession ? TEXT("Create") : TEXT("Join")));
	if (UTextBlock* Summary = Cast<UTextBlock>(GetWidgetFromName(TEXT("RoomSummaryText"))))
		Summary->SetVisibility(bCreateSession ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	SessionNameInputBox->SetKeyboardFocus();
}

// 세션 이름 입력 완료 처리
void UPFTitle::InputComplete(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter) ConfirmRoom();
}

// 캐릭터 선택 화면 표시
void UPFTitle::ShowCharacterSelection()
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!Session || !Session->bInitialized
		|| (Session->Phase != EPFSessionPhase::Lobby && Session->Phase != EPFSessionPhase::Starting)) return;
	SetSessionBusy(false);
	RefreshLobby();
}

// 참가 실패 시 입력 화면 복원
void UPFTitle::ShowJoinFailed()
{
	SetSessionBusy(false);
	ShowSessionNameInput(IsCreateSession);
}

// 세션 입력 제한, 입장 중 타이틀 숨김
void UPFTitle::SetSessionBusy(bool bBusy, bool bEnteringGame)
{
	bSessionBusy = bBusy;
	if (bEnteringGame && CurrentPage == 3)
	{
		if (GetVisibility() != ESlateVisibility::Collapsed)
		{
			SessionVisibility = GetVisibility();
		}
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		SetVisibility(SessionVisibility);
	}
	if (CreateSessionButton) CreateSessionButton->SetIsEnabled(!bBusy);
	if (TutorialButton) TutorialButton->SetIsEnabled(!bBusy);
	if (JoinSessionButton) JoinSessionButton->SetIsEnabled(!bBusy);
	if (SessionNameInputBox) SessionNameInputBox->SetIsEnabled(!bBusy);
	if (ExitGameButton) ExitGameButton->SetIsEnabled(!bEnteringGame);
	if (RoomSetupPanel) RoomSetupPanel->SetIsEnabled(!bBusy);
	if (RoomNamePanel) RoomNamePanel->SetIsEnabled(!bBusy);
	RefreshLobby();
}

// 세션 상태 안내 표시
void UPFTitle::SetSessionNotice(const FText& Message)
{
	if (!SessionNoticeText) return;
	SessionNoticeText->SetText(Message);
	SessionNoticeText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

// 호스트 시작, 게스트 준비 전환
void UPFTitle::StartSession()
{
	APFLobbyPlayerController* PC = Cast<APFLobbyPlayerController>(GetOwningPlayer());
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	const APFPlayerState* Player = PC ? PC->GetPlayerState<APFPlayerState>() : nullptr;
	if (bSessionBusy || CurrentPage != 3 || !PC || !Player || !Session
		|| !Session->bInitialized || Session->Phase != EPFSessionPhase::Lobby) return;
	if (Session->IsHost(Player))
	{
		if (Session->CanStart()) PC->Server_StartMatch();
	}
	else if (Player->HasLobbySelection()) PC->Server_SetReady(!Player->IsLobbyReady(), Session->SettingsRevision);
}

// 타이틀 화면 단계 전환
void UPFTitle::ShowPage(uint8 Page)
{
	CurrentPage = Page;
	if (Page != 3) PreviewPlayerCount = 0;
	if (LobbyTestButton) LobbyTestButton->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* Background = GetWidgetFromName(TEXT("TitleBackground")))
		Background->SetVisibility(Page < 3 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (TitleUI) TitleUI->SetVisibility(Page == 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (RoomSetupPanel) RoomSetupPanel->SetVisibility(Page == 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (RoomNamePanel) RoomNamePanel->SetVisibility(Page == 2 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (SessionNameInputBox) SessionNameInputBox->SetVisibility(Page == 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (LobbyPanel) LobbyPanel->SetVisibility(Page == 3 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (ChooseCharacter) ChooseCharacter->SetVisibility(ESlateVisibility::Collapsed);
	if (MapSelectPanel) MapSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (CharacterSelectPanel) CharacterSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
	for (APFDummy* Dummy : SelectionDummies)
		if (IsValid(Dummy))
		{
			Dummy->SetActorHiddenInGame(Page != 3);
			Dummy->SetActorEnableCollision(Page == 3);
			Dummy->SetLobbyPresentation(false, false);
		}
}

// 스토리 모드 선택
void UPFTitle::SelectStory()
{
	if (bSessionBusy || CurrentPage != 1) return;
	PendingMode = EPFSessionMode::Story;
	RefreshSetup();
}

// 대전 모드 선택
void UPFTitle::SelectVersus()
{
	if (bSessionBusy || CurrentPage != 1) return;
	PendingMode = EPFSessionMode::Versus;
	RefreshSetup();
}

// 1인 정원 선택
void UPFTitle::SelectOnePlayer()
{
	if (bSessionBusy || CurrentPage != 1) return;
	PendingCapacity = 1;
	RefreshSetup();
}

// 2인 정원 선택
void UPFTitle::SelectTwoPlayers()
{
	if (bSessionBusy || CurrentPage != 1) return;
	PendingCapacity = 2;
	RefreshSetup();
}

// 3인 정원 선택
void UPFTitle::SelectThreePlayers()
{
	if (bSessionBusy || CurrentPage != 1) return;
	PendingCapacity = 3;
	RefreshSetup();
}

// 4인 정원 선택
void UPFTitle::SelectFourPlayers()
{
	if (bSessionBusy || CurrentPage != 1) return;
	PendingCapacity = 4;
	RefreshSetup();
}

// 선택한 모드, 정원 표시
void UPFTitle::RefreshSetup()
{
	PendingCapacity = FMath::Clamp(PendingCapacity, 1, APFSessionGameState::GetMaxPlayers(PendingMode));
	const auto SetSelected = [](UButton* Button, bool bSelected)
	{
		if (!Button) return;
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = bSelected ? Style.Pressed : Style.Disabled;
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
	};
	SetSelected(Cast<UButton>(GetWidgetFromName(TEXT("StoryModeButton"))), PendingMode == EPFSessionMode::Story);
	SetSelected(Cast<UButton>(GetWidgetFromName(TEXT("VersusModeButton"))), PendingMode == EPFSessionMode::Versus);
	for (int32 Count = 1; Count <= 4; ++Count)
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(FName(FString::Printf(TEXT("PlayerCount%dButton"), Count)))))
		{
			SetSelected(Button, PendingCapacity == Count);
			Button->SetIsEnabled(!bSessionBusy && Count <= APFSessionGameState::GetMaxPlayers(PendingMode));
		}
	if (UTextBlock* Summary = Cast<UTextBlock>(GetWidgetFromName(TEXT("RoomSummaryText"))))
		Summary->SetText(FText::FromString(FString::Printf(TEXT("%s / %d %s"),
			PendingMode == EPFSessionMode::Story ? TEXT("Story Mode") : TEXT("Versus Mode"),
			PendingCapacity, PendingCapacity == 1 ? TEXT("Player") : TEXT("Players"))));
}

// 방 설정 후 이름 입력
void UPFTitle::SetupNext()
{
	if (CurrentPage == 1) ShowSessionNameInput(true);
}

// 방 생성, 참가 이전 단계 복귀
void UPFTitle::GoBack()
{
	if (bSessionBusy) return;
	if (CurrentPage == 2) ShowPage(IsCreateSession ? 1 : 0);
	else if (CurrentPage == 1) ShowPage(0);
}

// 입력한 이름으로 세션 생성, 참가
void UPFTitle::ConfirmRoom()
{
	if (bSessionBusy || CurrentPage != 2 || !SessionNameInputBox) return;
	SessionName = SessionNameInputBox->GetText().ToString().TrimStartAndEnd();
	if (SessionName.IsEmpty())
	{
		SessionNameInputBox->SetKeyboardFocus();
		return;
	}
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		if (IsCreateSession) GI->CreateGameSession(SessionName, PendingMode, PendingCapacity);
		else GI->JoinGameSession(SessionName);
	}
}

// 준비 전 로비 퇴장 요청
void UPFTitle::LeaveLobby()
{
	APFLobbyPlayerController* PC = Cast<APFLobbyPlayerController>(GetOwningPlayer());
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!bSessionBusy && CurrentPage == 3 && PC && Session && Session->CanLeave(PC->GetPlayerState<APFPlayerState>()))
		PC->Server_LeaveLobby();
}

// 로컬 대전 배치 미리보기 인원 추가
void UPFTitle::AddTestPlayer()
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (bSessionBusy || CurrentPage != 3 || !Session || !Session->bInitialized
		|| Session->Mode != EPFSessionMode::Versus || Session->Phase != EPFSessionPhase::Lobby) return;
	int32 PlayerCount = 0;
	for (APlayerState* Entry : Session->PlayerArray)
		if (const APFPlayerState* Player = Cast<APFPlayerState>(Entry); IsValid(Player)
			&& Player->GetLobbySlot() >= 0 && Player->GetLobbySlot() < Session->Capacity)
			++PlayerCount;
	PreviewPlayerCount = FMath::Clamp(PreviewPlayerCount + 1, 0, FMath::Max(0, 4 - PlayerCount));
	RefreshLobby();
}

// 호스트의 전체 맵 목록 표시
void UPFTitle::OpenMapSelection()
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	const APlayerController* PC = GetOwningPlayer();
	if (bSessionBusy || CurrentPage != 3 || !Session || !Session->bInitialized || !PC || !MapSelectPanel
		|| Session->Phase != EPFSessionPhase::Lobby || Session->Mode != EPFSessionMode::Versus
		|| !Session->IsHost(PC->PlayerState)) return;
	MapSelectPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (CharacterSelectPanel) CharacterSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// 맵 목록 닫기
void UPFTitle::CloseMapSelection()
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!bSessionBusy && Session && Session->Phase == EPFSessionPhase::Lobby && MapSelectPanel)
		MapSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// 대전 맵 변경 요청
void UPFTitle::SelectMap(FName MapName)
{
	APFLobbyPlayerController* PC = Cast<APFLobbyPlayerController>(GetOwningPlayer());
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (bSessionBusy || !PC || !Session || !Session->bInitialized || Session->Phase != EPFSessionPhase::Lobby
		|| Session->Mode != EPFSessionMode::Versus || !Session->IsHost(PC->PlayerState)) return;
	PC->Server_SelectMap(MapName);
	CloseMapSelection();
}

// Map3 선택
void UPFTitle::SelectMap3()
{
	SelectMap(TEXT("Map3"));
}

// Map4 선택
void UPFTitle::SelectMap4()
{
	SelectMap(TEXT("Map4"));
}

// Map5 선택
void UPFTitle::SelectMap5()
{
	SelectMap(TEXT("Map5"));
}

// Map6 선택
void UPFTitle::SelectMap6()
{
	SelectMap(TEXT("Map6"));
}

// Map7 선택
void UPFTitle::SelectMap7()
{
	SelectMap(TEXT("Map7"));
}

// Map8 선택
void UPFTitle::SelectMap8()
{
	SelectMap(TEXT("Map8"));
}

// 튜토리얼 전장 선택
void UPFTitle::SelectTutorialMap()
{
	SelectMap(TEXT("Tutorial"));
}

// TwinBlast 선택
void UPFTitle::SelectTwinBlast()
{
	SelectCharacter(CHARACTER_TWINBLAST);
}

// Kwang 선택
void UPFTitle::SelectKwang()
{
	SelectCharacter(CHARACTER_KWANG);
}

// 캐릭터 목록 닫기
void UPFTitle::CloseCharacterSelection()
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!bSessionBusy && Session && Session->Phase == EPFSessionPhase::Lobby && CharacterSelectPanel)
		CharacterSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// 서버에 본인 캐릭터 변경 요청
void UPFTitle::SelectCharacter(ECHARACTER Character)
{
	APFLobbyPlayerController* PC = Cast<APFLobbyPlayerController>(GetOwningPlayer());
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (bSessionBusy || CurrentPage != 3 || !PC || !Session || !Session->bInitialized
		|| Session->Phase != EPFSessionPhase::Lobby) return;
	PC->Server_SelectCharacter(Character);
	CloseCharacterSelection();
}

// 더미 클릭으로 본인 캐릭터 선택
void UPFTitle::HandleDummyClicked(APFDummy* Dummy)
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	const APlayerController* PC = GetOwningPlayer();
	const APFPlayerState* Player = PC ? PC->GetPlayerState<APFPlayerState>() : nullptr;
	if (bSessionBusy || CurrentPage != 3 || !IsValid(Dummy) || !Session || !Session->bInitialized || !Player
		|| Session->Phase != EPFSessionPhase::Lobby || (MapSelectPanel && MapSelectPanel->IsVisible())
		|| (CharacterSelectPanel && CharacterSelectPanel->IsVisible())) return;
	const int32 Index = SelectionDummies.IndexOfByKey(Dummy);
	if (Index == INDEX_NONE) return;
	if (Session->Mode == EPFSessionMode::Versus)
	{
		if (DummyPlayers.IsValidIndex(Index) && DummyPlayers[Index].Get() == Player && CharacterSelectPanel)
			CharacterSelectPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		if (!Player->HasLobbySelection() || Player->GetCharacter() != Dummy->GetDummyType()) Dummy->PlaySelection();
		SelectCharacter(Dummy->GetDummyType());
	}
}

// 복제된 로비 상태를 화면에 반영
void UPFTitle::RefreshLobby()
{
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!Session || !Session->bInitialized
		|| (Session->Phase != EPFSessionPhase::Lobby && Session->Phase != EPFSessionPhase::Starting))
	{
		PreviewPlayerCount = 0;
		if (LobbyTestButton)
		{
			LobbyTestButton->SetVisibility(ESlateVisibility::Collapsed);
			LobbyTestButton->SetIsEnabled(false);
		}
		return;
	}
	if (CurrentPage != 3) ShowPage(3);
	APlayerController* PC = GetOwningPlayer();
	if (ACameraActor* Cam = SelectionCamera.Get(); Cam && PC && (PC->GetViewTarget() != Cam || SelectionDummies.IsEmpty()))
	{
		// 로비 카메라의 고정 노출, 빛 번짐 조절
		UCameraComponent* Camera = Cam->GetCameraComponent();
		FPostProcessSettings& Settings = Camera->PostProcessSettings;
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Manual;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Settings.AutoExposureApplyPhysicalCameraExposure = false;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = 0.f;
		Settings.bOverride_AutoExposureBiasCurve = true;
		Settings.AutoExposureBiasCurve = nullptr;
		Settings.bOverride_LocalExposureHighlightContrastScale = true;
		Settings.LocalExposureHighlightContrastScale = 1.f;
		Settings.bOverride_LocalExposureShadowContrastScale = true;
		Settings.LocalExposureShadowContrastScale = 1.f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 0.15f;
		Camera->PostProcessBlendWeight = 1.f;
		PC->SetViewTargetWithBlend(Cam, 0.f);
		BackdropViewportSize = FIntPoint::ZeroValue;
	}
	const APFPlayerState* LocalPlayer = PC ? PC->GetPlayerState<APFPlayerState>() : nullptr;
	const bool bVersus = Session->Mode == EPFSessionMode::Versus;
	const bool bHost = Session->IsHost(LocalPlayer);
	const bool bMayEdit = !bSessionBusy && Session->Phase == EPFSessionPhase::Lobby;
	if (!bMayEdit)
	{
		if (MapSelectPanel) MapSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (CharacterSelectPanel) CharacterSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LobbyRoomText) LobbyRoomText->SetText(FText::FromString(Session->RoomName));
	if (LobbyModeText) LobbyModeText->SetText(FText::FromString(FString::Printf(TEXT("%s / %d %s"),
		bVersus ? TEXT("Versus Mode") : TEXT("Story Mode"), Session->Capacity,
		Session->Capacity == 1 ? TEXT("Player") : TEXT("Players"))));
	if (LobbyActionText) LobbyActionText->SetText(FText::FromString(bHost ? TEXT("Start") : TEXT("Ready")));
	if (LobbyActionCheck) LobbyActionCheck->SetVisibility(!bHost && LocalPlayer && LocalPlayer->IsLobbyReady()
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (LobbyActionButton) LobbyActionButton->SetIsEnabled(bMayEdit && LocalPlayer
		&& (bHost ? Session->CanStart() : LocalPlayer->HasLobbySelection()));
	if (LobbyLeaveButton) LobbyLeaveButton->SetIsEnabled(bMayEdit && Session->CanLeave(LocalPlayer));
	if (MapSelectPanel) MapSelectPanel->SetIsEnabled(bMayEdit && bHost && bVersus);
	if (CharacterSelectPanel) CharacterSelectPanel->SetIsEnabled(bMayEdit && LocalPlayer && bVersus);
	if (MapPreviewButton)
	{
		MapPreviewButton->SetVisibility(bVersus ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		MapPreviewButton->SetIsEnabled(bMayEdit && bHost);
	}
	if (MapPreviewName)
	{
		MapPreviewName->SetVisibility(bVersus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		MapPreviewName->SetText(FText::FromName(Session->SelectedMap));
	}
	if (MapPreviewImage)
	{
		MapPreviewImage->SetVisibility(bVersus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		UTexture2D* Texture = nullptr;
		if (MinimapAtlas)
			for (const FPFMinimapEntry& Entry : MinimapAtlas->Maps)
				if (Entry.LevelName == Session->SelectedMap) { Texture = Entry.Texture; break; }
		if (MapPreviewImage->GetBrush().GetResourceObject() != Texture)
			MapPreviewImage->SetBrushFromTexture(Texture, false);
		MapPreviewImage->SetRenderTransformPivot(FVector2D(.5f, .5f));
		MapPreviewImage->SetRenderScale(FVector2D(-1.f, 1.f));
	}

	TArray<APFPlayerState*> Players;
	for (APlayerState* Entry : Session->PlayerArray)
		if (APFPlayerState* Player = Cast<APFPlayerState>(Entry); IsValid(Player)
			&& Player->GetLobbySlot() >= 0 && Player->GetLobbySlot() < Session->Capacity)
			Players.Add(Player);
	Players.Sort([](const APFPlayerState& A, const APFPlayerState& B) { return A.GetLobbySlot() < B.GetLobbySlot(); });
	PreviewPlayerCount = bVersus ? FMath::Clamp(PreviewPlayerCount, 0, FMath::Max(0, 4 - Players.Num())) : 0;
	const int32 DisplayedCount = bVersus ? Players.Num() + PreviewPlayerCount : 2;
	if (LobbyTestButton)
	{
		LobbyTestButton->SetVisibility(bVersus ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		LobbyTestButton->SetIsEnabled(bVersus && bMayEdit && DisplayedCount < 4);
	}
	bool bRebuild = bDummiesAreVersus != bVersus || SelectionDummies.Num() != DisplayedCount
		|| (bVersus && DummyPlayers.Num() != Players.Num());
	if (bVersus && !bRebuild)
		for (int32 Index = 0; Index < Players.Num(); ++Index)
			if (!DummyPlayers.IsValidIndex(Index) || DummyPlayers[Index].Get() != Players[Index]) { bRebuild = true; break; }
	if (bRebuild) RebuildDummies(Players, bVersus);
	else if (bVersus)
	{
		const ULocalPlayer* ViewportPlayer = GetOwningLocalPlayer();
		const FViewport* Viewport = ViewportPlayer && ViewportPlayer->ViewportClient ? ViewportPlayer->ViewportClient->Viewport : nullptr;
		if (Viewport && Viewport->GetSizeXY() != DummyViewportSize) UpdateDummyLayout();
	}
	const bool bPopupOpen = (MapSelectPanel && MapSelectPanel->IsVisible()) || (CharacterSelectPanel && CharacterSelectPanel->IsVisible());
	for (int32 Index = 0; Index < SelectionDummies.Num(); ++Index)
	{
		APFDummy* Dummy = SelectionDummies[Index];
		if (!IsValid(Dummy)) continue;
		if (bVersus)
		{
			APFPlayerState* Player = Players.IsValidIndex(Index) ? Players[Index] : nullptr;
			if (!Player)
			{
				Dummy->SetLobbyPresentation(false, false);
				Dummy->SetActorEnableCollision(false);
				continue;
			}
			if (Dummy->GetDummyType() != Player->GetCharacter())
			{
				Dummy->SetMesh(Player->GetCharacter());
				Dummy->PlaySelection();
			}
			Dummy->SetLobbyPresentation(Player->HasLobbySelection(), bMayEdit && !bPopupOpen && Player == LocalPlayer);
		}
		else
		{
			const bool bSelected = Players.ContainsByPredicate([Dummy](const APFPlayerState* Player)
			{
				return Player->HasLobbySelection() && Player->GetCharacter() == Dummy->GetDummyType();
			});
			Dummy->SetLobbyPresentation(bSelected, bMayEdit && !bPopupOpen && LocalPlayer && LocalPlayer->GetLobbySlot() >= 0);
		}
	}
	UpdateNameplates(Players, bVersus);
}

// 참가 인원에 맞춰 로컬 캐릭터 더미 배치
void UPFTitle::RebuildDummies(const TArray<APFPlayerState*>& Players, bool bVersus)
{
	for (APFDummy* Dummy : SelectionDummies)
		if (IsValid(Dummy)) Dummy->Destroy();
	SelectionDummies.Reset();
	DummyPlayers.Reset();
	DummyNameplateAnchors.Reset();
	bDummiesAreVersus = bVersus;
	const int32 Count = bVersus ? Players.Num() + PreviewPlayerCount : 2;
	const FVector Center(1450.f, 1000.f, 100.f);
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		APFPlayerState* Player = Players.IsValidIndex(Index) ? Players[Index] : nullptr;
		const ECHARACTER Character = bVersus ? (Player ? Player->GetCharacter() : CHARACTER_TWINBLAST)
			: (Index == 0 ? CHARACTER_TWINBLAST : CHARACTER_KWANG);
		const FVector Location = bVersus ? Center
			: FVector(Index == 0 ? 1300.f : 1600.f, 1000.f, Index == 0 ? 100.f : 90.f);
		APFDummy* Dummy = GetWorld()->SpawnActor<APFDummy>(APFDummy::StaticClass(), Location, FRotator::ZeroRotator, Parameters);
		SelectionDummies.Add(Dummy);
		DummyNameplateAnchors.Add(Location + FVector(0.f, 0.f,
			Dummy ? Dummy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 28.f : 0.f));
		if (bVersus && Player) DummyPlayers.Add(Player);
		if (Dummy)
		{
			Dummy->SetReplicates(false);
			Dummy->SetMesh(Character);
			Dummy->GetMesh()->SetLightingChannels(true, true, false);
			Dummy->SetTitle(this);
			Dummy->SetLobbyPresentation(false, false);
			if (bVersus && !Player) Dummy->SetActorEnableCollision(false);
		}
	}
	if (bVersus) UpdateDummyLayout();
}

// 고정 카메라 범위 안에서 대전 더미 중앙 정렬
void UPFTitle::UpdateDummyLayout()
{
	const ACameraActor* CameraActor = SelectionCamera.Get();
	const UCameraComponent* Camera = CameraActor ? CameraActor->GetCameraComponent() : nullptr;
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
	DummyViewportSize = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
	float HorizontalWidth = Camera ? Camera->OrthoWidth : BaseCameraWidth;
	if (Camera && !Camera->bConstrainAspectRatio && LocalPlayer && DummyViewportSize.X > 0 && DummyViewportSize.Y > 0)
	{
		const EAspectRatioAxisConstraint Axis = Camera->bOverrideAspectRatioAxisConstraint
			? Camera->AspectRatioAxisConstraint.GetValue() : LocalPlayer->AspectRatioAxisConstraint.GetValue();
		const bool bMaintainX = Axis == AspectRatio_MaintainXFOV
			|| (Axis == AspectRatio_MajorAxisFOV && DummyViewportSize.X > DummyViewportSize.Y);
		if (!bMaintainX) HorizontalWidth *= static_cast<float>(DummyViewportSize.X) / DummyViewportSize.Y;
	}
	const int32 Count = SelectionDummies.Num();
	const float MaxSpread = FMath::Max(0.f, HorizontalWidth * .5f - 80.f);
	const float Spread = FMath::Min(MaxSpread, Count == 2 ? 150.f : (Count == 3 ? 210.f : 240.f));
	const FVector Right = CameraActor ? CameraActor->GetActorRightVector() : FVector::RightVector;
	const FVector Center(1450.f, 1000.f, 100.f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		APFDummy* Dummy = SelectionDummies[Index];
		if (!IsValid(Dummy)) continue;
		const float Offset = Count > 1 ? FMath::Lerp(-Spread, Spread, static_cast<float>(Index) / (Count - 1)) : 0.f;
		const FVector Location = Center + Right * Offset;
		Dummy->SetActorLocation(Location);
		if (DummyNameplateAnchors.IsValidIndex(Index))
			DummyNameplateAnchors[Index] = Location + FVector(0.f, 0.f, Dummy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 28.f);
	}
}

// 더미 위 고정 위치에 이름, 준비 체크 배치
void UPFTitle::UpdateNameplates(const TArray<APFPlayerState*>& Players, bool bVersus)
{
	APlayerController* PC = GetOwningPlayer();
	int32 StackedNames[2] = { 0, 0 };
	for (int32 Index = 0; Index < Nameplates.Num(); ++Index)
	{
		UWidget* Plate = Nameplates[Index];
		if (!Plate) continue;
		Plate->SetVisibility(ESlateVisibility::Collapsed);
		const APFPlayerState* Player = Players.IsValidIndex(Index) ? Players[Index] : nullptr;
		const bool bPreview = bVersus && Index >= Players.Num() && Index < Players.Num() + PreviewPlayerCount;
		if (!PC || (!bPreview && (!Player || !Player->HasLobbySelection()))) continue;
		const int32 DummyIndex = bVersus ? Index : (Player->GetCharacter() == CHARACTER_TWINBLAST ? 0 : 1);
		APFDummy* Dummy = SelectionDummies.IsValidIndex(DummyIndex) ? SelectionDummies[DummyIndex] : nullptr;
		if (!IsValid(Dummy) || !DummyNameplateAnchors.IsValidIndex(DummyIndex)) continue;
		FVector2D ScreenPosition;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, DummyNameplateAnchors[DummyIndex], ScreenPosition, false)) continue;
		if (!bVersus) ScreenPosition.Y -= StackedNames[DummyIndex]++ * 38.f;
		if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(Plate->Slot)) PlateSlot->SetPosition(ScreenPosition);
		if (PlayerNames.IsValidIndex(Index) && PlayerNames[Index]) PlayerNames[Index]->SetText(FText::FromString(bPreview
			? FString::Printf(TEXT("Test Player %d"), Index - Players.Num() + 1) : Player->GetPlayerName()));
		if (PlayerChecks.IsValidIndex(Index) && PlayerChecks[Index]) PlayerChecks[Index]->SetVisibility(!bPreview && Player->IsLobbyReady()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

// 게임 종료 요청
void UPFTitle::ExitGame()
{
	if (!IsInViewport() || GetVisibility() == ESlateVisibility::Collapsed || !ExitGameButton->GetIsEnabled())
	{
		return;
	}
	PFLOG(Warning, TEXT("Exit Game"));
	GetWorld()->GetGameInstance<UPFGameInstance>()->ExitGame();
}
