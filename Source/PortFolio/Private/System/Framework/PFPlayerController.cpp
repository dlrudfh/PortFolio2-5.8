#include "System/Framework/PFPlayerController.h"

#include "Character/PFCharacter.h"
#include "Character/Kwang/PFKwang.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "Props/PFChest.h"
#include "Props/PFItem.h"
#include "UI/HUD/PFCharacterWidget.h"
#include "UI/HUD/PFCrosshairWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFGameMode.h"
#include "UI/Inventory/PFInventoryWidget.h"
#include "UI/HUD/PFStatWidget.h"
#include "UI/HUD/PFRespawnWidget.h"
#include "UI/Menu/PFMenuWidget.h"
#include "InputCoreTypes.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"

APFPlayerController::APFPlayerController()
{
	static ConstructorHelpers::FClassFinder<UUserWidget> SELFUI(TEXT("/Game/GameData/UI/SelfUI.SelfUI_C"));
	if (SELFUI.Succeeded())
	{
		SelfWidgetClass = SELFUI.Class;
	}
}

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
	GetWorldTimerManager().ClearTimer(PlayerRespawnTimerHandle);
	BindControlledCharacter();
	BindInventoryWidget();
	if (aPawn && GetPawn() == aPawn)
	{
		Client_StopRespawnCountdown();
	}
}

void APFPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SynchronizeUsername();
	BindControlledCharacter();
	BindInventoryWidget();
}

void APFPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PlayerRespawnTimerHandle);
	GetWorldTimerManager().ClearTimer(UsernameSyncTimerHandle);
	UnbindControlledCharacter();
	if (SelfHPBar) SelfHPBar->RemoveFromParent();
	if (CrosshairWidget) CrosshairWidget->RemoveFromParent();
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

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
	// 행동, 이동 입력 연결
	InputComponent->BindAction(TEXT("ViewChange"), EInputEvent::IE_Pressed, this, &APFPlayerController::ViewChange);
	InputComponent->BindAction(TEXT("Jump"), EInputEvent::IE_Pressed, this, &APFPlayerController::JumpStart);
	InputComponent->BindAction(TEXT("Jump"), EInputEvent::IE_Released, this, &APFPlayerController::JumpEnd);
	InputComponent->BindAction(TEXT("Attack"), EInputEvent::IE_Pressed, this, &APFPlayerController::AttackStart);
	InputComponent->BindAction(TEXT("Attack"), EInputEvent::IE_Released, this, &APFPlayerController::AttackEnd);
	InputComponent->BindAction(TEXT("Ultimate"), EInputEvent::IE_Pressed, this, &APFPlayerController::Ultimate);
	InputComponent->BindAction(TEXT("Sprint"), EInputEvent::IE_Pressed, this, &APFPlayerController::Sprint);
	InputComponent->BindAction(TEXT("ViewpointFix"), EInputEvent::IE_Pressed, this, &APFPlayerController::ViewpointFix);
	InputComponent->BindAction(TEXT("Interaction"), EInputEvent::IE_Pressed, this, &APFPlayerController::Interaction);
	InputComponent->BindAction(TEXT("ChangeCharacter"), EInputEvent::IE_Pressed, this, &APFPlayerController::ChangeCharacter);
	InputComponent->BindAction(TEXT("SpawnTestTwinblastEnemy"), EInputEvent::IE_Pressed, this, &APFPlayerController::SpawnTestTwinblastEnemy);
	InputComponent->BindAction(TEXT("SpawnTestKwangEnemy"), EInputEvent::IE_Pressed, this, &APFPlayerController::SpawnTestKwangEnemy);

	InputComponent->BindAxis(TEXT("UpDown"), this, &APFPlayerController::UpDown);
	InputComponent->BindAxis(TEXT("LeftRight"), this, &APFPlayerController::LeftRight);
	InputComponent->BindAxis(TEXT("LookUp"), this, &APFPlayerController::LookUp);
	InputComponent->BindAxis(TEXT("Turn"), this, &APFPlayerController::Turn);
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

	// 온라인 이름을 PlayerState에 동기화
	SynchronizeUsername();
	GetWorldTimerManager().SetTimer(UsernameSyncTimerHandle, this, &APFPlayerController::SynchronizeUsername, 1.f, true);

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
	Super::Tick(DeltaTime);
	BindControlledCharacter();
	if (IsLocalController())
	{
		GetViewportSize(CURRENTSCREENX, CURRENTSCREENY);
		BindCharacterHUD();
		UpdateCharacterControl();
	}
}

