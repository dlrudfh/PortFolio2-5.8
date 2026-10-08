#include "System/Framework/PFPlayerController.h"
#include "Campaign/PFCampaignDirector.h"
#include "Campaign/PFCampaignAnchor.h"
#include "Campaign/PFCampaignWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

#include "Character/PFCharacter.h"
#include "Character/Kwang/PFKwang.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "Props/PFChest.h"
#include "Props/PFItem.h"
#include "UI/PFGameCursor.h"
#include "UI/HUD/PFCharacterWidget.h"
#include "UI/HUD/PFCrosshairWidget.h"
#include "UI/HUD/PFMinimapWidget.h"
#include "UI/HUD/PFTutorialWidget.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StreamableRenderAsset.h"
#include "ContentStreaming.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "System/Framework/PFTutorialManager.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFGameMode.h"
#include "System/Framework/PFSessionGameState.h"
#include "UI/Inventory/PFInventoryWidget.h"
#include "UI/Inventory/PFShopWidget.h"
#include "UI/HUD/PFStatWidget.h"
#include "UI/HUD/PFRespawnWidget.h"
#include "UI/Menu/PFMenuWidget.h"
#include "InputCoreTypes.h"
#include "Misc/PackageName.h"

APFPlayerController::APFPlayerController()
{
	static ConstructorHelpers::FClassFinder<UUserWidget> SELFUI(TEXT("/Game/GameData/UI/SelfUI.SelfUI_C"));
	if (SELFUI.Succeeded())
	{
		SelfWidgetClass = SELFUI.Class;
	}
	static ConstructorHelpers::FClassFinder<UPFInventoryWidget> INVENTORYUI(TEXT("/Game/GameData/UI/Inventory"));
	if (INVENTORYUI.Succeeded())
	{
		InventoryWidgetClass = INVENTORYUI.Class;
	}
	static ConstructorHelpers::FClassFinder<UPFStatWidget> STATUI(TEXT("/Game/GameData/UI/Stats"));
	if (STATUI.Succeeded())
	{
		StatWidgetClass = STATUI.Class;
	}
	static ConstructorHelpers::FClassFinder<UPFMenuWidget> MENUUI(TEXT("/Game/GameData/UI/Menu"));
	if (MENUUI.Succeeded())
	{
		MenuWidgetClass = MENUUI.Class;
	}
	static ConstructorHelpers::FClassFinder<UPFRespawnWidget> RESPAWNUI(TEXT("/Game/GameData/UI/Respawn"));
	if (RESPAWNUI.Succeeded())
	{
		RespawnWidgetClass = RESPAWNUI.Class;
	}
}

void APFPlayerController::OnPossess(APawn* aPawn)
{
	PFLOG_W;
	Super::OnPossess(aPawn);
	GetWorldTimerManager().ClearTimer(PlayerRespawnTimerHandle);
	BindControlledCharacter();
	BindInventoryWidget();
	if (StatWidget) StatWidget->BindPlayerState(GetPlayerState<APFPlayerState>());
	if (aPawn && GetPawn() == aPawn)
	{
		Client_StopRespawnCountdown();
		if (APFCharacter* PossessedCharacter = Cast<APFCharacter>(aPawn); PossessedCharacter && PossessedCharacter->IsDeadCharacter())
		{
			HandleCharacterDied(PossessedCharacter);
		}
	}
}

void APFPlayerController::SeamlessTravelFrom(APlayerController* OldPC)
{
	Super::SeamlessTravelFrom(OldPC);

	// 이동 전 선택 캐릭터로 최초 생성 준비
	if (APFPlayerState* State = GetPlayerState<APFPlayerState>())
	{
		InitialSpawnCharacter = State->GetCharacter();
		bInitialSpawnRequested = true;
	}
	if (const APFPlayerController* OldController = Cast<APFPlayerController>(OldPC))
	{
		SavedViewState = OldController->SavedViewState;
		bHasSavedViewState = OldController->bHasSavedViewState;
	}
}

void APFPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SynchronizeUsername();
	BindControlledCharacter();
	BindCharacterHUD();
	BindInventoryWidget();
	if (StatWidget) StatWidget->BindPlayerState(GetPlayerState<APFPlayerState>());
}

void APFPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bMapFadePending = false;
	MapFadeInRemaining = 0.f;
	ReleaseScreenFade();
	ReleaseCampaignPresentation();
	if (MapLoadingCamera) MapLoadingCamera->Destroy();
	MapLoadingCamera = nullptr;
	MapRenderAssets.Reset();
	MapPrimitives.Reset();
	GetWorldTimerManager().ClearTimer(PlayerRespawnTimerHandle);
	GetWorldTimerManager().ClearTimer(UsernameSyncTimerHandle);
	UnbindControlledCharacter();
	if (SelfHPBar) SelfHPBar->RemoveFromParent();
	if (CrosshairWidget) CrosshairWidget->RemoveFromParent();
	if (MinimapWidget) MinimapWidget->RemoveFromParent();
	if (ShopWidget)
	{
		ShopWidget->RemoveFromParent();
		ShopWidget = nullptr;
	}
	if (TutorialWidget)
	{
		TutorialWidget->RemoveFromParent();
		TutorialWidget = nullptr;
	}
	TutorialManager.Reset();
	if (StatWidget)
	{
		StatWidget->RemoveFromParent();
		StatWidget = nullptr;
	}
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
	if (SelectedCharacter != CHARACTER_TWINBLAST && SelectedCharacter != CHARACTER_KWANG) return;
	if (GetPawn() || bInitialSpawnComplete) return;
	if (const APFCharacter* ControlledPawn = GetControlledCharacter(); ControlledPawn && ControlledPawn->IsJumpPadFlightActive()) return;
	if (HasAuthority())
	{
		if (APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>())
		{
			if (PFPlayerState->HasLobbySelection()) return;
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
	APFGameMode* GameMode = World->GetAuthGameMode<APFGameMode>();
	if (GameMode)
	{
		bMapPresentationReady = true;
		GameMode->RequestInitialSpawn(this, SelectedCharacter);
	}
}

// 서버 캐릭터 선택 반영
void APFPlayerController::Server_SetCharacter_Implementation(ECHARACTER SelectedCharacter)
{
	if (GetPawn() && APFCampaignDirector::BlocksInput(this)) return;
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
		if (!RespawnWidgetClass)
		{
			PFLOG(Warning, TEXT("Respawn widget class unavailable"));
			return;
		}
		RespawnWidget = CreateWidget<UPFRespawnWidget>(this, RespawnWidgetClass);
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
	InputComponent->BindKey(EKeys::B, EInputEvent::IE_Pressed, this, &APFPlayerController::ToggleShop);

	const FKey QuickSlotKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(QuickSlotKeys); ++Index)
	{
		FInputKeyBinding Binding(FInputChord(QuickSlotKeys[Index]), EInputEvent::IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &APFPlayerController::UseQuickSlotByIndex, Index);
		InputComponent->KeyBindings.Add(MoveTemp(Binding));
	}
	InputComponent->BindKey(EKeys::M, EInputEvent::IE_Pressed, this, &APFPlayerController::ChangeTestMap);
	InputComponent->BindKey(EKeys::Enter, EInputEvent::IE_Pressed, this, &APFPlayerController::SkipCampaignCamera);
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

	FPFGameCursor::Install(GetWorld()->GetGameViewport());

	// 온라인 이름을 PlayerState에 동기화
	SynchronizeUsername();
	GetWorldTimerManager().SetTimer(UsernameSyncTimerHandle, this, &APFPlayerController::SynchronizeUsername, 1.f, true);

	UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
	if (!GI)
	{
		return;
	}
	GI->ApplyMenuVolume();
	GI->CreateTitle();

	UWorld* World = GetWorld();

	FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		SetCharacter(GI->GetCharacterType());
	}
	else
	{
		BeginMapPresentation();
		CreateUI();
	}
}

void APFPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (HasAuthority())
	{
		CampaignGuideRemaining -= DeltaTime;
		if (CampaignGuideRemaining <= 0.f)
		{
			CampaignGuideRemaining = .5f;
			APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld());
			bHasCampaignGuide = Director && GetPawn() && !bCampaignWaiting
				&& Director->GetObjectiveLocation(this, CampaignGuide);
		}
	}
	if (IsLocalController())
	{
		UpdateCampaignPresentation();
		UpdateMapPresentation(DeltaTime);
		UpdateScreenFade(DeltaTime);
		UpdateUIAvailability();
		UpdateCrosshairVisibility();
		UpdateCharacterControl();
	}
}

// 맵 진입 화면을 검게 유지
void APFPlayerController::BeginMapPresentation()
{
	bMapFadePending = true;
	bMapPresentationReady = false;
	bMapStreamingPrimed = false;
	MapReadyTime = 0.f;
	MapFadeInRemaining = 0.f;
	MapRenderAssets.Reset();
	MapPrimitives.Reset();
	UpdateScreenFade(0.f);
}

// 서버가 지정한 맵 진입 방식 반영
void APFPlayerController::Client_PrepareMapPresentation_Implementation(bool bCampaign)
{
	if (IsLocalController())
	{
		ReleaseCampaignPresentation();
		BeginMapPresentation();
	}
	bMapUsesCampaign = bCampaign;
	bMapRoleConfirmed = true;
}

// 진입 연출, 시작 지점의 화면을 스트리밍 기준으로 준비
bool APFPlayerController::PrepareMapEntryView()
{
	if (!bMapRoleConfirmed) return false;
	const APFSessionGameState* Session = APFSessionGameState::Get(GetWorld());
	if (!Session || !Session->bInitialized || Session->Phase != EPFSessionPhase::Playing) return false;
	bMapUsesCampaign = APFSessionGameState::IsStory(GetWorld());
	if (GetPawn()) return true;
	UWorld* World = GetWorld();
	FVector Location;
	FRotator Rotation;
	float FieldOfView = 90.f;
	if (bMapUsesCampaign)
	{
		const APFCampaignDirector* Director = APFCampaignDirector::Find(World);
		if (!Director || !Director->Definition) return false;
		const UPFCampaignDefinition* Definition = Director->Definition;
		FName ShotId = Director->Progress.Step == 0 ? Definition->IntroShot : NAME_None;
		if (Definition->Steps.IsValidIndex(Director->Progress.Step - 1))
			ShotId = Definition->Steps[Director->Progress.Step - 1].Shot;
		const APFCampaignShot* EntryShot = nullptr;
		for (TActorIterator<APFCampaignShot> It(World); !ShotId.IsNone() && It; ++It)
			if (It->Id == ShotId) { EntryShot = *It; break; }
		if (EntryShot)
		{
			EntryShot->Evaluate(0.f, Location, Rotation);
			FieldOfView = EntryShot->FieldOfView;
		}
		else
		{
			FName Checkpoint = Definition->StartCheckpoint;
			for (int32 Index = 0; Index < Director->Progress.Step && Definition->Steps.IsValidIndex(Index); ++Index)
				if (Definition->Steps[Index].bCheckpoint) Checkpoint = Definition->Steps[Index].Checkpoint;
			const APFCampaignAnchor* Start = Director->Anchor(FName(FString::Printf(TEXT("%s_Spawn_01"), *Checkpoint.ToString())));
			if (!Start) return false;
			Location = Start->GetActorLocation() + FVector(0.f, 0.f, 160.f);
			Rotation = Start->GetActorRotation();
		}
	}
	else if (APFSessionGameState::IsTraining(World))
	{
		Location = FVector(1544.f, 975.f, 165.f);
		Rotation = FRotator(0.f, -90.f, 0.f);
	}
	else
	{
		return true;
	}
	if (!MapLoadingCamera) MapLoadingCamera = World->SpawnActor<ACameraActor>();
	if (!MapLoadingCamera) return false;
	if (!MapLoadingCamera->GetActorLocation().Equals(Location, 1.f)
		|| !MapLoadingCamera->GetActorRotation().Equals(Rotation, .1f))
	{
		bMapStreamingPrimed = false;
		MapReadyTime = 0.f;
	}
	MapLoadingCamera->SetActorLocationAndRotation(Location, Rotation);
	MapLoadingCamera->GetCameraComponent()->SetFieldOfView(FieldOfView);
	SetViewTarget(MapLoadingCamera);
	return true;
}

