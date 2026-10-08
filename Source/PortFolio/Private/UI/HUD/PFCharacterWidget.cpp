#include "UI/HUD/PFCharacterWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"

// 플레이어 이름, 표시 여부 갱신
void UPFCharacterWidget::SetDisplayUsername(const FString& NewUsername)
{
	if (DisplayUsername == NewUsername)
	{
		return;
	}

	DisplayUsername = NewUsername;
	UpdateUsername();
}

// 이름, 표시 여부 반영
void UPFCharacterWidget::UpdateUsername()
{
	if (UsernameText)
	{
		UsernameText->SetText(FText::FromString(DisplayUsername));
	}
	if (UsernameBox)
	{
		UsernameBox->SetVisibility(DisplayUsername.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

// 자원 비율, 수치 표시
void UPFCharacterWidget::UpdateResourceWidgets(UProgressBar* Progress, UTextBlock* Text, float Current, float Maximum)
{
	if (Progress)
	{
		Progress->SetPercent(Maximum > 0.f ? Current / Maximum : 0.f);
	}
	if (Text && IsLocalPlayer)
	{
		Text->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), static_cast<int32>(Current), static_cast<int32>(Maximum))));
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
	// 다른 플레이어 이름 위젯 연결
	UsernameBox = Cast<USizeBox>(GetWidgetFromName(TEXT("UsernameContainer")));
	UsernameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TEXT_Username")));
	UpdateUsername();

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
		UpdateResourceWidgets(HPProgressBar, HPTextBlock, CurrentAttributeSet->GetHealth(), CurrentAttributeSet->GetMaxHealth());
	}
}

// 마나 비율, 수치 갱신
void UPFCharacterWidget::UpdateMPWidget()
{
	if (CurrentAttributeSet.IsValid())
	{
		UpdateResourceWidgets(MPProgressBar, MPTextBlock, CurrentAttributeSet->GetMana(), CurrentAttributeSet->GetMaxMana());
	}
}
