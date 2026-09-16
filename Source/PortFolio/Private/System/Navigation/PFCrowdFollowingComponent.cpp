#include "System/Navigation/PFCrowdFollowingComponent.h"

#include "NavigationData.h"
#include "Templates/UnrealTemplate.h"

FAIRequestID UPFCrowdFollowingComponent::RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath)
{
	TGuardValue<bool> ReplacementGuard(bReplacingMove,
		InPath.IsValid() && InPath->IsValid() && GetStatus() == EPathFollowingStatus::Moving
		&& !IsFollowingNavLink() && !CurrentCustomLinkOb.IsValid());
	return Super::RequestMove(RequestData, InPath);
}

void UPFCrowdFollowingComponent::OnPathFinished(const FPathFollowingResult& Result)
{
	if (bReplacingMove && !bStopMovementOnFinish && Result.Code == EPathFollowingResult::Aborted
		&& Result.HasFlag(FPathFollowingResultFlags::NewRequest))
	{
		// 새 경로로 교체할 때만 Crowd 속도 초기화 생략
		bReplacingMove = false;
		UPathFollowingComponent::OnPathFinished(Result);
		return;
	}
	Super::OnPathFinished(Result);
}