// 맵, 렌더링 리소스 준비 후 진입 페이드 해제
void APFPlayerController::UpdateMapPresentation(float DeltaTime)
{
	if (!bMapFadePending || !PlayerCameraManager) return;
	if (!bMapPresentationReady && !PrepareMapEntryView()) return;
	UWorld* World = GetWorld();
	bool bLoading = IsAsyncLoading() || IStreamingManager::Get().GetNumWantingResources() > 0;
	if (const UWorldPartitionSubsystem* Partition = World->GetSubsystem<UWorldPartitionSubsystem>())
		bLoading |= !Partition->IsStreamingCompleted();
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
		if (Level && Level->IsStreamingStatePending()) bLoading = true;
#if WITH_EDITOR
	bLoading |= FAssetCompilingManager::Get().GetNumRemainingAssets() > 0
		|| (GShaderCompilingManager && GShaderCompilingManager->IsCompiling());
#endif
	if (bLoading)
	{
		bMapStreamingPrimed = false;
		MapReadyTime = 0.f;
		return;
	}
	if (!bMapStreamingPrimed)
	{
		int32 Width = 0, Height = 0;
		GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0) return;
		FVector Location;
		FRotator Rotation;
		GetPlayerViewPoint(Location, Rotation);
		float FieldOfView = PlayerCameraManager->GetFOVAngle();
		if (MapLoadingCamera && !bMapPresentationReady)
		{
			Location = MapLoadingCamera->GetActorLocation();
			FieldOfView = MapLoadingCamera->GetCameraComponent()->FieldOfView;
		}
		const float ScreenSize = static_cast<float>(Width);
		const float FOVScreenSize = ScreenSize / FMath::Tan(FMath::DegreesToRadians(FieldOfView * .5f));
		IStreamingManager::Get().AddViewInformation(Location, ScreenSize, FOVScreenSize, 1.f, false, 0.f, nullptr, World);
		IStreamingManager::Get().UpdateResourceStreaming(0.f, true);
		MapRenderAssets.Reset();
		for (TObjectIterator<UStreamableRenderAsset> It; It; ++It) MapRenderAssets.Add(*It);
		MapPrimitives.Reset();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
			for (UPrimitiveComponent* Primitive : Primitives) MapPrimitives.Add(Primitive);
		}
		bMapStreamingPrimed = true;
		MapReadyTime = 0.f;
		return;
	}
	for (const TWeakObjectPtr<UStreamableRenderAsset>& Asset : MapRenderAssets)
		if (Asset.IsValid() && (Asset->HasPendingInitOrStreaming() || Asset->bHasStreamingUpdatePending))
		{
			MapReadyTime = 0.f;
			return;
		}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : MapPrimitives)
		if (Primitive.IsValid() && (Primitive->IsCompiling() || Primitive->IsPSOPrecaching()))
		{
			MapReadyTime = 0.f;
			return;
		}
	MapReadyTime += DeltaTime;
	if (!bMapPresentationReady)
	{
		if (MapReadyTime < .5f) return;
		UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
		if (!GI) return;
		const APFPlayerState* State = GetPlayerState<APFPlayerState>();
		if (!State) return;
		bMapPresentationReady = true;
		Server_RequestInitialSpawn(State->HasLobbySelection() ? State->GetCharacter() : GI->GetCharacterType());
		return;
	}
	if (bMapUsesCampaign)
	{
		const APFCampaignDirector* Director = APFCampaignDirector::Find(World);
		if (!Director || Director->Progress.Phase == EPFCampaignPhase::Preparing) return;
		if (Director->Progress.Phase == EPFCampaignPhase::Camera && !bCampaignCameraActive) return;
		if (Director->Progress.Phase == EPFCampaignPhase::Starting && !GetPawn()) return;
	}
	else if (!GetPawn()) return;
	if (MapLoadingCamera && GetViewTarget() == MapLoadingCamera) return;
	bMapFadePending = false;
	MapFadeInRemaining = .35f;
	if (MapLoadingCamera) MapLoadingCamera->Destroy();
	MapLoadingCamera = nullptr;
	MapRenderAssets.Reset();
	MapPrimitives.Reset();
}

