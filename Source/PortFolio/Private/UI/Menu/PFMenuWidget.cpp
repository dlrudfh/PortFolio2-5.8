#include "UI/Menu/PFMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFPlayerController.h"

TSharedRef<SWidget> UPFMenuWidget::RebuildWidget()
{
	SetIsFocusable(true);
	if (!WidgetTree->RootWidget)
	{
		// 화면을 덮는 배경, 중앙 메뉴 배치
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuRoot"));
		WidgetTree->RootWidget = Root;
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBackground"));
		Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background);
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuPanel"));
		Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.98f));
		Panel->SetPadding(FMargin(32.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
		PanelSlot->SetSize(FVector2D(460.f, 350.f));

		UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuLayout"));
		Panel->SetContent(Layout);
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MenuTitle"));
		Title->SetText(FText::FromString(TEXT("MENU")));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.85f, 1.f)));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 32;
		Title->SetFont(TitleFont);
		Layout->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

		UButton* TitleButton = CreateMenuButton(Layout, TEXT("GoToTitle"), FText::FromString(TEXT("Go To Title")));
		TitleButton->OnClicked.AddDynamic(this, &UPFMenuWidget::HandleGoToTitle);
		UButton* ExitButton = CreateMenuButton(Layout, TEXT("ExitGame"), FText::FromString(TEXT("Exit Game")));
		ExitButton->OnClicked.AddDynamic(this, &UPFMenuWidget::HandleExitGame);

		UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MenuHint"));
		Hint->SetText(FText::FromString(TEXT("U / Esc : Close")));
		Hint->SetJustification(ETextJustify::Center);
		Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.67f, 0.72f)));
		FSlateFontInfo HintFont = Hint->GetFont();
		HintFont.Size = 16;
		Hint->SetFont(HintFont);
		Layout->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
	return Super::RebuildWidget();
}

// 메뉴 버튼 모양, 간격 구성
UButton* UPFMenuWidget::CreateMenuButton(UVerticalBox* Layout, FName Name, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(FSlateColorBrush(FLinearColor(0.06f, 0.09f, 0.13f)));
	Style.SetHovered(FSlateColorBrush(FLinearColor(0.10f, 0.24f, 0.33f)));
	Style.SetPressed(FSlateColorBrush(FLinearColor(0.06f, 0.17f, 0.24f)));
	Style.SetNormalPadding(FMargin(20.f, 14.f));
	Style.SetPressedPadding(FMargin(20.f, 14.f));
	Button->SetStyle(Style);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text->SetJustification(ETextJustify::Center);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 24;
	Text->SetFont(Font);
	Button->SetContent(Text);
	Layout->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	return Button;
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
	if (UPFGameInstance* GameInstance = GetGameInstance<UPFGameInstance>())
	{
		SetIsEnabled(false);
		GameInstance->ReturnToMainMenu();
	}
}

// 세션 정리 후 게임 종료
void UPFMenuWidget::HandleExitGame()
{
	if (UPFGameInstance* GameInstance = GetGameInstance<UPFGameInstance>())
	{
		SetIsEnabled(false);
		GameInstance->ExitGame();
	}
}
