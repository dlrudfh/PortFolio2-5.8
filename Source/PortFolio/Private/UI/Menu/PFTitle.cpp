#include "UI/Menu/PFTitle.h"

#include "Character/PFDummy.h"
#include "EngineUtils.h"
#include "System/Framework/PFGameInstance.h"
#include "Types/SlateEnums.h" 
#include "Components/Button.h"
#include "Camera/CameraActor.h"
#include "System/Framework/PFPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Internationalization/Text.h"
#include "Components/EditableTextBox.h"

void UPFTitle::NativeConstruct()
{
	Super::NativeConstruct();

	// 메뉴 버튼, 입력 이벤트 연결
	TitleUI = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TitleUI")));
	PFCHECK(TitleUI);

	CreateSessionButton = Cast<UButton>(GetWidgetFromName(TEXT("CreateSession")));
	PFCHECK(CreateSessionButton);
	CreateSessionButton->OnClicked.AddDynamic(this, &UPFTitle::CreateSessionNameInput);

	JoinSessionButton = Cast<UButton>(GetWidgetFromName(TEXT("JoinSession")));
	PFCHECK(JoinSessionButton);
	JoinSessionButton->OnClicked.AddDynamic(this, &UPFTitle::JoinSessionNameInput);

	ExitGameButton = Cast<UButton>(GetWidgetFromName(TEXT("ExitGame")));
	PFCHECK(ExitGameButton);
	ExitGameButton->OnClicked.AddDynamic(this, &UPFTitle::ExitGame);

	SessionNameInputBox = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("SessionNameInput")));
	PFCHECK(SessionNameInputBox);
	SessionNameInputBox->OnTextCommitted.AddDynamic(this, &UPFTitle::InputComplete);
	SessionNameInputBox->SetVisibility(ESlateVisibility::Hidden);

	ChooseCharacter = Cast<UTextBlock>(GetWidgetFromName(TEXT("ChooseCharacter")));
	PFCHECK(ChooseCharacter);
	ChooseCharacter->SetVisibility(ESlateVisibility::Hidden);

	SetIsFocusable(true);

	// 캐릭터 선택용 더미 생성
	FActorSpawnParameters DummySpawnParams;
	DummySpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APFDummy* Dummy_TwinBlast = GetWorld()->SpawnActor<APFDummy>(
		APFDummy::StaticClass(),
		FVector(1300.f, 1000.f, 100.f),
		FRotator(0.f, 0.f, 0.f),
		DummySpawnParams
	);
	if (Dummy_TwinBlast)
	{
		Dummy_TwinBlast->SetMesh(CHARACTER_TWINBLAST);
		Dummy_TwinBlast->SetTitle(this);
	}
	else
	{
		PFLOG(Warning, TEXT("Dummy_TwinBlast Spawn Failed"));
	}

	APFDummy* Dummy_Kwang = GetWorld()->SpawnActor<APFDummy>(
		APFDummy::StaticClass(),
		FVector(1600.f, 1000.f, 90.f),
		FRotator(0.f, 0.f, 0.f),
		DummySpawnParams
	);
	if (Dummy_Kwang)
	{
		Dummy_Kwang->SetMesh(CHARACTER_KWANG);
		Dummy_Kwang->SetTitle(this);
	}
	else
	{
		PFLOG(Warning, TEXT("Dummy_Kwang Spawn Failed"));
	}

	// 캐릭터 선택 카메라 적용
	FString TargetName = "DummyCamera";

	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		ACameraActor* Cam = *It;
		if (Cam && Cam->ActorHasTag(FName(*TargetName)))
		{
			PFLOG(Warning, TEXT("Found DummyCamera"));
			APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
			if (PC)
			{
				PC->SetViewTargetWithBlend(Cam, 0.f);
			}
			break;
		}
	}
}

// 생성할 세션 이름 입력
void UPFTitle::CreateSessionNameInput()
{
	if (bSessionBusy)
	{
		return;
	}

	IsCreateSession = true;
	SessionNameInputBox->SetKeyboardFocus();
	SessionNameInputBox->SetVisibility(ESlateVisibility::Visible);
}

// 참가할 세션 이름 입력
void UPFTitle::JoinSessionNameInput()
{
	if (bSessionBusy)
	{
		return;
	}

	IsCreateSession = false;
	SessionNameInputBox->SetKeyboardFocus();
	SessionNameInputBox->SetVisibility(ESlateVisibility::Visible);
}

// 세션 이름 입력 완료 처리
void UPFTitle::InputComplete(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (bSessionBusy)
	{
		return;
	}

	if (CommitMethod == ETextCommit::OnEnter)
	{
		SessionName = Text.ToString();

		if (IsCreateSession)
		{
			ShowCharacterSelection();
		}
		else
		{
			SessionNameInputBox->SetVisibility(ESlateVisibility::Hidden);
			if (!GetWorld()->GetGameInstance<UPFGameInstance>()->JoinGameSession(SessionName))
			{
				PFLOG(Warning, TEXT("Invalid Session"));
			}
		}
	}
}

// 캐릭터 선택 화면 표시
void UPFTitle::ShowCharacterSelection()
{
	SetSessionBusy(false);
	ChooseCharacter->SetVisibility(ESlateVisibility::Visible);
	TitleUI->SetVisibility(ESlateVisibility::Hidden);
	SessionNameInputBox->SetVisibility(ESlateVisibility::Hidden);
}

// 참가 실패 시 입력 화면 복원
void UPFTitle::ShowJoinFailed()
{
	SetSessionBusy(false);
	ChooseCharacter->SetVisibility(ESlateVisibility::Hidden);
	TitleUI->SetVisibility(ESlateVisibility::Visible);
	SessionNameInputBox->SetVisibility(ESlateVisibility::Visible);
	SessionNameInputBox->SetKeyboardFocus();
}

// 세션 처리 중 중복 입력 차단
void UPFTitle::SetSessionBusy(bool bBusy)
{
	bSessionBusy = bBusy;
	CreateSessionButton->SetIsEnabled(!bBusy);
	JoinSessionButton->SetIsEnabled(!bBusy);
	SessionNameInputBox->SetIsEnabled(!bBusy);
	ExitGameButton->SetIsEnabled(true);

	if (bBusy)
	{
		TitleUI->SetVisibility(ESlateVisibility::Visible);
		ChooseCharacter->SetVisibility(ESlateVisibility::Hidden);
		SessionNameInputBox->SetVisibility(ESlateVisibility::Hidden);
	}
}

// 캐릭터 선택 후 세션 시작
void UPFTitle::StartSession()
{
	if (bSessionBusy || !IsInViewport() || ChooseCharacter->GetVisibility() != ESlateVisibility::Visible)
	{
		return;
	}

	if (IsCreateSession)
	{
		PFLOG(Warning, TEXT("CreateSession"));
		GetWorld()->GetGameInstance<UPFGameInstance>()->CreateGameSession(SessionName);
	}
	else
	{
		PFLOG(Warning, TEXT("JoinSession"));
		GetWorld()->GetGameInstance<UPFGameInstance>()->StartGame();
	}
}

// 게임 종료 요청
void UPFTitle::ExitGame()
{
	PFLOG(Warning, TEXT("Exit Game"));
	GetWorld()->GetGameInstance<UPFGameInstance>()->ExitGame();
}
