#include "UI/HUD/PFRespawnWidget.h"

#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void UPFRespawnWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RespawnProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("RespawnGauge")));
	PFCHECK(RespawnProgressBar);
	UpdateCountdown();
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