// 온라인 닉네임, 플레이어 이름 조회
FString APFPlayerController::GetUsername() const
{
	if (IsLocalController())
	{
		if (IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(STEAM_SUBSYSTEM))
		{
			const IOnlineIdentityPtr IdentityInterface = SteamSubsystem->GetIdentityInterface();
			if (IdentityInterface.IsValid() && IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn)
			{
				const FString SteamNickname = IdentityInterface->GetPlayerNickname(0);
				if (!SteamNickname.IsEmpty())
				{
					return SteamNickname.Left(128);
				}
			}
		}
	}

	if (const APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>())
	{
		FString PlayerName = PFPlayerState->GetPlayerName();
		PlayerName.TrimStartAndEndInline();
		if (!PlayerName.IsEmpty())
		{
			return PlayerName.Left(128);
		}
	}

	return TEXT("Offline Player");
}

// 로컬 이름 변경을 서버에 전달
void APFPlayerController::SynchronizeUsername()
{
	const APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>();
	if (!IsLocalController() || !PFPlayerState)
	{
		return;
	}

	const FString Username = GetUsername();
	if (PFPlayerState->GetPlayerName() != Username)
	{
		Server_SetUsername(Username);
	}
}

// 서버 플레이어 이름 변경, 전체 클라이언트 복제
void APFPlayerController::Server_SetUsername_Implementation(const FString& NewUsername)
{
	APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>();
	if (PFPlayerState && !NewUsername.IsEmpty() && NewUsername.Len() <= 128
		&& PFPlayerState->GetPlayerName() != NewUsername)
	{
		PFPlayerState->SetPlayerName(NewUsername);
	}
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

// 게임 메뉴 표시 전환
void APFPlayerController::OpenMenu()
{
	if (!IsLocalController() || !GetWorld()
		|| FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title")))
	{
		return;
	}
	if (MenuWidget && MenuWidget->IsInViewport())
	{
		MenuWidget->RemoveFromParent();
		UpdateWindowInputMode();
		return;
	}
	if (!MenuWidget)
	{
		MenuWidget = CreateWidget<UPFMenuWidget>(this, UPFMenuWidget::StaticClass());
	}
	if (!MenuWidget)
	{
		return;
	}

	// 다른 창, 누르고 있던 조작 입력 해제
	if (InventoryWidget) InventoryWidget->SetInventoryWindowVisible(false);
	if (StatWidget) StatWidget->SetStatWindowVisible(false);
	FlushPressedKeys();
	JumpEnd();
	AttackEnd();
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
	if (APFCharacter* ControlledPawn = GetControlledCharacter())
	{
		ControlledPawn->ConsumeMovementInputVector();
	}
	UpdateCharacterControl();
	MenuWidget->AddToPlayerScreen(etoi(PLAYERSTAT) + 10);
	UpdateWindowInputMode();
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
	if (MenuWidget && MenuWidget->IsInViewport())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
		{
			ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		}
		return;
	}
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
	APFCharacter* PFPlayer = GetControlledCharacter();
	if (!PFPlayerState || !PFPlayer)
	{
		return;
	}

	PFPlayerState->UseQuickSlot(QuickSlotIndex, PFPlayer);
}

// 화면 중앙의 조준 위치 계산
FVector APFPlayerController::CalculateAimPoint(bool* bOutCharacterTargeted) const
{
	if (bOutCharacterTargeted)
	{
		*bOutCharacterTargeted = false;
	}

	const APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!ControlledPawn)
	{
		return FVector::ZeroVector;
	}
	constexpr float MaxAimTraceDistance = 4000.f;
	FVector TraceStart = ControlledPawn->GetPawnViewLocation();
	FVector TraceDirection = ControlledPawn->GetBaseAimRotation().Vector();

	// 화면 중앙을 월드 조준선으로 변환
	if (const APlayerController* PlayerController = this)
	{
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		FVector ScreenCenterLocation;
		FVector ScreenCenterDirection;

		if (PlayerController->DeprojectScreenPositionToWorld(static_cast<float>(ViewportSizeX) * 0.5f,
			static_cast<float>(ViewportSizeY) * 0.5f, ScreenCenterLocation, ScreenCenterDirection))
		{
			TraceStart = ScreenCenterLocation;
			TraceDirection = ScreenCenterDirection;
		}
	}

	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * MaxAimTraceDistance;
	ECollisionChannel AimTraceChannel;
	if (!GetCollisionChannel(PFCollisionChannelNames::AimTrace, AimTraceChannel))
	{
		return TraceEnd;
	}

	// 자신, 부착 액터를 조준 검사에서 제외
	FCollisionQueryParams AimTraceParams(SCENE_QUERY_STAT(TwinBlastAimTrace), false, ControlledPawn);
	TArray<AActor*> AttachedActors;
	ControlledPawn->GetAttachedActors(AttachedActors);
	AimTraceParams.AddIgnoredActors(AttachedActors);
	AimTraceParams.AddIgnoredActor(ControlledPawn);

	// 조준선 충돌 위치 반환
	FHitResult AimHit;
	if (UWorld* World = GetWorld();
		World && World->LineTraceSingleByChannel(AimHit, TraceStart, TraceEnd, AimTraceChannel, AimTraceParams))
	{
		if (bOutCharacterTargeted)
		{
			*bOutCharacterTargeted = IsValid(Cast<APFCharacter>(AimHit.GetActor()));
		}
		return AimHit.ImpactPoint;
	}

	return TraceEnd;
}

