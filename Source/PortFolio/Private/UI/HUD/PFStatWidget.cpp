#include "UI/HUD/PFStatWidget.h"

#include "GAS/Attributes/PFAttributeSet.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"

TSharedRef<SWidget> UPFStatWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		// 스탯 창 배경, 제목 구성
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StatRoot"));
		WidgetTree->RootWidget = RootCanvas;

		StatWindowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatWindow"));
		StatWindowBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.94f));
		StatWindowBorder->SetPadding(FMargin(18.f));
		if (UCanvasPanelSlot* WindowSlot = RootCanvas->AddChildToCanvas(StatWindowBorder))
		{
			WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			WindowSlot->SetPosition(FVector2D::ZeroVector);
			WindowSlot->SetSize(FVector2D(540.f, 510.f));
		}

		UVerticalBox* LayoutBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatLayout"));
		StatWindowBorder->SetContent(LayoutBox);

		UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatTitle"));
		TitleText->SetText(FText::FromString(TEXT("Player Stats")));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.85f, 1.f, 1.f)));
		TitleText->SetJustification(ETextJustify::Center);
		FSlateFontInfo TitleFont = TitleText->GetFont();
		TitleFont.Size = 24;
		TitleText->SetFont(TitleFont);
		if (UVerticalBoxSlot* TitleSlot = LayoutBox->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		// 플레이어 정보, 스탯 행 구성
		UButton* UsernameButton = nullptr;
		UButton* CharacterButton = nullptr;
		UButton* LevelButton = nullptr;
		UButton* StatPointButton = nullptr;

		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("Username"), FText::FromString(TEXT("Username")), UsernameValueText, UsernameButton, false));
		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("Character"), FText::FromString(TEXT("Character")), CharacterValueText, CharacterButton, false));
		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("Level"), FText::FromString(TEXT("Level")), LevelValueText, LevelButton, false));
		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("HP"), FText::FromString(TEXT("HP")), HPValueText, HPIncreaseButton, true));
		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("MP"), FText::FromString(TEXT("MP")), MPValueText, MPIncreaseButton, true));
		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("Damage"), FText::FromString(TEXT("Damage")), DamageValueText, DamageIncreaseButton, true));
		LayoutBox->AddChildToVerticalBox(CreateInfoRow(TEXT("StatPoint"), FText::FromString(TEXT("Stat Point")), StatPointValueText, StatPointButton, false));

		// 강화 버튼 이벤트 연결
		if (HPIncreaseButton)
		{
			HPIncreaseButton->OnClicked.AddDynamic(this, &UPFStatWidget::HandleIncreaseHP);
		}
		if (MPIncreaseButton)
		{
			MPIncreaseButton->OnClicked.AddDynamic(this, &UPFStatWidget::HandleIncreaseMP);
		}
		if (DamageIncreaseButton)
		{
			DamageIncreaseButton->OnClicked.AddDynamic(this, &UPFStatWidget::HandleIncreaseDamage);
		}

		UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatHint"));
		HintText->SetText(FText::FromString(TEXT("Press J to close")));
		HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.67f, 0.72f, 1.f)));
		HintText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* HintSlot = LayoutBox->AddChildToVerticalBox(HintText))
		{
			HintSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
			HintSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	return Super::RebuildWidget();
}

