#include "UI/HUD/PFStatWidget.h"

#include "GAS/Attributes/PFAttributeSet.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFPlayerState.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UPFStatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 스탯 표시, 강화 버튼 연결
	StatWindowBorder = Cast<UBorder>(GetWidgetFromName(TEXT("StatWindow")));
	UsernameValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("UsernameValue")));
	CharacterValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("CharacterValue")));
	LevelValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("LevelValue")));
	HPValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("HPValue")));
	MPValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("MPValue")));
	DamageValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("DamageValue")));
	StatPointValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("StatPointValue")));
	HPIncreaseButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_HPIncrease")));
	MPIncreaseButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_MPIncrease")));
	DamageIncreaseButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_DamageIncrease")));

	if (HPIncreaseButton)
	{
		HPIncreaseButton->OnClicked.AddUniqueDynamic(this, &UPFStatWidget::HandleIncreaseHP);
	}
	if (MPIncreaseButton)
	{
		MPIncreaseButton->OnClicked.AddUniqueDynamic(this, &UPFStatWidget::HandleIncreaseMP);
	}
	if (DamageIncreaseButton)
	{
		DamageIncreaseButton->OnClicked.AddUniqueDynamic(this, &UPFStatWidget::HandleIncreaseDamage);
	}

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
