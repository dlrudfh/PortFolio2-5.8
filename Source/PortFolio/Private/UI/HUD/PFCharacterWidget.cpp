#include "UI/HUD/PFCharacterWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

// 스탯 참조, 변경 이벤트 연결
void UPFCharacterWidget::BindAttributeSet(UPFAttributeSet* NewAttributeSet, bool IsLocal)
{
	PFCHECK(NewAttributeSet);
	CurrentAttributeSet = NewAttributeSet;
	IsLocalPlayer = IsLocal;
	NewAttributeSet->OnHealthChanged.AddUObject(this, &UPFCharacterWidget::UpdateHPWidget);
	NewAttributeSet->OnManaChanged.AddUObject(this, &UPFCharacterWidget::UpdateMPWidget);
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
