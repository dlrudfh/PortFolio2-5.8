#pragma once

#include "CoreMinimal.h"
#include "Navigation/GeneratedNavLinksProxy.h"
#include "NavAreas/NavArea.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "PFNavLinkProxy.generated.h"

// 점프 이동 영역
UCLASS()
class PORTFOLIO_API UPFNavArea_Jump : public UNavArea
{
	GENERATED_BODY()

public:
	UPFNavArea_Jump();
};

// 위로 도약하지 않는 보행, 낙하 영역
UCLASS()
class PORTFOLIO_API UPFNavArea_Drop : public UNavArea
{
	GENERATED_BODY()

public:
	UPFNavArea_Drop();
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
	virtual UWorld* GetWorld() const override;
	virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const override;
	virtual bool OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint) override;
	virtual void OnLinkMoveFinished(UObject* PathComp) override;
};
