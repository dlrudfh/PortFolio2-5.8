#pragma once

#include "Blueprint/UserWidget.h"
#include "PFMenuWidget.generated.h"

// 게임 설정, 타이틀 복귀, 종료 메뉴
UCLASS()
class PORTFOLIO_API UPFMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void HandleVolumeChanged(float Value);
	UFUNCTION()
	void HandleSensitivityChanged(float Value);
	UFUNCTION()
	void SaveSettings();
	UFUNCTION()
	void HandleGoToTitle();
	UFUNCTION()
	void HandleExitGame();

	UPROPERTY(Transient)
	class USlider* MenuVolumeSlider = nullptr;
	UPROPERTY(Transient)
	class USlider* CameraSensitivitySlider = nullptr;
	UPROPERTY(Transient)
	class UTextBlock* VolumeValueText = nullptr;
	UPROPERTY(Transient)
	class UTextBlock* SensitivityValueText = nullptr;
};