// 조종 캐릭터 조회
APFCharacter* APFPlayerController::GetControlledCharacter() const
{
	return Cast<APFCharacter>(GetPawn());
}

// 요청 대상과 현재 조종 권한 확인
bool APFPlayerController::IsCurrentCharacter(const APFCharacter* ControlledPawn) const
{
	return IsValid(ControlledPawn) && ControlledPawn == GetPawn() && ControlledPawn->IsPlayerCharacter()
		&& ControlledPawn->GetController() == this && !ControlledPawn->IsDeadCharacter();
}

void APFPlayerController::OnUnPossess()
{
	UnbindControlledCharacter();
	Super::OnUnPossess();
}

void APFPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	BindControlledCharacter();
}

void APFPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	BindControlledCharacter();
}

// 캐릭터 수명주기 연결
void APFPlayerController::BindControlledCharacter()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (ControlledCharacter.Get() != ControlledPawn)
	{
		UnbindControlledCharacter();
		ControlledCharacter = ControlledPawn;
		if (ControlledPawn)
		{
			ControlledPawn->OnCharacterReady.AddUObject(this, &APFPlayerController::HandleCharacterReady);
			ControlledPawn->OnCharacterDied.AddUObject(this, &APFPlayerController::HandleCharacterDied);
			ControlledPawn->OnCharacterLanded.AddUObject(this, &APFPlayerController::HandleCharacterLanded);
			AimPoint = ControlledPawn->GetPawnViewLocation() + ControlledPawn->GetBaseAimRotation().Vector() * 4000.f;
			if (IsLocalController() && PlayerCameraManager)
			{
				PlayerCameraManager->ViewPitchMin = -60.f;
				PlayerCameraManager->ViewPitchMax = 60.f;
			}
			BindCharacterHUD();
			RefreshPawnOverlaps();
		}
	}
	if (bPendingViewRestore && ControlledPawn && PendingViewCharacter.Get() == ControlledPawn && ControlledPawn->IsPlayerCharacter())
	{
		ControlledPawn->ApplyViewState(SavedViewState);
		bPendingViewRestore = false;
		PendingViewCharacter.Reset();
	}
}

