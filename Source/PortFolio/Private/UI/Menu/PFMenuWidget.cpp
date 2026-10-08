#include "UI/Menu/PFMenuWidget.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFPlayerController.h"

void UPFMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	UButton* TitleButton = Cast<UButton>(GetWidgetFromName(TEXT("GoToTitle")));
	UButton* ExitButton = Cast<UButton>(GetWidgetFromName(TEXT("ExitGame")));
	MenuVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("VolumeSlider")));
	CameraSensitivitySlider = Cast<USlider>(GetWidgetFromName(TEXT("SensitivitySlider")));
	VolumeValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("VolumeValue")));
	SensitivityValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("SensitivityValue")));
	PFCHECK(TitleButton && ExitButton && MenuVolumeSlider && CameraSensitivitySlider && VolumeValueText && SensitivityValueText);

	TitleButton->OnClicked.AddUniqueDynamic(this, &UPFMenuWidget::HandleGoToTitle);
	ExitButton->OnClicked.AddUniqueDynamic(this, &UPFMenuWidget::HandleExitGame);

	if (const UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		MenuVolumeSlider->SetValue(GI->GetMenuVolume());
		CameraSensitivitySlider->SetValue(GI->GetCameraSensitivity());
		VolumeValueText->SetText(FText::AsNumber(FMath::RoundToInt(GI->GetMenuVolume())));
		SensitivityValueText->SetText(FText::AsNumber(FMath::RoundToInt(GI->GetCameraSensitivity())));
	}
	MenuVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UPFMenuWidget::HandleVolumeChanged);
	CameraSensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &UPFMenuWidget::HandleSensitivityChanged);
	MenuVolumeSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UPFMenuWidget::SaveSettings);
	CameraSensitivitySlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UPFMenuWidget::SaveSettings);
	MenuVolumeSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UPFMenuWidget::SaveSettings);
	CameraSensitivitySlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UPFMenuWidget::SaveSettings);
}

void UPFMenuWidget::NativeDestruct()
{
	SaveSettings();
	Super::NativeDestruct();
}

// 전체 음량 적용, 수치 갱신
void UPFMenuWidget::HandleVolumeChanged(float Value)
{
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseSettingsInput()) return;
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		GI->SetMenuVolume(Value);
		VolumeValueText->SetText(FText::AsNumber(FMath::RoundToInt(GI->GetMenuVolume())));
	}
}

// 카메라 감도 적용, 수치 갱신
void UPFMenuWidget::HandleSensitivityChanged(float Value)
{
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseSettingsInput()) return;
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		GI->SetCameraSensitivity(Value);
		if (Value < 1.f)
		{
			CameraSensitivitySlider->SetValue(GI->GetCameraSensitivity());
		}
		SensitivityValueText->SetText(FText::AsNumber(FMath::RoundToInt(GI->GetCameraSensitivity())));
	}
}

// 조절을 마친 설정 저장
void UPFMenuWidget::SaveSettings()
{
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		GI->SaveMenuSettings();
	}
}

FReply UPFMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::U || InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat())
		{
			if (APFPlayerController* Controller = Cast<APFPlayerController>(GetOwningPlayer()))
			{
				Controller->OpenMenu();
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UPFMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled().SetUserFocus(TakeWidget());
}

// 연결 종료, 타이틀 복귀
void UPFMenuWidget::HandleGoToTitle()
{
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseSettingsInput()) return;
	if (UPFGameInstance* GameInstance = GetGameInstance<UPFGameInstance>())
	{
		SetIsEnabled(false);
		GameInstance->ReturnToMainMenu();
	}
	else
	{
		PFLOG(Warning, TEXT("GoToTitle failed: PFGameInstance unavailable"));
	}
}

// 세션 정리 후 게임 종료
void UPFMenuWidget::HandleExitGame()
{
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseSettingsInput()) return;
	if (UPFGameInstance* GameInstance = GetGameInstance<UPFGameInstance>())
	{
		SetIsEnabled(false);
		GameInstance->ExitGame();
	}
	else
	{
		PFLOG(Warning, TEXT("ExitGame failed: PFGameInstance unavailable"));
	}
}
