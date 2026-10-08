#include "UI/HUD/PFStatWidget.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"

#include "Character/PFCharacter.h"
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
	checkf(StatWindowBorder && UsernameValueText && CharacterValueText && LevelValueText && HPValueText
		&& MPValueText && DamageValueText && StatPointValueText && HPIncreaseButton && MPIncreaseButton
		&& DamageIncreaseButton, TEXT("Required stat widgets are missing"));

	HPIncreaseButton->OnClicked.AddUniqueDynamic(this, &UPFStatWidget::HandleIncreaseHP);
	MPIncreaseButton->OnClicked.AddUniqueDynamic(this, &UPFStatWidget::HandleIncreaseMP);
	DamageIncreaseButton->OnClicked.AddUniqueDynamic(this, &UPFStatWidget::HandleIncreaseDamage);

	SetStatWindowVisible(false);
	BindPlayerState(GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APFPlayerState>() : nullptr);
}

void UPFStatWidget::NativeDestruct()
{
	UnbindPlayerState();
	Super::NativeDestruct();
}

// 플레이어 정보, 스탯 변경 이벤트 연결
void UPFStatWidget::BindPlayerState(APFPlayerState* PlayerState)
{
	UPFAttributeSet* Attributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	if (BoundPlayerState.Get() == PlayerState && BoundAttributeSet.Get() == Attributes) return;
	UnbindPlayerState();
	BoundPlayerState = PlayerState;
	BoundAttributeSet = Attributes;
	if (PlayerState) PlayerState->OnPlayerInfoChanged.AddUObject(this, &UPFStatWidget::RefreshIdentity);
	if (Attributes) Attributes->OnStatsChanged.AddUObject(this, &UPFStatWidget::RefreshStats);
	RefreshIdentity();
	RefreshStats();
}

// 이전 표시 대상의 이벤트 해제
void UPFStatWidget::UnbindPlayerState()
{
	if (BoundPlayerState.IsValid()) BoundPlayerState->OnPlayerInfoChanged.RemoveAll(this);
	if (BoundAttributeSet.IsValid()) BoundAttributeSet->OnStatsChanged.RemoveAll(this);
	BoundPlayerState.Reset();
	BoundAttributeSet.Reset();
}

// 이름, 선택 캐릭터 표시 갱신
void UPFStatWidget::RefreshIdentity()
{
	if (!IsStatWindowVisible()) return;
	UsernameValueText->SetText(FText::FromString(ResolveUsername()));
	const APFPlayerState* State = BoundPlayerState.Get();
	const FPFCharacterDefinition* Definition = State ? UPFGameInstanceSubsystem::GetCharacterDefinition(State->GetCharacter()) : nullptr;
	CharacterValueText->SetText(FText::FromString(Definition ? Definition->Name : TEXT("Unknown")));
}

// 스탯 창 표시 전환
void UPFStatWidget::SetStatWindowVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	StatWindowBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		BindPlayerState(GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APFPlayerState>() : nullptr);
		RefreshIdentity();
		RefreshStats();
	}
}

// 스탯 창 표시 여부 조회
bool UPFStatWidget::IsStatWindowVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& StatWindowBorder->GetVisibility() != ESlateVisibility::Collapsed;
}

// 스탯 수치, 강화 가능 여부 갱신
void UPFStatWidget::RefreshStats()
{
	if (!IsStatWindowVisible()) return;

	// GAS 스탯, 강화 가능 여부 반영
	UPFAttributeSet* AttributeSet = GetCurrentAttributeSet();
	if (!AttributeSet)
	{
		LevelValueText->SetText(FText::FromString(TEXT("-")));
		HPValueText->SetText(FText::FromString(TEXT("-")));
		MPValueText->SetText(FText::FromString(TEXT("-")));
		DamageValueText->SetText(FText::FromString(TEXT("-")));
		StatPointValueText->SetText(FText::FromString(TEXT("-")));
		HPIncreaseButton->SetIsEnabled(false);
		MPIncreaseButton->SetIsEnabled(false);
		DamageIncreaseButton->SetIsEnabled(false);
		return;
	}

	const int32 CurrentLevel = FMath::Max(1, FMath::FloorToInt(AttributeSet->GetLevel()));
	const int32 CurrentExperience = FMath::Max(0, FMath::RoundToInt(AttributeSet->GetExperience()));
	const int32 RequiredExperience = FMath::RoundToInt(UPFAttributeSet::GetRequiredExperience(CurrentLevel));
	LevelValueText->SetText(FText::FromString(FString::Printf(
		TEXT("%d ( %d / %d )"), CurrentLevel, CurrentExperience, RequiredExperience)));

	HPValueText->SetText(FText::AsNumber(FMath::RoundToInt(AttributeSet->GetMaxHealth())));
	MPValueText->SetText(FText::AsNumber(FMath::RoundToInt(AttributeSet->GetMaxMana())));
	const APFCharacter* Character = Cast<APFCharacter>(GetOwningPlayerPawn());
	DamageValueText->SetText(FText::AsNumber(Character ? Character->GetDamage() : AttributeSet->GetAttackPower()));

	const int32 CurrentStatPoint = FMath::Max(0, FMath::FloorToInt(AttributeSet->GetStatPoint()));
	StatPointValueText->SetText(FText::AsNumber(CurrentStatPoint));
	const bool bCanIncreaseStat = CurrentStatPoint > 0;
	HPIncreaseButton->SetIsEnabled(bCanIncreaseStat);
	MPIncreaseButton->SetIsEnabled(bCanIncreaseStat);
	DamageIncreaseButton->SetIsEnabled(bCanIncreaseStat);
}

// 소유 플레이어 스탯 조회
UPFAttributeSet* UPFStatWidget::GetCurrentAttributeSet() const
{
	return BoundAttributeSet.Get();
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

// 소유 플레이어의 스탯 강화 요청
void UPFStatWidget::RequestStatIncrease(EPFStatUpgradeType UpgradeType)
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APFPlayerState* PlayerState = PlayerController->GetPlayerState<APFPlayerState>())
		{
			PlayerState->RequestStatIncrease(UpgradeType);
		}
	}
}

// 최대 체력 강화 요청
void UPFStatWidget::HandleIncreaseHP()
{
	RequestStatIncrease(EPFStatUpgradeType::MaxHealth);
}

// 최대 마나 강화 요청
void UPFStatWidget::HandleIncreaseMP()
{
	RequestStatIncrease(EPFStatUpgradeType::MaxMana);
}

// 공격력 강화 요청
void UPFStatWidget::HandleIncreaseDamage()
{
	RequestStatIncrease(EPFStatUpgradeType::AttackPower);
}