// 이전 캐릭터 연결, 입력 해제
void APFPlayerController::UnbindControlledCharacter()
{
	if (APFCharacter* ControlledPawn = ControlledCharacter.Get())
	{
		ControlledPawn->ClearControlCommands();
		ControlledPawn->OnCharacterReady.RemoveAll(this);
		ControlledPawn->OnCharacterDied.RemoveAll(this);
		ControlledPawn->OnCharacterLanded.RemoveAll(this);
	}
	ControlledCharacter.Reset();
	NearestChest.Reset();
	JumpButtonHeld = false;
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
	LastMovementDirection = IDLE;
	HUDAttributeSet.Reset();
	if (SelfHPBar)
	{
		SelfHPBar->BindAttributeSet(nullptr, true);
		SelfHPBar->SetVisibility(ESlateVisibility::Hidden);
	}
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

// ASC 준비 후 HUD 연결
void APFPlayerController::HandleCharacterReady(APFCharacter* ControlledPawn)
{
	if (ControlledPawn == GetControlledCharacter())
	{
		BindCharacterHUD();
	}
}

// 사망 입력 해제, 부활 예약
void APFPlayerController::HandleCharacterDied(APFCharacter* ControlledPawn)
{
	if (!HasAuthority() || ControlledPawn != GetControlledCharacter())
	{
		return;
	}
	JumpButtonHeld = false;
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
	if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		constexpr float RespawnDelay = 5.f;
		GetWorldTimerManager().SetTimer(PlayerRespawnTimerHandle,
			FTimerDelegate::CreateUObject(GameMode, &APFGameMode::RespawnPlayer,
				TWeakObjectPtr<APlayerController>(this)), RespawnDelay, false);
		Client_StartRespawnCountdown(GetWorld()->GetTimeSeconds() + RespawnDelay, RespawnDelay);
	}
}

// 점프 입력 유지 시 재점프
void APFPlayerController::HandleCharacterLanded(APFCharacter* ControlledPawn)
{
	if (IsLocalController() && JumpButtonHeld && IsCurrentCharacter(ControlledPawn))
	{
		ControlledPawn->ActivateJumpAbility();
	}
}

// 로컬 HUD, 조준점 연결
void APFPlayerController::BindCharacterHUD()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!IsLocalController() || !ControlledPawn || !ControlledPawn->IsLocalPlayerCharacter() || !ControlledPawn->GetAttributeSet())
	{
		return;
	}
	if (!SelfHPBar && SelfWidgetClass)
	{
		SelfHPBar = CreateWidget<UPFCharacterWidget>(this, SelfWidgetClass);
		if (SelfHPBar)
		{
			SelfHPBar->BindAttributeSet(ControlledPawn->GetAttributeSet(), true);
			HUDAttributeSet = ControlledPawn->GetAttributeSet();
			SelfHPBar->SetPositionInViewport(FVector2D::ZeroVector);
			SelfHPBar->AddToPlayerScreen(etoi(PLAYERSTAT));
		}
	}
	if (SelfHPBar && HUDAttributeSet.Get() != ControlledPawn->GetAttributeSet())
	{
		SelfHPBar->BindAttributeSet(ControlledPawn->GetAttributeSet(), true);
		HUDAttributeSet = ControlledPawn->GetAttributeSet();
	}
	if (!CrosshairWidget && ControlledPawn->HasCrosshair())
	{
		CrosshairWidget = CreateWidget<UPFCrosshairWidget>(this, UPFCrosshairWidget::StaticClass());
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToPlayerScreen(etoi(PLAYERSTAT) - 1);
		}
	}
	const bool bInTitle = FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title"));
	if (SelfHPBar)
	{
		SelfHPBar->SetVisibility(bInTitle ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}
	if (CrosshairWidget)
	{
		const bool bVisible = !bInTitle && ControlledPawn->HasCrosshair() && ControlledPawn->GetCurrentControlMode() != TOPVIEW;
		CrosshairWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

// 로컬 이동 방향, 공격 조준 갱신
void APFPlayerController::UpdateCharacterControl()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!IsCurrentCharacter(ControlledPawn))
	{
		return;
	}
	const EPFDirection Direction = eAdd(UpDownDir, LeftRightDir);
	ControlledPawn->SetMovementInputDirection(Direction);
	if (Direction != LastMovementDirection)
	{
		LastMovementDirection = Direction;
		if (!HasAuthority())
		{
			Server_SetDir(ControlledPawn, Direction);
		}
	}
	if (ControlledPawn->HasCrosshair())
	{
		bool bCharacterTargeted = false;
		const FVector CurrentAim = CalculateAimPoint(&bCharacterTargeted);
		if (CrosshairWidget) CrosshairWidget->SetCharacterTargeted(bCharacterTargeted);
		if (ControlledPawn->IsAttackCommandActive())
		{
			Server_UpdateAimPoint(ControlledPawn, CurrentAim);
		}
	}
}

