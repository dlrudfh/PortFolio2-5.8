#include "System/Framework/PFLobbyPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFLobbyGameMode.h"
#include "System/Framework/PFPlayerState.h"
#include "UI/PFGameCursor.h"

// 로비 정보 수신 전에 선택 카메라 고정
void APFLobbyPlayerController::InitializeLobbyCamera(APlayerController* PlayerController)
{
	UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer
		|| !World->GetMapName().Contains(TEXT("Title")) || !PlayerController->PlayerCameraManager) return;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* CameraActor = *It;
		if (!CameraActor->ActorHasTag(TEXT("DummyCamera"))) continue;

		// 로비 카메라의 고정 노출, 빛 번짐 조절
		UCameraComponent* Camera = CameraActor->GetCameraComponent();
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

		PlayerController->bAutoManageActiveCameraTarget = false;
		PlayerController->SetViewTarget(CameraActor);
		PlayerController->PlayerCameraManager->UpdateCamera(0.f);
		return;
	}
}

void APFLobbyPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	InitializeLobbyCamera(this);
}

void APFLobbyPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController()) InitializeLobbyCamera(this);
}

void APFLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	FPFGameCursor::Install(GetWorld()->GetGameViewport());
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		GI->ApplyMenuVolume();
		GI->CreateTitle();
	}
	SynchronizeUsername();
	GetWorldTimerManager().SetTimer(UsernameSyncTimerHandle, this, &APFLobbyPlayerController::SynchronizeUsername, 1.f, true);
}

void APFLobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SynchronizeUsername();
}

void APFLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UsernameSyncTimerHandle);
	if (IsLocalController())
		if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>()) GI->RemoveTitle();
	Super::EndPlay(EndPlayReason);
}

// 로컬 온라인 이름 동기화
void APFLobbyPlayerController::SynchronizeUsername()
{
	const APFPlayerState* State = GetPlayerState<APFPlayerState>();
	const UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
	if (!IsLocalController() || !State || !GI) return;
	const FString Username = GI->GetOnlineUsername();
	if (!Username.IsEmpty() && State->GetPlayerName() != Username) Server_SetUsername(Username);
}

// 서버 플레이어 이름 반영
void APFLobbyPlayerController::Server_SetUsername_Implementation(const FString& Username)
{
	FString Name = Username.TrimStartAndEnd();
	if (Name.IsEmpty() || Name.Len() > 128) return;
	if (APFPlayerState* State = GetPlayerState<APFPlayerState>()) State->SetPlayerName(Name);
}

// 서버에 캐릭터 선택 요청
void APFLobbyPlayerController::Server_SelectCharacter_Implementation(ECHARACTER SelectedCharacter)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SelectCharacter(this, SelectedCharacter);
}

// 서버에 준비 상태 요청
void APFLobbyPlayerController::Server_SetReady_Implementation(bool bReady, int32 SettingsRevision)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SetReady(this, bReady, SettingsRevision);
}

// 서버에 대전 맵 변경 요청
void APFLobbyPlayerController::Server_SelectMap_Implementation(FName Map)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SelectMap(this, Map);
}

// 서버에 로비 정원 변경 요청
void APFLobbyPlayerController::Server_SetCapacity_Implementation(int32 Capacity)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SetCapacity(this, Capacity);
}

// 서버에 게임 시작 요청
void APFLobbyPlayerController::Server_StartMatch_Implementation()
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->StartMatch(this);
}

// 서버에 로비 퇴장 요청
void APFLobbyPlayerController::Server_LeaveLobby_Implementation()
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->LeaveLobby(this);
}

// 세션을 정리하고 타이틀로 복귀
void APFLobbyPlayerController::Client_LeaveLobby_Implementation()
{
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>()) GI->ReturnToMainMenu();
}