// HUD, 안내, 디버그 표시 위에 화면 전체 페이드 적용
void APFPlayerController::UpdateScreenFade(float DeltaTime)
{
	if (bMapFadePending) ScreenFadeOpacity = 1.f;
	else if (MapFadeInRemaining > 0.f)
	{
		MapFadeInRemaining = FMath::Max(0.f, MapFadeInRemaining - DeltaTime);
		ScreenFadeOpacity = MapFadeInRemaining / .35f;
	}
	if (!ScreenFadeWidget && ScreenFadeOpacity > 0.f)
	{
		UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
		if (!Viewport || !Viewport->GetGameLayerManager().IsValid()) return;
		ScreenFadeWidget = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor::Black)
			.Padding(0.f)
			.Cursor(EMouseCursor::None)
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			.OnMouseButtonUp_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); });
		Viewport->AddGameLayerWidget(ScreenFadeWidget.ToSharedRef(), MAX_int32);
		ScreenFadeViewport = Viewport;
	}
	if (ScreenFadeWidget)
	{
		ScreenFadeWidget->SetBorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, ScreenFadeOpacity));
		ScreenFadeWidget->SetVisibility(ScreenFadeOpacity > 0.f ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

// 맵 종료 시 화면 덮개 제거
void APFPlayerController::ReleaseScreenFade()
{
	if (ScreenFadeWidget && ScreenFadeViewport.IsValid())
		ScreenFadeViewport->RemoveGameLayerWidget(ScreenFadeWidget.ToSharedRef());
	ScreenFadeWidget.Reset();
	ScreenFadeViewport.Reset();
	ScreenFadeOpacity = 0.f;
}

// 조작 제한 시 열린 창, 드래그 정리
void APFPlayerController::UpdateUIAvailability()
{
	if (FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title"))) return;
	const bool bCanUseUI = CanUseUIInput();
	const bool bCanUseSettings = CanUseSettingsInput();
	const bool bCanUseResult = CanUseCampaignResultInput();
	if (InventoryWidget) InventoryWidget->SetIsEnabled(bCanUseUI);
	if (StatWidget) StatWidget->SetIsEnabled(bCanUseUI);
	if (ShopWidget) ShopWidget->SetIsEnabled(bCanUseUI);
	if (MenuWidget && !bCanUseSettings) MenuWidget->SetIsEnabled(false);
	if (CampaignWidget) CampaignWidget->SetIsEnabled(bCanUseUI || bCanUseResult);
	if (bCanUseUI) return;
	const bool bWindowOpen = (InventoryWidget && InventoryWidget->IsInventoryWindowVisible())
		|| (StatWidget && StatWidget->IsStatWindowVisible()) || IsShopOpen()
		|| (MenuWidget && MenuWidget->IsInViewport() && !bCanUseSettings);
	if (!bWindowOpen)
	{
		if (bCanUseResult || (bCanUseSettings && MenuWidget && MenuWidget->IsInViewport()))
		{
			if (!bShowMouseCursor) UpdateWindowInputMode();
			return;
		}
		if (!bShowMouseCursor) return;
	}
	if (InventoryWidget) InventoryWidget->SetInventoryWindowVisible(false);
	if (StatWidget) StatWidget->SetStatWindowVisible(false);
	if (MenuWidget && !bCanUseSettings) MenuWidget->RemoveFromParent();
	CloseShop();
	UpdateWindowInputMode();
}

// 캠페인 관전 대기 상태
void APFPlayerController::SetCampaignWaiting(bool bWaiting)
{
	if (!HasAuthority()) return;
	bCampaignWaiting = bWaiting;
	if (bWaiting) ResetGameplayInput();
	ForceNetUpdate();
}

// 서버에서 구한 미니맵 안내 위치
bool APFPlayerController::GetCampaignGuide(FVector& Location) const
{
	Location = CampaignGuide;
	return bHasCampaignGuide;
}

// 복구 기준의 시점 적용
void APFPlayerController::SetCampaignViewState(const FPFCharacterSharedStateSnapshot& Snapshot)
{
	SavedViewState = Snapshot;
	bHasSavedViewState = true;
	RestoreCharacterState();
}

// 로컬 연출 건너뛰기 요청
void APFPlayerController::SkipCampaignCamera()
{
	if (bMapFadePending || ScreenFadeOpacity > 0.f) return;
	if (APFCampaignDirector::Find(GetWorld())) Server_SkipCampaignCamera();
}

// 서버 연출 건너뛰기 투표
void APFPlayerController::Server_SkipCampaignCamera_Implementation()
{
	if (APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld())) Director->VoteSkip(this);
}

// 캠페인 HUD, 동료 관전, 공통 카메라 갱신
void APFPlayerController::UpdateCampaignPresentation()
{
	if (!APFSessionGameState::IsStory(GetWorld())) { ReleaseCampaignPresentation(); return; }
	APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld());
	if (!Director) { ReleaseCampaignPresentation(); return; }
	if (PresentedCampaign.Get() != Director)
	{
		ReleaseCampaignPresentation();
		PresentedCampaign = Director;
	}
	if (!CampaignWidget)
	{
		CampaignWidget = CreateWidget<UPFCampaignWidget>(this, UPFCampaignWidget::StaticClass());
		if (CampaignWidget) CampaignWidget->AddToPlayerScreen(etoi(PLAYERSTAT) + 3);
	}
	const bool bLock = APFCampaignDirector::BlocksInput(this);
	if (bLock != bCampaignInputLocked)
	{
		bCampaignInputLocked = bLock;
		SetIgnoreMoveInput(bLock);
		SetIgnoreLookInput(bLock);
		ResetGameplayInput();
	}
	APFCharacter* ViewedCharacter = Cast<APFCharacter>(GetPawn());
	if (Director->Progress.Phase == EPFCampaignPhase::Camera)
	{
		if (!bCampaignCameraActive)
		{
			CampaignPreviousView = GetViewTarget();
			bCampaignCameraActive = true;
		}
		if (ViewedCharacter) ViewedCharacter->SetCampaignCameraActive(true);
		const APFCampaignShot* Shot = Director->GetCurrentShot();
		if (!Shot) return;
		if (!CampaignCamera) CampaignCamera = GetWorld()->SpawnActor<ACameraActor>();
		if (!CampaignCamera) return;
		FVector Position;
		FRotator Rotation;
		Shot->Evaluate((Director->ServerTime() - Director->Progress.PhaseStarted) / Shot->Duration, Position, Rotation);
		if (Shot->Id == FName(TEXT("Extraction")))
			ScreenFadeOpacity = FMath::Clamp(static_cast<float>(Director->ServerTime() - Director->Progress.PhaseStarted) - Shot->Duration + 1.f, 0.f, 1.f);
		CampaignCamera->SetActorLocationAndRotation(Position, Rotation);
		CampaignCamera->GetCameraComponent()->SetFieldOfView(Shot->FieldOfView);
		if (GetViewTarget() != CampaignCamera) SetViewTargetWithBlend(CampaignCamera, bMapFadePending ? 0.f : .3f);
		return;
	}
	if (bCampaignCameraActive)
	{
		if (Director->Progress.Phase == EPFCampaignPhase::Starting && (!ViewedCharacter || ViewedCharacter->IsDeadCharacter())) return;
		bCampaignCameraActive = false;
		if (ViewedCharacter) ViewedCharacter->SetCampaignCameraActive(false);
		AActor* Restore = ViewedCharacter ? static_cast<AActor*>(ViewedCharacter) : CampaignPreviousView.Get();
		if (Restore) SetViewTargetWithBlend(Restore, .3f);
		if (CampaignCamera) CampaignCamera->Destroy();
		CampaignCamera = nullptr;
		CampaignPreviousView.Reset();
	}
	if (bCampaignWaiting)
	{
		for (TActorIterator<APFCharacter> It(GetWorld()); It; ++It)
			if (It->IsPlayerCharacter() && !It->IsDeadCharacter() && *It != ViewedCharacter)
			{
				if (GetViewTarget() != *It) SetViewTargetWithBlend(*It, .3f);
				break;
			}
	}
	else if (ViewedCharacter && GetViewTarget() != ViewedCharacter && Director->Progress.Phase == EPFCampaignPhase::Playing)
	{
		SetViewTargetWithBlend(ViewedCharacter, .3f);
	}
	if (Director->Progress.Phase == EPFCampaignPhase::Complete && !bCampaignResultVisible)
	{
		bCampaignResultVisible = true;
		ScreenFadeOpacity = 0.f;
		UpdateWindowInputMode();
	}
}