// 점프 입력 시작
void APFPlayerController::JumpStart()
{
	JumpButtonHeld = true;
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); IsCurrentCharacter(ControlledPawn))
	{
		ControlledPawn->ActivateJumpAbility();
	}
}

// 점프 입력 해제
void APFPlayerController::JumpEnd()
{
	JumpButtonHeld = false;
	if (APFCharacter* ControlledPawn = GetControlledCharacter()) ControlledPawn->StopJumping();
}

// 전후 이동 입력
void APFPlayerController::UpDown(float Value)
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!IsCurrentCharacter(ControlledPawn) || ControlledPawn->IsMovementBlocked())
	{
		UpDownDir = IDLE;
		return;
	}
	UpDownDir = Value > 0.f ? FWD : (Value < 0.f ? BWD : IDLE);
	ControlledPawn->AddMovementInput(FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X), Value);
}

// 좌우 이동 입력
void APFPlayerController::LeftRight(float Value)
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!IsCurrentCharacter(ControlledPawn) || ControlledPawn->IsMovementBlocked())
	{
		LeftRightDir = IDLE;
		return;
	}
	LeftRightDir = Value > 0.f ? RIGHT : (Value < 0.f ? LEFT : IDLE);
	ControlledPawn->AddMovementInput(FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y), Value);
}

// 시선 상하 회전
void APFPlayerController::LookUp(float Value)
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (IsCurrentCharacter(ControlledPawn) && ControlledPawn->GetCurrentControlMode() != TOPVIEW)
	{
		AddPitchInput(Value * ControlledPawn->GetLookUpSpeed());
	}
}

// 시선 좌우 회전
void APFPlayerController::Turn(float Value)
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); IsCurrentCharacter(ControlledPawn))
	{
		AddYawInput(Value * ControlledPawn->GetTurnSpeed());
	}
}

// 공격 입력 시작
void APFPlayerController::AttackStart()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); IsCurrentCharacter(ControlledPawn))
	{
		Server_UpdateAimPoint(ControlledPawn, CalculateAimPoint());
		ControlledPawn->SetAttackInputPressed(true);
	}
}

// 공격 입력 해제
void APFPlayerController::AttackEnd()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter()) ControlledPawn->SetAttackInputPressed(false);
}

// 궁극기 입력
void APFPlayerController::Ultimate()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); IsCurrentCharacter(ControlledPawn))
	{
		ControlledPawn->ActivateUltimateAbility();
	}
}

// 질주 입력
void APFPlayerController::Sprint()
{
	Server_Sprint(GetControlledCharacter());
}

// 시점 고정 입력
void APFPlayerController::ViewpointFix()
{
	Server_ViewpointFix(GetControlledCharacter());
}

// 카메라 시점 전환
void APFPlayerController::ViewChange()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!IsCurrentCharacter(ControlledPawn)) return;
	switch (ControlledPawn->GetCurrentControlMode())
	{
	case TOPVIEW:
		SetControlRotation(ControlledPawn->GetCameraSpringArm()->GetRelativeRotation());
		Server_SetControlMode(ControlledPawn, TPS);
		break;
	case TPS:
		SetControlRotation(ControlledPawn->GetActorRotation());
		Server_SetControlMode(ControlledPawn, FPS);
		break;
	case FPS:
		SetControlRotation(ControlledPawn->GetActorRotation());
		Server_SetControlMode(ControlledPawn, TOPVIEW);
		break;
	default:
		break;
	}
}

// 서버 이동 입력 반영
void APFPlayerController::Server_SetDir_Implementation(APFCharacter* ControlledPawn, EPFDirection NewDirection)
{
	if (IsCurrentCharacter(ControlledPawn)) ControlledPawn->SetMovementInputDirection(NewDirection);
}

// 서버 카메라 모드 반영
void APFPlayerController::Server_SetControlMode_Implementation(APFCharacter* ControlledPawn, ECONTROLMODE NewControlMode)
{
	if (IsCurrentCharacter(ControlledPawn)) ControlledPawn->SetControlMode(NewControlMode);
}

// 서버 질주 전환
void APFPlayerController::Server_Sprint_Implementation(APFCharacter* ControlledPawn)
{
	if (IsCurrentCharacter(ControlledPawn)) ControlledPawn->ToggleSprint();
}

