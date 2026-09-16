#include "UI/HUD/PFCharacterWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UPFCharacterWidget::RebuildWidget()
{
	TSharedRef<SWidget> HealthWidget = Super::RebuildWidget();

	// 체력바 위에 플레이어 이름 배치
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SAssignNew(UsernameBox, SBox)
			.MaxDesiredWidth(300.f)
			.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			.Clipping(EWidgetClipping::ClipToBounds)
			.Visibility(DisplayUsername.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
			[
				SAssignNew(UsernameText, STextBlock)
				.Text(FText::FromString(DisplayUsername))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowColorAndOpacity(FLinearColor::Black)
				.ShadowOffset(FVector2D(1.f, 1.f))
				.Justification(ETextJustify::Center)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			HealthWidget
		];
}

void UPFCharacterWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	UsernameBox.Reset();
	UsernameText.Reset();
}

// 플레이어 이름, 표시 여부 갱신
void UPFCharacterWidget::SetDisplayUsername(const FString& NewUsername)
{
	if (DisplayUsername == NewUsername)
	{
		return;
	}

	DisplayUsername = NewUsername;
	if (UsernameText.IsValid())
	{
		UsernameText->SetText(FText::FromString(DisplayUsername));
	}
	if (UsernameBox.IsValid())
	{
		UsernameBox->SetVisibility(DisplayUsername.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	}
}

// 스탯 참조, 변경 이벤트 연결
void UPFCharacterWidget::BindAttributeSet(UPFAttributeSet* NewAttributeSet, bool IsLocal)
{
	if (UPFAttributeSet* PreviousAttributeSet = CurrentAttributeSet.Get())
	{
		PreviousAttributeSet->OnHealthChanged.RemoveAll(this);
		PreviousAttributeSet->OnManaChanged.RemoveAll(this);
	}
	CurrentAttributeSet = NewAttributeSet;
	IsLocalPlayer = IsLocal;
	if (NewAttributeSet)
	{
		NewAttributeSet->OnHealthChanged.AddUObject(this, &UPFCharacterWidget::UpdateHPWidget);
		NewAttributeSet->OnManaChanged.AddUObject(this, &UPFCharacterWidget::UpdateMPWidget);
	}
	UpdateAllWidget();
}

void UPFCharacterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 체력바, 로컬 플레이어 수치 위젯 연결
	HPProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PB_HPBar")));
	PFCHECK(HPProgressBar);

	if (IsLocalPlayer)
	{
		HPTextBlock = Cast<UTextBlock>(GetWidgetFromName(TEXT("TEXT_HPBar")));
		PFCHECK(HPTextBlock);
		MPProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PB_MPBar")));
		PFCHECK(MPProgressBar);
		MPTextBlock = Cast<UTextBlock>(GetWidgetFromName(TEXT("TEXT_MPBar")));
		PFCHECK(MPTextBlock);

	}
	
	UpdateAllWidget();
}

// 체력, 마나 UI 갱신
void UPFCharacterWidget::UpdateAllWidget()
{
	UpdateHPWidget();

	if (IsLocalPlayer)
	{
		UpdateMPWidget();
	}
}

// 체력 비율, 수치 갱신
void UPFCharacterWidget::UpdateHPWidget()
{
	if (CurrentAttributeSet.IsValid())
	{
		const float CurHP = CurrentAttributeSet->GetHealth();
		const float MaxHP = CurrentAttributeSet->GetMaxHealth();
		if (HPProgressBar)
		{
			HPProgressBar->SetPercent(MaxHP > 0.f ? CurHP / MaxHP : 0.f);
		}

		if (HPTextBlock && IsLocalPlayer)
		{
			HPTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), (int)CurHP, (int)MaxHP)));
		}
	}
}

// 마나 비율, 수치 갱신
void UPFCharacterWidget::UpdateMPWidget()
{
	if (CurrentAttributeSet.IsValid())
	{
		const float CurMP = CurrentAttributeSet->GetMana();
		const float MaxMP = CurrentAttributeSet->GetMaxMana();
		if (MPProgressBar)
		{
			MPProgressBar->SetPercent(MaxMP > 0.f ? CurMP / MaxMP : 0.f);
		}

		if (MPTextBlock && IsLocalPlayer)
		{
			MPTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), (int)CurMP, (int)MaxMP)));
		}
	}
}