// 맵 이동, 중단 시 연출과 입력 잠금 정리
void APFPlayerController::ReleaseCampaignPresentation()
{
	if (!bMapFadePending && (bCampaignCameraActive || bCampaignResultVisible))
	{
		ScreenFadeOpacity = 0.f;
		MapFadeInRemaining = 0.f;
	}
	if (bCampaignCameraActive)
	{
		AActor* Restore = GetPawn() ? static_cast<AActor*>(GetPawn()) : CampaignPreviousView.Get();
		if (IsValid(Restore)) SetViewTarget(Restore);
	}
	if (APFCharacter* ViewedCharacter = Cast<APFCharacter>(GetPawn())) ViewedCharacter->SetCampaignCameraActive(false);
	if (bCampaignInputLocked)
	{
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		bCampaignInputLocked = false;
	}
	if (CampaignCamera) CampaignCamera->Destroy();
	CampaignCamera = nullptr;
	bCampaignCameraActive = false;
	CampaignPreviousView.Reset();
	if (CampaignWidget) CampaignWidget->RemoveFromParent();
	CampaignWidget = nullptr;
	PresentedCampaign.Reset();
	if (bCampaignResultVisible)
	{
		bCampaignResultVisible = false;
		UpdateWindowInputMode();
	}
}

void APFPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APFPlayerController, bCampaignWaiting);
	DOREPLIFETIME(APFPlayerController, CampaignGuide);
	DOREPLIFETIME(APFPlayerController, bHasCampaignGuide);
}