// 서버 시점 고정 전환
void APFPlayerController::Server_ViewpointFix_Implementation(APFCharacter* ControlledPawn)
{
	if (IsCurrentCharacter(ControlledPawn)) ControlledPawn->ToggleViewpointFixed();
}

// 서버 조준점 반영
void APFPlayerController::Server_UpdateAimPoint_Implementation(APFCharacter* ControlledPawn, FVector NewAimPoint)
{
	if (IsCurrentCharacter(ControlledPawn) && !NewAimPoint.ContainsNaN())
	{
		AimPoint = NewAimPoint;
	}
}

bool APFPlayerController::TryGetCombatAim(FVector& OutAimPoint)
{
	if (!IsCurrentCharacter(GetControlledCharacter())) return false;
	OutAimPoint = AimPoint;
	return true;
}

// 상자 접근 대상 갱신
void APFPlayerController::SetChest(APFChest* Chest)
{
	if (!Chest && NearestChest.IsValid() && IsValid(NearestChest->Trigger)
		&& NearestChest->Trigger->IsOverlappingActor(GetPawn()))
	{
		return;
	}
	NearestChest = Chest;
}

// 상호작용 요청
void APFPlayerController::Interaction()
{
	Server_Interaction(GetControlledCharacter());
}

// 서버 상자 열기
void APFPlayerController::Server_Interaction_Implementation(APFCharacter* ControlledPawn)
{
	if (IsCurrentCharacter(ControlledPawn) && NearestChest.IsValid() && IsValid(NearestChest->Trigger)
		&& NearestChest->Trigger->IsOverlappingActor(ControlledPawn))
	{
		NearestChest->ChestOpen();
	}
}

// 캐릭터 교체 요청
void APFPlayerController::ChangeCharacter()
{
	Server_ChangeCharacter(GetControlledCharacter());
}

// 서버 캐릭터 교체
void APFPlayerController::Server_ChangeCharacter_Implementation(APFCharacter* ControlledPawn)
{
	if (!IsCurrentCharacter(ControlledPawn) || ControlledPawn->GetCharacterMovement()->IsFalling() || ControlledPawn->HasAirborneTag()
		|| FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title")))
	{
		return;
	}
	if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GameMode->ChangeCharacter(this);
	}
}

// 트윈블라스트 적 생성 요청
void APFPlayerController::SpawnTestTwinblastEnemy()
{
	Server_SpawnTestEnemy(GetControlledCharacter(), true);
}

// 광 적 생성 요청
void APFPlayerController::SpawnTestKwangEnemy()
{
	Server_SpawnTestEnemy(GetControlledCharacter(), false);
}

// 교체 전 카메라 상태 보관
void APFPlayerController::CaptureCharacterState()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter())
	{
		SavedViewState = ControlledPawn->CaptureViewState();
		bHasSavedViewState = true;
	}
}

// 교체 후 서버, 소유 클라이언트 상태 복원
void APFPlayerController::RestoreCharacterState()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); HasAuthority() && ControlledPawn && bHasSavedViewState)
	{
		ControlledPawn->ApplyViewState(SavedViewState);
		Client_RestoreCharacterState(ControlledPawn, SavedViewState);
	}
}

// 새 Pawn 준비 후 카메라 복원
void APFPlayerController::Client_RestoreCharacterState_Implementation(APFCharacter* ControlledPawn, FPFCharacterSharedStateSnapshot Snapshot)
{
	SavedViewState = Snapshot;
	bHasSavedViewState = true;
	PendingViewCharacter = ControlledPawn;
	bPendingViewRestore = true;
	BindControlledCharacter();
}

