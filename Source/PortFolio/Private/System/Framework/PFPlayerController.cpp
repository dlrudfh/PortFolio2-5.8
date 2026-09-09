#include "System/Framework/PFPlayerController.h"

#include "Character/PFPlayer.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFGameMode.h"
#include "UI/Inventory/PFInventoryWidget.h"
#include "UI/HUD/PFStatWidget.h"
#include "UI/HUD/PFRespawnWidget.h"
#include "InputCoreTypes.h"
#include "Misc/PackageName.h"

void APFPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	PFLOG_W;

	GetViewportSize(CURRENTSCREENX, CURRENTSCREENY);
}

void APFPlayerController::OnPossess(APawn* aPawn)
{
	PFLOG_W;
	Super::OnPossess(aPawn);
	BindInventoryWidget();
	if (aPawn && GetPawn() == aPawn)
	{
		Client_StopRespawnCountdown();
	}
}

void APFPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindInventoryWidget();
}

void APFPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 인벤토리 구독 해제, 위젯 제거
	if (InventoryWidget)
	{
		InventoryWidget->BindPlayerState(nullptr);
		InventoryWidget->RemoveFromParent();
		InventoryWidget = nullptr;
	}

	// 부활 대기 UI 정리
	if (RespawnWidget)
	{
		RespawnWidget->StopCountdown();
		RespawnWidget->RemoveFromParent();
		RespawnWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// 선택 캐릭터 전달
void APFPlayerController::SetCharacter(ECHARACTER SelectedCharacter)
{
	if (HasAuthority())
	{
		if (APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>())
		{
			PFPlayerState->SetCharacter(SelectedCharacter);
		}
	}
	else
	{
		Server_SetCharacter(SelectedCharacter);
	}
}

// 서버에 최초 생성 요청 전달
void APFPlayerController::Server_RequestInitialSpawn_Implementation(ECHARACTER SelectedCharacter)
{
	UWorld* World = GetWorld();
	APFGameMode* GameMode = World ? World->GetAuthGameMode<APFGameMode>() : nullptr;
	if (GameMode)
	{
		GameMode->RequestInitialSpawn(this, SelectedCharacter);
	}
}

// 서버 캐릭터 선택 반영
void APFPlayerController::Server_SetCharacter_Implementation(ECHARACTER SelectedCharacter)
{
	SetCharacter(SelectedCharacter);
}

// 소유 플레이어의 부활 대기 표시
void APFPlayerController::Client_StartRespawnCountdown_Implementation(double RespawnEndServerTime, float RespawnDuration)
{
	if (!IsLocalController() || RespawnDuration <= 0.f)
	{
		return;
	}

	if (!RespawnWidget)
	{
		RespawnWidget = CreateWidget<UPFRespawnWidget>(this, UPFRespawnWidget::StaticClass());
		if (!RespawnWidget)
		{
			PFLOG(Warning, TEXT("Respawn widget creation failed"));
			return;
		}
		RespawnWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!RespawnWidget->IsInViewport())
	{
		RespawnWidget->AddToPlayerScreen(etoi(PLAYERSTAT) + 4);
	}
	RespawnWidget->StartCountdown(RespawnEndServerTime, RespawnDuration);
}

// 소유 플레이어의 부활 대기 종료
void APFPlayerController::Client_StopRespawnCountdown_Implementation()
{
	if (IsLocalController() && RespawnWidget)
	{
		RespawnWidget->StopCountdown();
	}
}

void APFPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 메뉴, 인벤토리, 퀵슬롯 입력 연결
	InputComponent->BindAction(TEXT("OpenMenu"), EInputEvent::IE_Pressed, this, &APFPlayerController::OpenMenu);
	InputComponent->BindAction(TEXT("Inventory"), EInputEvent::IE_Pressed, this, &APFPlayerController::ToggleInventory);
	InputComponent->BindAction(TEXT("PlayerStats"), EInputEvent::IE_Pressed, this, &APFPlayerController::ToggleStats);

	InputComponent->BindKey(EKeys::One, EInputEvent::IE_Pressed, this, &APFPlayerController::UseQuickSlot1);
	InputComponent->BindKey(EKeys::Two, EInputEvent::IE_Pressed, this, &APFPlayerController::UseQuickSlot2);
	InputComponent->BindKey(EKeys::Three, EInputEvent::IE_Pressed, this, &APFPlayerController::UseQuickSlot3);
	InputComponent->BindKey(EKeys::Four, EInputEvent::IE_Pressed, this, &APFPlayerController::UseQuickSlot4);
}

void APFPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 게임 입력, 마우스 캡처 설정
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
	{
		ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently);
	}

	if (!IsLocalController())
	{
		return;
	}

	UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
	if (!GI)
	{
		return;
	}
	GI->CreateTitle();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		SetCharacter(GI->GetCharacterType());
	}
	else
	{
		CreateUI();
		Server_RequestInitialSpawn(GI->GetCharacterType());
	}
}

void APFPlayerController::Tick(float DeltaTime)
{

	GetViewportSize(CURRENTSCREENX, CURRENTSCREENY);
}

