#pragma once

#include "CoreMinimal.h"
#include "Navigation/GeneratedNavLinksProxy.h"
#include "NavAreas/NavArea.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "PFNavLinkProxy.generated.h"

// 자동 생성 점프 영역
UCLASS()
class PORTFOLIO_API UPFNavArea_Jump : public UNavArea
{
	GENERATED_BODY()

public:
	UPFNavArea_Jump();
};

// 봇별 점프 능력, 실패 링크 필터
UCLASS()
class PORTFOLIO_API UPFNavigationQueryFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UPFNavigationQueryFilter();

protected:
	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};

// 자동 생성 링크의 점프 실행
UCLASS()
class PORTFOLIO_API UPFNavLinkProxy : public UGeneratedNavLinksProxy
{
	GENERATED_BODY()

public:
	static void CalculateJump(const FVector& Start, const FVector& End, float WalkSpeed, float JumpSpeed,
		float Gravity, FVector& OutVelocity, float& OutFlightTime);

	virtual UWorld* GetWorld() const override;
	virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const override;
	virtual bool OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint) override;
	virtual void OnLinkMoveFinished(UObject* PathComp) override;
};
