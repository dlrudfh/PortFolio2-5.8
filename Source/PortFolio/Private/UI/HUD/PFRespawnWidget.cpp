#include "UI/HUD/PFRespawnWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

TSharedRef<SWidget> UPFRespawnWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		// 화면 중앙 위에 부활 안내 배치
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RespawnRoot"));
		WidgetTree->RootWidget = RootCanvas;

		UVerticalBox* LayoutBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RespawnLayout"));
		if (UCanvasPanelSlot* LayoutSlot = RootCanvas->AddChildToCanvas(LayoutBox))
		{
			LayoutSlot->SetAnchors(FAnchors(0.5f, 0.4f));
			LayoutSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LayoutSlot->SetPosition(FVector2D::ZeroVector);
			LayoutSlot->SetAutoSize(true);
		}

		UTextBlock* RespawnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RespawnTitle"));
		RespawnText->SetText(FText::FromString(TEXT("Respawn")));
		RespawnText->SetJustification(ETextJustify::Center);
		RespawnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RespawnText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		RespawnText->SetShadowOffset(FVector2D(2.f, 2.f));
		FSlateFontInfo TitleFont = RespawnText->GetFont();
		TitleFont.Size = 96;
		RespawnText->SetFont(TitleFont);
		if (UVerticalBoxSlot* TitleSlot = LayoutBox->AddChildToVerticalBox(RespawnText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		// 왼쪽 기준으로 줄어드는 부활 게이지 구성
		USizeBox* GaugeSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RespawnGaugeSize"));
		GaugeSizeBox->SetWidthOverride(1220.f);
		GaugeSizeBox->SetHeightOverride(62.f);
		if (UVerticalBoxSlot* GaugeSlot = LayoutBox->AddChildToVerticalBox(GaugeSizeBox))
		{
			GaugeSlot->SetHorizontalAlignment(HAlign_Center);
		}

		RespawnProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("RespawnGauge"));
		FProgressBarStyle GaugeStyle;
		GaugeStyle.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.02f, 0.03f, 0.05f, 0.85f)));
		GaugeStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
		GaugeStyle.SetEnableFillAnimation(false);
		RespawnProgressBar->SetWidgetStyle(GaugeStyle);
		RespawnProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
		RespawnProgressBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
		RespawnProgressBar->SetBorderPadding(FVector2D::ZeroVector);
		RespawnProgressBar->SetFillColorAndOpacity(FLinearColor(0.1f, 0.8f, 0.85f, 1.f));
		RespawnProgressBar->SetPercent(1.f);
		GaugeSizeBox->SetContent(RespawnProgressBar);
	}

	return Super::RebuildWidget();
}

// 서버 부활 시각으로 대기 표시 시작
void UPFRespawnWidget::StartCountdown(double InRespawnEndServerTime, float InRespawnDuration)
{
	RespawnEndServerTime = InRespawnEndServerTime;
	RespawnDuration = InRespawnDuration;
	if (RespawnProgressBar)
	{
		RespawnProgressBar->SetPercent(1.f);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UpdateCountdown();
}

// 부활 대기 표시 종료
void UPFRespawnWidget::StopCountdown()
{
	RespawnEndServerTime = 0.0;
	RespawnDuration = 0.f;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPFRespawnWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateCountdown();
}

// 서버 시각에 맞춰 남은 시간 비율 갱신
void UPFRespawnWidget::UpdateCountdown()
{
	if (!RespawnProgressBar || RespawnDuration <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	const double RemainingTime = RespawnEndServerTime - GameState->GetServerWorldTimeSeconds();
	const float RemainingRatio = static_cast<float>(FMath::Clamp(RemainingTime / RespawnDuration, 0.0, 1.0));
	RespawnProgressBar->SetPercent(RemainingRatio);
}