// 온라인 닉네임, 플레이어 이름 조회
FString APFPlayerController::GetUsername() const
{
	if (IsLocalController())
	{
		if (const UPFGameInstance* Instance = GetGameInstance<UPFGameInstance>())
		{
			return Instance->GetOnlineUsername();
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
	if (!MinimapWidget)
	{
		MinimapWidget = CreateWidget<UPFMinimapWidget>(this, UPFMinimapWidget::StaticClass());
		if (MinimapWidget)
		{
			MinimapWidget->AddToPlayerScreen(etoi(PLAYERSTAT) - 2);
		}
	}

	// 인벤토리, 퀵슬롯 UI 생성
	if (!InventoryWidget && InventoryWidgetClass)
	{
		InventoryWidget = CreateWidget<UPFInventoryWidget>(this, InventoryWidgetClass);
		if (InventoryWidget)
		{
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
	if (!StatWidget && StatWidgetClass)
	{
		StatWidget = CreateWidget<UPFStatWidget>(this, StatWidgetClass);
		if (StatWidget)
		{
			StatWidget->AddToViewport(etoi(PLAYERSTAT) + 2);
			StatWidget->BindPlayerState(GetPlayerState<APFPlayerState>());
			StatWidget->SetVisibility(ESlateVisibility::Visible);
			StatWidget->SetStatWindowVisible(false);
		}
		else
		{
			PFLOG(Warning, TEXT("Stat Widget Create Failed"));
		}
	}
	BindControlledCharacter();
	BindCharacterHUD();
	UpdateUIAvailability();
}

// 맵 관리자의 로컬 안내 연결
UPFTutorialWidget* APFPlayerController::AttachTutorial(APFTutorialManager* Manager)
{
	if (!IsLocalController() || !IsValid(Manager) || Manager->GetWorld() != GetWorld()) return nullptr;
	if (TutorialManager.Get() != Manager)
	{
		DetachTutorial(TutorialManager.Get());
		TutorialManager = Manager;
	}
	if (!TutorialWidget)
	{
		TutorialWidget = CreateWidget<UPFTutorialWidget>(this, UPFTutorialWidget::StaticClass());
		if (TutorialWidget)
		{
			TutorialWidget->SetVisibility(ESlateVisibility::Hidden);
			TutorialWidget->AddToPlayerScreen(etoi(PLAYERSTAT) + 3);
		}
	}
	return TutorialWidget;
}

// 맵 관리자의 로컬 안내 해제
void APFPlayerController::DetachTutorial(APFTutorialManager* Manager)
{
	if (TutorialManager.Get() != Manager) return;
	if (TutorialWidget)
	{
		TutorialWidget->RemoveFromParent();
		TutorialWidget = nullptr;
	}
	TutorialManager.Reset();
}

// 안내 대상 창의 현재 상태 전달
void APFPlayerController::GetTutorialWindows(UPFInventoryWidget*& OutInventory, UPFStatWidget*& OutStats, bool& bOutMenuOpen) const
{
	OutInventory = InventoryWidget;
	OutStats = StatWidget;
	bOutMenuOpen = (MenuWidget && MenuWidget->IsInViewport()) || IsShopOpen();
}

// 게임 메뉴 표시 전환
void APFPlayerController::OpenMenu()
{
	if (!CanUseSettingsInput()) return;
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
	if (!MenuWidget && MenuWidgetClass)
	{
		MenuWidget = CreateWidget<UPFMenuWidget>(this, MenuWidgetClass);
	}
	if (!MenuWidget)
	{
		return;
	}

	// 다른 창, 누르고 있던 조작 입력 해제
	CloseShop();
	if (InventoryWidget) InventoryWidget->SetInventoryWindowVisible(false);
	if (StatWidget) StatWidget->SetStatWindowVisible(false);
	ReleaseGameplayInputForWindow();
	MenuWidget->SetIsEnabled(true);
	MenuWidget->AddToViewport(etoi(PLAYERSTAT) + 10);
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
	if (!CanUseUIInput()) return;
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
		InventoryWidget->SetInventoryWindowVisible(false);
		UpdateWindowInputMode();
		return;
	}

	// 스탯 창을 닫고 인벤토리 표시
	if (StatWidget)
	{
		StatWidget->SetStatWindowVisible(false);
	}
	CloseShop();
	InventoryWidget->SetInventoryWindowVisible(true);
	UpdateWindowInputMode();
}

// 스탯 창 표시 전환
void APFPlayerController::ToggleStats()
{
	if (!CanUseUIInput()) return;
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
	if (bWillOpen) CloseShop();
	if (bWillOpen && InventoryWidget)
	{
		InventoryWidget->SetInventoryWindowVisible(false);
	}

	StatWidget->SetStatWindowVisible(bWillOpen);
	UpdateWindowInputMode();
}

// 상점 표시 여부
bool APFPlayerController::IsShopOpen() const
{
	return ShopWidget && ShopWidget->IsInViewport();
}

// 상점 닫기, 게임 입력 복원
void APFPlayerController::CloseShop()
{
	if (!ShopWidget) return;
	ShopWidget->RemoveFromParent();
	ShopWidget = nullptr;
	UpdateWindowInputMode();
}

// 상점 표시 전환, 로컬 조작 해제
void APFPlayerController::ToggleShop()
{
	if (!IsLocalController() || !CanUseUIInput()) return;
	if (IsShopOpen())
	{
		CloseShop();
		return;
	}
	if (!GetPlayerState<APFPlayerState>() || (MenuWidget && MenuWidget->IsInViewport())
		|| FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title"))) return;

	ShopWidget = CreateWidget<UPFShopWidget>(this, UPFShopWidget::StaticClass());
	if (!ShopWidget) return;
	if (InventoryWidget) InventoryWidget->SetInventoryWindowVisible(false);
	if (StatWidget) StatWidget->SetStatWindowVisible(false);
	ReleaseGameplayInputForWindow();
	ShopWidget->AddToPlayerScreen(etoi(PLAYERSTAT) + 5);
	UpdateWindowInputMode();
}

// 창 진입 시 누르고 있던 조작 해제
void APFPlayerController::ReleaseGameplayInputForWindow()
{
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
}

// 열린 창에 맞춰 입력, 커서 전환
void APFPlayerController::UpdateWindowInputMode()
{
	UUserWidget* ModalWidget = nullptr;
	if (CanUseSettingsInput() && MenuWidget && MenuWidget->IsInViewport()) ModalWidget = MenuWidget;
	else if (CanUseUIInput() && IsShopOpen()) ModalWidget = ShopWidget;
	else if (CanUseCampaignResultInput() && CampaignWidget) ModalWidget = CampaignWidget;
	if (ModalWidget)
	{
		if (ModalWidget == CampaignWidget.Get())
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(ModalWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			SetInputMode(InputMode);
		}
		else
		{
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(ModalWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
		}
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
	if (!CanUseUIInput() || (!bInventoryOpen && !bStatsOpen))
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
	else
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

// 지정한 퀵슬롯 사용 요청
void APFPlayerController::UseQuickSlotByIndex(int32 QuickSlotIndex)
{
	if (!IsLocalController() || !CanUseUIInput())
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

	if (TutorialManager.IsValid() && PFPlayerState->GetQuickSlotItemIDs().IsValidIndex(QuickSlotIndex))
	{
		const int32 ItemID = PFPlayerState->GetQuickSlotItemIDs()[QuickSlotIndex];
		TutorialManager->NotifyQuickSlotUse(this, ItemID, PFPlayerState->GetInventoryItemCount(ItemID));
	}
	PFPlayerState->UseQuickSlot(QuickSlotIndex, PFPlayer);
}

// 화면 중앙의 조준 위치 계산
FVector APFPlayerController::CalculateAimPoint(const APFCharacter* ControlledPawn, bool* bOutCharacterTargeted) const
{
	if (bOutCharacterTargeted)
	{
		*bOutCharacterTargeted = false;
	}

	constexpr float MaxAimTraceDistance = 4000.f;
	FVector TraceStart = ControlledPawn->GetPawnViewLocation();
	FVector TraceDirection = ControlledPawn->GetBaseAimRotation().Vector();

	// 화면 중앙을 월드 조준선으로 변환
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	FVector ScreenCenterLocation;
	FVector ScreenCenterDirection;

	if (DeprojectScreenPositionToWorld(static_cast<float>(ViewportSizeX) * 0.5f,
		static_cast<float>(ViewportSizeY) * 0.5f, ScreenCenterLocation, ScreenCenterDirection))
	{
		TraceStart = ScreenCenterLocation;
		TraceDirection = ScreenCenterDirection;
	}

	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * MaxAimTraceDistance;
	static const ECollisionChannel AimTraceChannel = GetCollisionChannel(PFCollisionChannelNames::AimTrace);

	// 자신, 부착 액터를 조준 검사에서 제외
	FCollisionQueryParams AimTraceParams(SCENE_QUERY_STAT(TwinBlastAimTrace), false, ControlledPawn);
	TArray<AActor*> AttachedActors;
	ControlledPawn->GetAttachedActors(AttachedActors);
	AimTraceParams.AddIgnoredActors(AttachedActors);
	AimTraceParams.AddIgnoredActor(ControlledPawn);

	// 조준선 충돌 위치 반환
	FHitResult AimHit;
	if (GetWorld()->LineTraceSingleByChannel(AimHit, TraceStart, TraceEnd, AimTraceChannel, AimTraceParams))
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

// 캐릭터 조작 상태에 따른 UI 입력 허용
bool APFPlayerController::CanUseUIInput() const
{
	const APFCharacter* ControlledPawn = GetControlledCharacter();
	return !bMapFadePending && MapFadeInRemaining <= 0.f && ScreenFadeOpacity <= 0.f
		&& IsCurrentCharacter(ControlledPawn) && !ControlledPawn->IsLevelStartActive()
		&& !ControlledPawn->IsMovementBlocked() && !IsMoveInputIgnored() && !IsLookInputIgnored();
}

// 페이드 외 상태에서 설정창 조작 허용
bool APFPlayerController::CanUseSettingsInput() const
{
	return IsLocalController() && !bMapFadePending && MapFadeInRemaining <= 0.f && ScreenFadeOpacity <= 0.f;
}

// 엔딩 결과 화면의 타이틀 복귀 허용
bool APFPlayerController::CanUseCampaignResultInput() const
{
	const APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld());
	return !bMapFadePending && MapFadeInRemaining <= 0.f && ScreenFadeOpacity <= 0.f
		&& Director && Director->Progress.Phase == EPFCampaignPhase::Complete;
}

// 공통 조작 상태, 열린 창에 따른 일반 행동 차단
bool APFPlayerController::CanUseGameplayInput(const APFCharacter* ControlledPawn) const
{
	return CanUseUIInput() && IsCurrentCharacter(ControlledPawn) && !IsShopOpen()
		&& !(MenuWidget && MenuWidget->IsInViewport());
}

// 보류된 이동, 점프 입력 해제
void APFPlayerController::ResetGameplayInput()
{
	JumpButtonHeld = false;
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
	LastMovementDirection = IDLE;
	RotationInput = FRotator::ZeroRotator;
	if (APFCharacter* ControlledPawn = GetControlledCharacter())
	{
		ControlledPawn->StopJumping();
		ControlledPawn->ConsumeMovementInputVector();
		ControlledPawn->SetMovementInputDirection(IDLE);
		if (ControlledPawn->IsJumpPadFlightActive() && StatWidget)
		{
			StatWidget->SetStatWindowVisible(false);
			UpdateWindowInputMode();
		}
	}
}

void APFPlayerController::OnUnPossess()
{
	UnbindControlledCharacter();
	Super::OnUnPossess();
	if (StatWidget) StatWidget->RefreshStats();
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
		if (StatWidget)
		{
			StatWidget->BindPlayerState(GetPlayerState<APFPlayerState>());
			StatWidget->RefreshStats();
		}
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
		BindControlledCharacter();
		BindCharacterHUD();
		if (StatWidget)
		{
			StatWidget->BindPlayerState(GetPlayerState<APFPlayerState>());
			StatWidget->RefreshStats();
		}
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
	if (APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld()))
	{
		GetWorldTimerManager().ClearTimer(PlayerRespawnTimerHandle);
		Director->HandlePlayerDeath(this);
		return;
	}
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
	if (IsLocalController() && JumpButtonHeld && CanUseGameplayInput(ControlledPawn))
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
	UpdateCrosshairVisibility();
}

// 캐릭터 상태, 연출에 따른 조준점 표시
void APFPlayerController::UpdateCrosshairVisibility()
{
	if (!CrosshairWidget) return;
	const APFCharacter* ControlledPawn = GetControlledCharacter();
	const bool bInTitle = FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title"));
	const bool bVisible = !bInTitle && !bMapFadePending && ScreenFadeOpacity <= 0.f && !bCampaignCameraActive && !bCampaignWaiting
		&& !APFCampaignDirector::BlocksInput(this)
		&& IsCurrentCharacter(ControlledPawn) && ControlledPawn->IsLocalPlayerCharacter()
		&& !ControlledPawn->IsLevelStartActive() && ControlledPawn->HasCrosshair()
		&& ControlledPawn->GetCurrentControlMode() != TOPVIEW;
	CrosshairWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

// 로컬 이동 방향, 공격 조준 갱신
void APFPlayerController::UpdateCharacterControl()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!CanUseGameplayInput(ControlledPawn))
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
		const FVector CurrentAim = CalculateAimPoint(ControlledPawn, &bCharacterTargeted);
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
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); CanUseGameplayInput(ControlledPawn))
	{
		JumpButtonHeld = true;
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
	if (!CanUseGameplayInput(ControlledPawn) || ControlledPawn->IsMovementBlocked())
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
	if (!CanUseGameplayInput(ControlledPawn) || ControlledPawn->IsMovementBlocked())
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
	if (CanUseGameplayInput(ControlledPawn) && ControlledPawn->GetCurrentControlMode() != TOPVIEW)
	{
		const UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
		const float Sensitivity = GI ? GI->GetCameraSensitivity() / 100.f : 1.f;
		AddPitchInput(Value * ControlledPawn->GetLookUpSpeed() * Sensitivity);
	}
}

// 시선 좌우 회전
void APFPlayerController::Turn(float Value)
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); CanUseGameplayInput(ControlledPawn))
	{
		const UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
		const float Sensitivity = GI ? GI->GetCameraSensitivity() / 100.f : 1.f;
		AddYawInput(Value * ControlledPawn->GetTurnSpeed() * Sensitivity);
	}
}

// 공격 입력 시작
void APFPlayerController::AttackStart()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); CanUseGameplayInput(ControlledPawn))
	{
		Server_UpdateAimPoint(ControlledPawn, CalculateAimPoint(ControlledPawn));
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
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); CanUseGameplayInput(ControlledPawn))
	{
		ControlledPawn->ActivateUltimateAbility();
	}
}