// 스탯 정보 행 생성
UHorizontalBox* UPFStatWidget::CreateInfoRow(
	const FString& RowName,
	const FText& Label,
	UTextBlock*& OutValueText,
	UButton*& OutIncreaseButton,
	bool bCanIncrease)
{
	// 항목 이름, 수치 영역 구성
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("%sRow"), *RowName));

	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *RowName));
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.86f, 0.9f, 1.f)));
	FSlateFontInfo LabelFont = LabelText->GetFont();
	LabelFont.Size = 18;
	LabelText->SetFont(LabelFont);
	if (UHorizontalBoxSlot* LabelSlot = RowBox->AddChildToHorizontalBox(LabelText))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(4.f));
	}

	USizeBox* ValueSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sValueSize"), *RowName));
	ValueSizeBox->SetWidthOverride(230.f);
	OutValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sValue"), *RowName));
	OutValueText->SetText(FText::FromString(TEXT("-")));
	OutValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	OutValueText->SetJustification(ETextJustify::Right);
	OutValueText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	OutValueText->SetClipping(EWidgetClipping::ClipToBounds);
	FSlateFontInfo ValueFont = OutValueText->GetFont();
	ValueFont.Size = 18;
	OutValueText->SetFont(ValueFont);
	ValueSizeBox->SetContent(OutValueText);
	if (UHorizontalBoxSlot* ValueSlot = RowBox->AddChildToHorizontalBox(ValueSizeBox))
	{
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		ValueSlot->SetPadding(FMargin(4.f, 4.f, 10.f, 4.f));
	}

	// 강화 가능한 항목에 버튼 배치
	USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sButtonSize"), *RowName));
	ButtonSizeBox->SetWidthOverride(42.f);
	ButtonSizeBox->SetHeightOverride(42.f);
	if (bCanIncrease)
	{
		OutIncreaseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("%sIncreaseButton"), *RowName));
		OutIncreaseButton->SetBackgroundColor(FLinearColor(0.08f, 0.48f, 0.68f, 1.f));

		UTextBlock* PlusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sPlusText"), *RowName));
		PlusText->SetText(FText::FromString(TEXT("+")));
		PlusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PlusText->SetJustification(ETextJustify::Center);
		FSlateFontInfo PlusFont = PlusText->GetFont();
		PlusFont.Size = 24;
		PlusText->SetFont(PlusFont);
		OutIncreaseButton->SetContent(PlusText);
		ButtonSizeBox->SetContent(OutIncreaseButton);
	}
	else
	{
		OutIncreaseButton = nullptr;
	}

	if (UHorizontalBoxSlot* ButtonSlot = RowBox->AddChildToHorizontalBox(ButtonSizeBox))
	{
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(4.f));
	}

	return RowBox;
}

void UPFStatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetStatWindowVisible(false);
}

void UPFStatWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsStatWindowVisible())
	{
		RefreshStats();
	}
}

// 스탯 창 표시 전환
void UPFStatWidget::SetStatWindowVisible(bool bVisible)
{
	if (!StatWindowBorder)
	{
		return;
	}

	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	StatWindowBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		RefreshStats();
	}
}

// 스탯 창 표시 여부 조회
bool UPFStatWidget::IsStatWindowVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& StatWindowBorder
		&& StatWindowBorder->GetVisibility() != ESlateVisibility::Collapsed;
}

