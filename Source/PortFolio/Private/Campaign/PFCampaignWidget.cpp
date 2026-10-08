#include "Campaign/PFCampaignWidget.h"
#include "UI/PFWidgetHelpers.h"
#include "Campaign/PFCampaignDirector.h"
#include "Character/PFCharacter.h"
#include "Character/Kwang/PFKwang.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFGameInstance.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UObject/ConstructorHelpers.h"

UPFCampaignWidget::UPFCampaignWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> Layout(TEXT("/Game/GameData/UI/Campaign/WBP_Campaign"));
	LayoutClass = Layout.Class;
}

TSharedRef<SWidget> UPFCampaignWidget::RebuildWidget()
{
	if (!LayoutWidget && LayoutClass && GetOwningPlayer())
	{
		LayoutWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), LayoutClass);
	}
	if (!LayoutWidget) return Super::RebuildWidget();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ObjectivePanel = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("ObjectivePanel"), true);
	EnemyRow = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("EnemyRow"), true);
	RadioPanel = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("RadioPanel"), true);
	PromptPanel = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("PromptPanel"), true);
	BossPanel = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("BossPanel"), true);
	ResultLayer = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("ResultLayer"), true);
	Title = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("TitleText"), true);
	StepText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("StepText"), true);
	Objective = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ObjectiveText"), true);
	EnemyCount = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("EnemyCount"), true);
	Status = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("StatusText"), true);
	Radio = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("RadioText"), true);
	Prompt = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("PromptText"), true);
	BossName = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("BossName"), true);
	BossHealth = PFWidgetHelpers::Find<UProgressBar>(LayoutWidget, TEXT("BossHealth"), true);
	ResultChapter = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ResultChapter"), true);
	ResultTime = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ResultTime"), true);
	ResultDeaths = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ResultDeaths"), true);
	ResultRecords = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ResultRecords"), true);
	if (UButton* ReturnButton = PFWidgetHelpers::Find<UButton>(LayoutWidget, TEXT("ReturnButton")))
	{
		ReturnButton->OnClicked.AddUniqueDynamic(this, &UPFCampaignWidget::ReturnToTitle);
	}
	for (UWidget* Panel : { RadioPanel.Get(), PromptPanel.Get(), BossPanel.Get(), ResultLayer.Get() })
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}
	return LayoutWidget->TakeWidget();
}

void UPFCampaignWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (bReleaseChildren && LayoutWidget) LayoutWidget->ReleaseSlateResources(true);
}

void UPFCampaignWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld());
	if (!LayoutWidget || !Director) return;
	const FPFCampaignProgress& Progress = Director->Progress;
	const bool bComplete = Progress.Phase == EPFCampaignPhase::Complete;
	const int32 StepCount = Director->Definition ? Director->Definition->Steps.Num() : 0;
	const FPFCampaignStep* Step = Director->Definition && Director->Definition->Steps.IsValidIndex(Progress.Step)
		? &Director->Definition->Steps[Progress.Step] : nullptr;
	if (Director->Definition) PFWidgetHelpers::SetTextIfChanged(Title, Director->Definition->ChapterTitle);
	PFWidgetHelpers::SetTextIfChanged(StepText, FText::FromString(FString::Printf(TEXT("진행 %d / %d"), FMath::Min(Progress.Step + 1, StepCount), StepCount)));
	PFWidgetHelpers::SetTextIfChanged(Objective, Director->GetObjectiveText());
	ObjectivePanel->SetVisibility(bComplete ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	FString Detail;
	if (Step && (Step->Type == EPFCampaignObjective::Rally || Step->Type == EPFCampaignObjective::Interact))
		Detail = FString::Printf(TEXT("집결 %d / %d"), Progress.Gathered, Progress.Living);
	if (Progress.Phase == EPFCampaignPhase::Preparing) Detail = TEXT("참가자와 캐릭터 준비 중");
	if (Progress.Phase == EPFCampaignPhase::Starting) Detail = TEXT("플레이어와 적 등장 중");
	if (Progress.Phase == EPFCampaignPhase::Camera) Detail = FString::Printf(TEXT("Enter · 연출 건너뛰기 %d / %d"), Progress.SkipVotes, Progress.Viewers);
	if (Progress.Phase == EPFCampaignPhase::Resetting) Detail = FString::Printf(TEXT("전원 사망 · %.0f초 후 체크포인트에서 재시작"), FMath::Max(0., 5. - (Director->ServerTime() - Progress.PhaseStarted)));
	if (Progress.Phase == EPFCampaignPhase::Fault) Detail = Progress.Error.ToString() + TEXT("\n메뉴에서 타이틀로 돌아갈 수 있습니다.");
	if (Progress.Phase == EPFCampaignPhase::Traveling) Detail = TEXT("다음 챕터로 이동 중");
	APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (Player && Player->IsCampaignWaiting() && Progress.Phase == EPFCampaignPhase::Playing) Detail = TEXT("동료 관전 중 · 다음 체크포인트에서 복귀");
	const bool bShowEnemies = Step && Step->RequiredEncounters.Num() > 0 && Detail.IsEmpty()
		&& Progress.Phase == EPFCampaignPhase::Playing;
	PFWidgetHelpers::SetTextIfChanged(EnemyCount, FText::AsNumber(Progress.RemainingEnemies));
	EnemyRow->SetVisibility(bShowEnemies ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PFWidgetHelpers::SetTextIfChanged(Status, FText::FromString(Detail));
	Status->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	FString Action = Director->GetInteractionText(Player).ToString();
	Action.RemoveFromStart(TEXT("G · "));
	PFWidgetHelpers::SetTextIfChanged(Prompt, FText::FromString(Action));
	PromptPanel->SetVisibility(!bComplete && !Action.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!Progress.Radio.EqualTo(LastRadio))
	{
		LastRadio = Progress.Radio;
		RadioRemaining = 9.f;
		PFWidgetHelpers::SetTextIfChanged(Radio, LastRadio);
	}
	RadioRemaining -= InDeltaTime;
	RadioPanel->SetVisibility(!bComplete && RadioRemaining > 0.f && !LastRadio.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const UPFAttributeSet* BossStats = IsValid(Progress.Boss) ? Progress.Boss->GetAttributeSet() : nullptr;
	BossPanel->SetVisibility(!bComplete && BossStats && BossStats->GetHealth() > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (BossStats)
	{
		PFWidgetHelpers::SetTextIfChanged(BossName, FText::FromString(Cast<APFKwang>(Progress.Boss) ? TEXT("Kwang") : TEXT("Twinblast")));
		BossHealth->SetPercent(BossStats->GetHealth() / FMath::Max(1.f, BossStats->GetMaxHealth()));
	}
	ResultLayer->SetVisibility(bComplete ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bComplete)
	{
		if (Director->Definition) PFWidgetHelpers::SetTextIfChanged(ResultChapter, Director->Definition->ChapterTitle);
		PFWidgetHelpers::SetTextIfChanged(ResultTime, FText::FromString(FString::Printf(TEXT("%02d:%02d"), static_cast<int32>(Progress.Elapsed) / 60, static_cast<int32>(Progress.Elapsed) % 60)));
		PFWidgetHelpers::SetTextIfChanged(ResultDeaths, FText::FromString(FString::Printf(TEXT("%d회"), Progress.Deaths)));
		PFWidgetHelpers::SetTextIfChanged(ResultRecords, FText::FromString(FString::Printf(TEXT("%d / 2"), Progress.Records)));
	}
}

// 기존 세션 정리를 거쳐 타이틀 복귀
void UPFCampaignWidget::ReturnToTitle()
{
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseCampaignResultInput()) return;
	if (UPFGameInstance* Instance = GetGameInstance<UPFGameInstance>()) Instance->ReturnToMainMenu();
}
