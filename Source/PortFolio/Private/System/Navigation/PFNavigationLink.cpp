#include "System/Navigation/PFNavigationLink.h"

#include "NavAreas/NavArea_Null.h"
#include "Navigation/PathFollowingComponent.h"
#include "System/Framework/PFEnemyAIController.h"
#include "System/Navigation/PFNavLinkProxy.h"

void UPFNavigationLinkComponent::OnRegister()
{
	SetMoveReachedLink(this, &UPFNavigationLinkComponent::HandleMoveReached);
	Super::OnRegister();
}

bool UPFNavigationLinkComponent::IsLinkPathfindingAllowed(const UObject* Querier) const
{
	const APFEnemyAIController* Controller = Cast<APFEnemyAIController>(Querier);
	const APFNavigationLink* Link = Cast<APFNavigationLink>(GetOwner());
	return Controller && Link && (Link->GetTraversal() == EPFNavigationTraversal::Drop || !Controller->IsPathJumpBlocked());
}

// 경로 추종을 프로젝트 이동 처리에 연결
void UPFNavigationLinkComponent::HandleMoveReached(UNavLinkCustomComponent* Link, UObject* PathComp, const FVector& Destination)
{
	UPathFollowingComponent* Following = Cast<UPathFollowingComponent>(PathComp);
	if (APFEnemyAIController* Controller = Following ? Cast<APFEnemyAIController>(Following->GetOwner()) : nullptr)
	{
		Controller->BeginNavigationJump(this, Destination);
	}
	else if (Following)
	{
		Following->AbortMove(*this, FPathFollowingResultFlags::InvalidPath);
	}
}

void UPFNavigationLinkComponent::OnLinkMoveFinished(UObject* PathComp)
{
	Super::OnLinkMoveFinished(PathComp);
	const UPathFollowingComponent* Following = Cast<UPathFollowingComponent>(PathComp);
	if (APFEnemyAIController* Controller = Following ? Cast<APFEnemyAIController>(Following->GetOwner()) : nullptr)
	{
		Controller->HandleNavigationJumpFinished(this);
	}
}

APFNavigationLink::APFNavigationLink(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPFNavigationLinkComponent>(TEXT("SmartLinkComp")))
{
	PointLinks.Reset();
	bSmartLinkIsRelevant = true;
	GetSmartLinkComp()->SetNavigationRelevancy(true);
	GetSmartLinkComp()->SetEnabledArea(UPFNavArea_Jump::StaticClass());
	GetSmartLinkComp()->SetDisabledArea(UNavArea_Null::StaticClass());
}

// 생성 결과를 저장 가능한 Smart Link에 반영
void APFNavigationLink::Configure(const FGuid& OwnerId, const FString& Key, const FVector& Start, const FVector& End,
	EPFNavigationTraversal Mode)
{
	Modify();
	GeneratorId = OwnerId;
	GenerationKey = Key;
	StartFeet = Start;
	EndFeet = End;
	Traversal = Mode;
	PointLinks.Reset();
	UNavLinkCustomComponent* Link = GetSmartLinkComp();
	Link->Modify();
	Link->SetEnabledArea(Mode == EPFNavigationTraversal::Drop ? UPFNavArea_Drop::StaticClass() : UPFNavArea_Jump::StaticClass());
	Link->SetEnabled(true);
	Link->SetLinkData(GetActorTransform().InverseTransformPosition(Start), GetActorTransform().InverseTransformPosition(End),
		ENavLinkDirection::LeftToRight);
	MarkPackageDirty();
}
