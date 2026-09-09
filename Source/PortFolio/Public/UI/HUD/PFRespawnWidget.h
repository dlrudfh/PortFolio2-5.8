#pragma once

#include "PortFolio/PortFolio.h"
#include "Blueprint/UserWidget.h"
#include "PFRespawnWidget.generated.h"

// 부활 대기 위젯
UCLASS()
class PORTFOLIO_API UPFRespawnWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void StartCountdown(double InRespawnEndServerTime, float InRespawnDuration);
	void StopCountdown();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void UpdateCountdown();

	// 남은 부활 시간 게이지
	UPROPERTY(Transient)
	class UProgressBar* RespawnProgressBar = nullptr;

	double RespawnEndServerTime = 0.0;
	float RespawnDuration = 0.f;
};