// 인벤토리, 스탯 UI 생성
void APFPlayerController::CreateUI()
{
	if (!IsLocalController())
	{
		return;
	}

	// 인벤토리, 퀵슬롯 UI 생성
	if (!InventoryWidget)
	{
		InventoryWidget = CreateWidget<UPFInventoryWidget>(this, UPFInventoryWidget::StaticClass());
		if (InventoryWidget)
		{
			PFLOG(Warning, TEXT("Inventory Widget Created"));
			InventoryWidget->AddToViewport(etoi(PLAYERSTAT) + 1);
			InventoryWidget->SetVisibility(ESlateVisibility::Visible);
			InventoryWidget->SetInventoryWindowVisible(false);
			BindInventoryWidget();
		}
		else
		{
			PFLOG(Warning, TEXT("Inventory Widget Create Failed"));
		}
	}

	// 스탯 창 생성
	if (!StatWidget)
	{
		StatWidget = CreateWidget<UPFStatWidget>(this, UPFStatWidget::StaticClass());
		if (StatWidget)
		{
			StatWidget->AddToViewport(etoi(PLAYERSTAT) + 2);
			StatWidget->SetVisibility(ESlateVisibility::Visible);
			StatWidget->SetStatWindowVisible(false);
		}
		else
		{
			PFLOG(Warning, TEXT("Stat Widget Create Failed"));
		}
	}
}

// 메뉴 열기 (미구현)
void APFPlayerController::OpenMenu()
{
}

// 인벤토리에 PlayerState 연결
void APFPlayerController::BindInventoryWidget()
{
	if (!IsLocalController() || !InventoryWidget)
	{
		return;
	}

	InventoryWidget->BindPlayerState(GetPlayerState<APFPlayerState>());
}

// 인벤토리 창 표시 전환
void APFPlayerController::ToggleInventory()
{
	if (!IsLocalController())
	{
		PFLOG(Warning, TEXT("ToggleInventory Rejected Because Not LocalController"));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const FString LevelName = FPackageName::GetShortName(World->GetMapName());
		if (LevelName.Contains(TEXT("Title")))
		{
			PFLOG(Warning, TEXT("ToggleInventory Rejected In Title Level"));
			return;
		}
	}

	PFLOG(Warning, TEXT("ToggleInventory Called"));

	if (!InventoryWidget)
	{
		PFLOG(Warning, TEXT("Inventory Widget Was Null Before Toggle"));
		CreateUI();
	}

	if (!InventoryWidget)
	{
		PFLOG(Warning, TEXT("Inventory Widget Still Null After CreateUI"));
		return;
	}

	const bool bIsWindowVisible = InventoryWidget->IsInventoryWindowVisible();
	if (bIsWindowVisible)
	{
		PFLOG(Warning, TEXT("Inventory Widget Hidden"));
		InventoryWidget->SetInventoryWindowVisible(false);
		UpdateWindowInputMode();
		return;
	}

	// 스탯 창을 닫고 인벤토리 표시
	if (StatWidget)
	{
		StatWidget->SetStatWindowVisible(false);
	}
	InventoryWidget->RefreshInventory();
	PFLOG(Warning, TEXT("Inventory Widget Shown"));
	InventoryWidget->SetInventoryWindowVisible(true);
	UpdateWindowInputMode();
}

// 스탯 창 표시 전환
void APFPlayerController::ToggleStats()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const FString LevelName = FPackageName::GetShortName(World->GetMapName());
		if (LevelName.Contains(TEXT("Title")))
		{
			return;
		}
	}

	if (!StatWidget)
	{
		CreateUI();
	}
	if (!StatWidget)
	{
		return;
	}

	const bool bWillOpen = !StatWidget->IsStatWindowVisible();
	if (bWillOpen && InventoryWidget)
	{
		InventoryWidget->SetInventoryWindowVisible(false);
	}

	StatWidget->SetStatWindowVisible(bWillOpen);
	UpdateWindowInputMode();
}

// 열린 창에 맞춰 입력, 커서 전환
void APFPlayerController::UpdateWindowInputMode()
{
	const bool bInventoryOpen = InventoryWidget && InventoryWidget->IsInventoryWindowVisible();
	const bool bStatsOpen = StatWidget && StatWidget->IsStatWindowVisible();
	// 창이 닫히면 게임 입력 복원
	if (!bInventoryOpen && !bStatsOpen)
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
		{
			ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently);
		}
		return;
	}

	// 열린 창에 포커스, 마우스 입력 전달
	FInputModeGameAndUI InputMode;
	if (bStatsOpen)
	{
		InputMode.SetWidgetToFocus(StatWidget->TakeWidget());
	}
	else if (bInventoryOpen)
	{
		InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
	{
		ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
	}
}

// 첫 번째 퀵슬롯 사용
void APFPlayerController::UseQuickSlot1()
{
	UseQuickSlotByIndex(0);
}

// 두 번째 퀵슬롯 사용
void APFPlayerController::UseQuickSlot2()
{
	UseQuickSlotByIndex(1);
}

// 세 번째 퀵슬롯 사용
void APFPlayerController::UseQuickSlot3()
{
	UseQuickSlotByIndex(2);
}

// 네 번째 퀵슬롯 사용
void APFPlayerController::UseQuickSlot4()
{
	UseQuickSlotByIndex(3);
}

// 지정한 퀵슬롯 사용 요청
void APFPlayerController::UseQuickSlotByIndex(int32 QuickSlotIndex)
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const FString LevelName = FPackageName::GetShortName(World->GetMapName());
		if (LevelName.Contains(TEXT("Title")))
		{
			return;
		}
	}

	APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>();
	APFPlayer* PFPlayer = Cast<APFPlayer>(GetPawn());
	if (!PFPlayerState || !PFPlayer)
	{
		return;
	}

	PFPlayerState->UseQuickSlot(QuickSlotIndex, PFPlayer);
}