// 질주 입력
void APFPlayerController::Sprint()
{
	if (CanUseGameplayInput(GetControlledCharacter())) Server_Sprint(GetControlledCharacter());
}

// 시점 고정 입력
void APFPlayerController::ViewpointFix()
{
	if (CanUseGameplayInput(GetControlledCharacter())) Server_ViewpointFix(GetControlledCharacter());
}

// 카메라 시점 전환
void APFPlayerController::ViewChange()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (!CanUseGameplayInput(ControlledPawn) || ControlledPawn->IsLevelStartActive()) return;
	switch (ControlledPawn->GetCurrentControlMode())
	{
	case TOPVIEW:
		Server_SetControlMode(ControlledPawn, TPS);
		break;
	case TPS:
		Server_SetControlMode(ControlledPawn, FPS);
		break;
	case FPS:
		Server_SetControlMode(ControlledPawn, TOPVIEW);
		break;
	default:
		break;
	}
}

// 서버 이동 입력 반영
void APFPlayerController::Server_SetDir_Implementation(APFCharacter* ControlledPawn, EPFDirection NewDirection)
{
	if (CanUseGameplayInput(ControlledPawn)) ControlledPawn->SetMovementInputDirection(NewDirection);
}

// 서버 카메라 모드 반영
void APFPlayerController::Server_SetControlMode_Implementation(APFCharacter* ControlledPawn, ECONTROLMODE NewControlMode)
{
	if (CanUseGameplayInput(ControlledPawn) && !ControlledPawn->IsLevelStartActive()) ControlledPawn->SetControlMode(NewControlMode);
}