// NavMesh 위에 AI 캐릭터 생성
void APFPlayerController::Server_SpawnTestEnemy_Implementation(APFCharacter* ControlledPawn, bool bSpawnTwinblast)
{
	UWorld* World = GetWorld();
	if (!World || !IsCurrentCharacter(ControlledPawn))
	{
		return;
	}

	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		return;
	}

	// 적 종류, 생성 조건 설정
	UClass* EnemyClass = bSpawnTwinblast
		? APFTwinBlast::StaticClass()
		: APFKwang::StaticClass();
	const APFCharacter* EnemyDefaultObject = EnemyClass ? EnemyClass->GetDefaultObject<APFCharacter>() : nullptr;
	const UCapsuleComponent* EnemyCapsule = EnemyDefaultObject ? EnemyDefaultObject->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* EnemyMovement = EnemyDefaultObject ? EnemyDefaultObject->GetCharacterMovement() : nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!EnemyCapsule || !EnemyMovement || !Navigation)
	{
		PFLOG(Warning, TEXT("Test enemy spawn failed: missing capsule, movement or navigation system"));
		return;
	}

	// 스폰 전후 발밑의 NavMesh 확인
	const auto IsOnSpawnNavMesh = [Navigation](const FVector& Feet,
		const UCharacterMovementComponent* Movement, const UCapsuleComponent* Capsule)
	{
		if (!Movement || !Capsule)
		{
			return false;
		}
		FNavAgentProperties AgentProperties = Movement->GetNavAgentPropertiesRef();
		if (Movement->ShouldUpdateNavAgentWithOwnersCollision())
		{
			AgentProperties.AgentRadius = Capsule->GetScaledCapsuleRadius();
			AgentProperties.AgentHeight = Capsule->GetScaledCapsuleHalfHeight() * 2.f;
		}
		const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(Navigation->GetNavDataForProps(AgentProperties, Feet));
		constexpr float HorizontalTolerance = 1.f;
		const float HeightRange = Movement->MaxStepHeight + 6.f;
		FNavLocation NavLocation;
		return NavMesh && Navigation->ProjectPointToNavigation(Feet, NavLocation,
			FVector(HorizontalTolerance, HorizontalTolerance, HeightRange), NavMesh)
			&& FVector::DistSquared2D(Feet, NavLocation.Location) <= FMath::Square(HorizontalTolerance)
			&& FMath::Abs(NavLocation.Location.Z - Feet.Z) <= HeightRange;
	};

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TestEnemySpawn), false, ControlledPawn);
	QueryParams.AddIgnoredActor(ControlledPawn);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = ControlledPawn;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	// 배치 가능한 위치를 찾아 생성
	int32 RemainingAttempts = 30;
	FTransform SpawnTransform;
	while (APFGameMode::FindSpawnTransform(World, EnemyCapsule, QueryParams, RemainingAttempts, false, SpawnTransform))
	{
		const FVector CandidateFeet = SpawnTransform.GetLocation() - FVector(0.f, 0.f, EnemyCapsule->GetScaledCapsuleHalfHeight());
		if (!IsOnSpawnNavMesh(CandidateFeet, EnemyMovement, EnemyCapsule))
		{
			continue;
		}
		if (APFCharacter* SpawnedEnemy = World->SpawnActor<APFCharacter>(
			EnemyClass, SpawnTransform.GetLocation(), SpawnTransform.Rotator(), SpawnParameters))
		{
			// 충돌 보정된 실제 위치 확인
			const UCharacterMovementComponent* SpawnedMovement = SpawnedEnemy->GetCharacterMovement();
			if (!SpawnedMovement || !IsOnSpawnNavMesh(SpawnedMovement->GetActorFeetLocation(),
				SpawnedMovement, SpawnedEnemy->GetCapsuleComponent()))
			{
				SpawnedEnemy->Destroy();
				continue;
			}
			SpawnedEnemy->SpawnDefaultController();
			if (!SpawnedEnemy->GetController())
			{
				SpawnedEnemy->Destroy();
				return;
			}
			PFLOG(Warning, TEXT("Test enemy spawned: %s"), *SpawnedEnemy->GetClass()->GetName());
			return;
		}
	}

	PFLOG(Warning, TEXT("Test enemy spawn failed: no spawnable NavMesh ground found inside SM_Cube bounds"));
}

// 빙의 전에 발생한 접촉 상태 반영
void APFPlayerController::RefreshPawnOverlaps()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!HasAuthority() || !IsCurrentCharacter(ControlledPawn))
	{
		return;
	}
	TArray<AActor*> OverlappingActors;
	ControlledPawn->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		if (APFChest* Chest = Cast<APFChest>(Actor); IsValid(Chest) && IsValid(Chest->Trigger)
			&& Chest->Trigger->IsOverlappingActor(ControlledPawn))
		{
			SetChest(Chest);
		}
		else if (APFItem* Item = Cast<APFItem>(Actor); IsValid(Item))
		{
			Item->UseItem(ControlledPawn);
		}
	}
}
