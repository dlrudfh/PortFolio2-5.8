#pragma once

#include "Navigation/CrowdFollowingComponent.h"
#include "PFCrowdFollowingComponent.generated.h"

// 경로 교체 시 Crowd 이동 속도 유지
UCLASS()
class PORTFOLIO_API UPFCrowdFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()

public:
	virtual FAIRequestID RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath) override;
	virtual void OnPathFinished(const FPathFollowingResult& Result) override;

private:
	bool bReplacingMove = false;
};