// 서버 질주 전환
void APFPlayerController::Server_Sprint_Implementation(APFCharacter* ControlledPawn)
{
	if (CanUseGameplayInput(ControlledPawn)) ControlledPawn->ToggleSprint();
}

// 서버 시점 고정 전환
void APFPlayerController::Server_ViewpointFix_Implementation(APFCharacter* ControlledPawn)
{
	if (CanUseGameplayInput(ControlledPawn)) ControlledPawn->ToggleViewpointFixed();
}

// 서버 조준점 반영
void APFPlayerController::Server_UpdateAimPoint_Implementation(APFCharacter* ControlledPawn, FVector NewAimPoint)
{
	if (CanUseGameplayInput(ControlledPawn) && !NewAimPoint.ContainsNaN())
	{
		AimPoint = NewAimPoint;
	}
}

bool APFPlayerController::TryGetCombatAim(FVector& OutAimPoint)
{
	if (!CanUseGameplayInput(GetControlledCharacter())) return false;
	OutAimPoint = AimPoint;
	return true;
}

// 상자 접근 대상 갱신
void APFPlayerController::SetChest(APFChest* Chest)
{
	if (!Chest && NearestChest.IsValid()
		&& NearestChest->Trigger->IsOverlappingActor(GetPawn()))
	{
		return;
	}
	NearestChest = Chest;
}

// 상호작용 요청
void APFPlayerController::Interaction()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); CanUseGameplayInput(ControlledPawn) && !ControlledPawn->IsLevelStartActive())
	{
		Server_Interaction(ControlledPawn);
	}
}

// 서버 상자 열기
void APFPlayerController::Server_Interaction_Implementation(APFCharacter* ControlledPawn)
{
	if (APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld()))
	{
		if (CanUseGameplayInput(ControlledPawn)) Director->Interact(this);
		return;
	}
	if (CanUseGameplayInput(ControlledPawn) && !ControlledPawn->IsLevelStartActive()
		&& NearestChest.IsValid()
		&& NearestChest->Trigger->IsOverlappingActor(ControlledPawn))
	{
		NearestChest->ChestOpen(this);
	}
}

// 캐릭터 교체 요청
void APFPlayerController::ChangeCharacter()
{
	if (APFCharacter* ControlledPawn = GetControlledCharacter(); CanUseGameplayInput(ControlledPawn) && !ControlledPawn->IsLevelStartActive())
	{
		Server_ChangeCharacter(ControlledPawn);
	}
}

// 서버 캐릭터 교체
void APFPlayerController::Server_ChangeCharacter_Implementation(APFCharacter* ControlledPawn)
{
	if (!CanUseGameplayInput(ControlledPawn) || ControlledPawn->IsLevelStartActive()
		|| ControlledPawn->GetCharacterMovement()->IsFalling() || ControlledPawn->HasAirborneTag()
		|| FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title")))
	{
		return;
	}
	if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GameMode->ChangeCharacter(this);
	}
}

// 테스트 맵 전환 요청
void APFPlayerController::ChangeTestMap()
{
	APFCharacter* ControlledPawn = GetControlledCharacter();
	if (IsLocalController() && CanUseGameplayInput(ControlledPawn) && !ControlledPawn->IsLevelStartActive())
	{
		Server_ChangeTestMap(ControlledPawn);
	}
}

// 서버에서 현재 캐릭터의 맵 전환 요청 처리
void APFPlayerController::Server_ChangeTestMap_Implementation(APFCharacter* ControlledPawn)
{
	if (!CanUseGameplayInput(ControlledPawn) || ControlledPawn->IsLevelStartActive())
	{
		return;
	}
	if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GameMode->ChangeTestMap();
	}
}

// 트윈블라스트 적 생성 요청
void APFPlayerController::SpawnTestTwinblastEnemy()
{
	if (CanUseGameplayInput(GetControlledCharacter())) Server_SpawnTestEnemy(GetControlledCharacter(), true);
}

// 광 적 생성 요청
void APFPlayerController::SpawnTestKwangEnemy()
{
	if (CanUseGameplayInput(GetControlledCharacter())) Server_SpawnTestEnemy(GetControlledCharacter(), false);
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

// 테스트 입력의 적 생성 처리
void APFPlayerController::Server_SpawnTestEnemy_Implementation(APFCharacter* ControlledPawn, bool bSpawnTwinblast)
{
	if (!CanUseGameplayInput(ControlledPawn)) return;
	if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GameMode->SpawnEnemy(ControlledPawn, bSpawnTwinblast ? APFTwinBlast::StaticClass() : APFKwang::StaticClass());
	}
}

// 로컬 관리자의 봇 준비 요청 전달
void APFPlayerController::RequestTutorialBot()
{
	if (IsLocalController() && TutorialManager.IsValid() && CanUseGameplayInput(GetControlledCharacter()))
	{
		Server_RequestTutorialBot(TutorialManager.Get(), GetControlledCharacter());
	}
}

// 서버의 맵 관리자에 봇 준비 요청 전달
void APFPlayerController::Server_RequestTutorialBot_Implementation(APFTutorialManager* Manager, APFCharacter* ControlledPawn)
{
	if (IsValid(Manager) && Manager == APFTutorialManager::Find(GetWorld()) && CanUseGameplayInput(ControlledPawn))
	{
		Manager->PrepareCombat(this);
	}
}

// 로컬 관리자로 봇 상태 전달
void APFPlayerController::Client_NotifyTutorialBot_Implementation(APFTutorialManager* Manager, APFCharacter* Bot, bool bDefeated)
{
	if (IsValid(Manager) && Manager == TutorialManager.Get())
	{
		Manager->NotifyBotState(this, Bot, bDefeated);
	}
}

// 서버에서 확인한 상자 보상 진행 전달
void APFPlayerController::Client_NotifyTutorialChest_Implementation(APFTutorialManager* Manager, FName ItemName, FVector ItemLocation, bool bCollected)
{
	if (IsValid(Manager) && Manager == TutorialManager.Get())
	{
		Manager->NotifyChestState(this, ItemName, ItemLocation, bCollected);
	}
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
		if (APFChest* Chest = Cast<APFChest>(Actor); IsValid(Chest)
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