// 플레이어 정보, 스탯 표시 갱신
void UPFStatWidget::RefreshStats()
{
	// 이름, 선택 캐릭터 표시
	if (UsernameValueText)
	{
		UsernameValueText->SetText(FText::FromString(ResolveUsername()));
	}

	APFPlayerState* PFPlayerState = nullptr;
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetOwningPlayer()))
	{
		PFPlayerState = PlayerController->GetPlayerState<APFPlayerState>();
	}

	if (CharacterValueText)
	{
		FString CharacterName = TEXT("Unknown");
		if (PFPlayerState)
		{
			switch (PFPlayerState->GetCharacter())
			{
			case CHARACTER_TWINBLAST:
				CharacterName = TEXT("Twinblast");
				break;
			case CHARACTER_KWANG:
				CharacterName = TEXT("Kwang");
				break;
			default:
				break;
			}
		}
		CharacterValueText->SetText(FText::FromString(CharacterName));
	}

	// GAS 스탯, 강화 가능 여부 반영
	UPFAttributeSet* AttributeSet = GetCurrentAttributeSet();
	if (!AttributeSet)
	{
		if (LevelValueText) LevelValueText->SetText(FText::FromString(TEXT("-")));
		if (HPValueText) HPValueText->SetText(FText::FromString(TEXT("-")));
		if (MPValueText) MPValueText->SetText(FText::FromString(TEXT("-")));
		if (DamageValueText) DamageValueText->SetText(FText::FromString(TEXT("-")));
		if (StatPointValueText) StatPointValueText->SetText(FText::FromString(TEXT("-")));
		if (HPIncreaseButton) HPIncreaseButton->SetIsEnabled(false);
		if (MPIncreaseButton) MPIncreaseButton->SetIsEnabled(false);
		if (DamageIncreaseButton) DamageIncreaseButton->SetIsEnabled(false);
		return;
	}

	if (LevelValueText)
	{
		const int32 CurrentLevel = FMath::Max(1, FMath::FloorToInt(AttributeSet->GetLevel()));
		const int32 CurrentExperience = FMath::Max(0, FMath::RoundToInt(AttributeSet->GetExperience()));
		const int32 RequiredExperience = CurrentLevel * 100;
		LevelValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%d ( %d / %d )"), CurrentLevel, CurrentExperience, RequiredExperience)));
	}

	if (HPValueText)
	{
		HPValueText->SetText(FText::AsNumber(FMath::RoundToInt(AttributeSet->GetMaxHealth())));
	}
	if (MPValueText)
	{
		MPValueText->SetText(FText::AsNumber(FMath::RoundToInt(AttributeSet->GetMaxMana())));
	}
	if (DamageValueText)
	{
		DamageValueText->SetText(FText::AsNumber(FMath::RoundToInt(AttributeSet->GetAttackPower())));
	}

	const int32 CurrentStatPoint = FMath::Max(0, FMath::FloorToInt(AttributeSet->GetStatPoint()));
	if (StatPointValueText)
	{
		StatPointValueText->SetText(FText::AsNumber(CurrentStatPoint));
	}
	const bool bCanIncreaseStat = CurrentStatPoint > 0;
	if (HPIncreaseButton) HPIncreaseButton->SetIsEnabled(bCanIncreaseStat);
	if (MPIncreaseButton) MPIncreaseButton->SetIsEnabled(bCanIncreaseStat);
	if (DamageIncreaseButton) DamageIncreaseButton->SetIsEnabled(bCanIncreaseStat);
}

// 소유 플레이어 스탯 조회
UPFAttributeSet* UPFStatWidget::GetCurrentAttributeSet() const
{
	APFPlayerController* PlayerController = Cast<APFPlayerController>(GetOwningPlayer());
	APFPlayerState* PFPlayerState = PlayerController ? PlayerController->GetPlayerState<APFPlayerState>() : nullptr;
	return PFPlayerState ? PFPlayerState->GetAttributeSet() : nullptr;
}

// 온라인 닉네임, 플레이어 이름 조회
FString UPFStatWidget::ResolveUsername() const
{
	if (const APFPlayerController* PlayerController = Cast<APFPlayerController>(GetOwningPlayer()))
	{
		return PlayerController->GetUsername();
	}

	return TEXT("Offline Player");
}

// 최대 체력 강화 요청
void UPFStatWidget::HandleIncreaseHP()
{
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetOwningPlayer()))
	{
		if (APFPlayerState* PFPlayerState = PlayerController->GetPlayerState<APFPlayerState>())
		{
			PFPlayerState->RequestStatIncrease(EPFStatUpgradeType::MaxHealth);
		}
	}
}

// 최대 마나 강화 요청
void UPFStatWidget::HandleIncreaseMP()
{
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetOwningPlayer()))
	{
		if (APFPlayerState* PFPlayerState = PlayerController->GetPlayerState<APFPlayerState>())
		{
			PFPlayerState->RequestStatIncrease(EPFStatUpgradeType::MaxMana);
		}
	}
}

// 공격력 강화 요청
void UPFStatWidget::HandleIncreaseDamage()
{
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetOwningPlayer()))
	{
		if (APFPlayerState* PFPlayerState = PlayerController->GetPlayerState<APFPlayerState>())
		{
			PFPlayerState->RequestStatIncrease(EPFStatUpgradeType::AttackPower);
		}
	}
}
