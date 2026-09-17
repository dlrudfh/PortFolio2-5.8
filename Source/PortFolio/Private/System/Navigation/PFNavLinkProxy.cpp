#include "System/Navigation/PFNavLinkProxy.h"

#include "Character/PFCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavMesh/RecastHelpers.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastQueryFilter.h"
#include "System/Framework/PFEnemyAIController.h"
#include "System/Subsystems/PFWorldSubsystem.h"

#if WITH_RECAST
#include "Detour/DetourNavMesh.h"

namespace
{
	// 쿼리, Crowd가 공유하는 불변 점프 조건
	class FPFRecastQueryFilter final : public FRecastQueryFilter
	{
	public:
		float Radius = 0.f;
		float HalfHeight = 0.f;
		float FloorTolerance = 0.f;
		bool bJumpBlocked = true;
		uint8 JumpArea = RECAST_NULL_AREA;
		uint8 DropArea = RECAST_NULL_AREA;
		TSet<FNavLinkId> ExcludedLinks;
		TArray<FPFJumpBlockRegion> BlockRegions;

		virtual INavigationQueryFilterInterface* CreateCopy() const override
		{
			return new FPFRecastQueryFilter(*this);
		}

		virtual bool IsEqual(const INavigationQueryFilterInterface* Other) const override
		{
			return Other == this;
		}

		virtual bool passVirtualFilter(const dtPolyRef Ref, const dtMeshTile* Tile, const dtPoly* Poly) const override
		{
			if (!passInlineFilter(Ref, Tile, Poly))
			{
				return false;
			}
			if (Poly->getType() == DT_POLYTYPE_GROUND)
			{
				return true;
			}
			if (Poly->getType() != DT_POLYTYPE_OFFMESH_POINT)
			{
				return false;
			}
			const int32 ConnectionIndex = static_cast<int32>(Poly - Tile->polys) - Tile->header->offMeshBase;
			if (ConnectionIndex < 0
				|| ConnectionIndex >= Tile->header->offMeshConCount)
			{
				return false;
			}

			const dtOffMeshConnection& Connection = Tile->offMeshCons[ConnectionIndex];
			const bool bDrop = Poly->getArea() == DropArea;
			// 프로젝트 단방향 링크, 재빌드에도 유지되는 식별자
			if (Connection.getIsGenerated() || Connection.getBiDirectional()
				|| (!bDrop && Poly->getArea() != JumpArea)
				|| !FNavLinkId(Connection.userId).IsValid() || ExcludedLinks.Contains(FNavLinkId(Connection.userId)))
			{
				return false;
			}
			const FVector Start = Recast2UnrealPoint(Connection.pos + (Connection.getIsReversed() ? 3 : 0));
			return bDrop || (!bJumpBlocked && !IsJumpStartBlocked(Start));
		}

	private:
		// 출발 캡슐과 효과 영역의 겹침 판정
		bool IsJumpStartBlocked(const FVector& Feet) const
		{
			FBox CapsuleBounds = FBox::BuildAABB(Feet + FVector(0, 0, HalfHeight), FVector(Radius, Radius, HalfHeight));
			CapsuleBounds.Min.Z -= FloorTolerance;
			for (const FPFJumpBlockRegion& Region : BlockRegions)
			{
				if (CapsuleBounds.TransformBy(Region.Transform.ToInverseMatrixWithScale()).Intersect(FBox(-Region.Extent, Region.Extent)))
				{
					return true;
				}
			}
			return false;
		}
	};
}
#endif

UPFNavArea_Jump::UPFNavArea_Jump()
{
	DefaultCost = 1.5f;
	DrawColor = FColor::Orange;
}

UPFNavArea_Drop::UPFNavArea_Drop()
{
	DefaultCost = 1.f;
	DrawColor = FColor::Cyan;
}

UPFNavigationQueryFilter::UPFNavigationQueryFilter()
{
	bInstantiateForQuerier = true;
}

void UPFNavigationQueryFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const
{
#if WITH_RECAST
	if (Cast<ARecastNavMesh>(&NavData))
	{
		FPFRecastQueryFilter Implementation;
		Implementation.JumpArea = NavData.GetAreaID(UPFNavArea_Jump::StaticClass());
		Implementation.DropArea = NavData.GetAreaID(UPFNavArea_Drop::StaticClass());
		if (const auto* Default = static_cast<const FRecastQueryFilter*>(Filter.GetImplementation()))
		{
			static_cast<FRecastQueryFilter&>(Implementation) = *Default;
			Implementation.SetIsVirtual(true);
		}
		const APFEnemyAIController* Controller = Cast<APFEnemyAIController>(Querier);
		const ACharacter* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (Character)
		{
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Implementation.Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			Implementation.HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Implementation.FloorTolerance = Movement->MaxStepHeight;
			Implementation.bJumpBlocked = Controller->IsPathJumpBlocked();
			Controller->GetExcludedJumpLinks(Implementation.ExcludedLinks);
			if (const UPFWorldSubsystem* WorldSubsystem = Character->GetWorld()->GetSubsystem<UPFWorldSubsystem>())
			{
				WorldSubsystem->GetJumpBlockRegions(Implementation.BlockRegions);
			}
		}
		Filter.SetFilterImplementation(&Implementation);
	}
#endif
	Super::InitializeFilter(NavData, Querier, Filter);
}

UWorld* UPFNavLinkProxy::GetWorld() const
{
	const UObject* LinkOwner = GetLinkOwner();
	return LinkOwner ? LinkOwner->GetWorld() : nullptr;
}

bool UPFNavLinkProxy::IsLinkPathfindingAllowed(const UObject* Querier) const
{
	const APFEnemyAIController* Controller = Cast<APFEnemyAIController>(Querier);
	return Controller && !Controller->IsPathJumpBlocked();
}

bool UPFNavLinkProxy::OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint)
{
	UPathFollowingComponent* Following = Cast<UPathFollowingComponent>(PathComp);
	APFEnemyAIController* Controller = Following ? Cast<APFEnemyAIController>(Following->GetOwner()) : nullptr;
	if (Controller)
	{
		Controller->BeginNavigationJump(this, DestPoint);
	}
	else if (Following)
	{
		Following->AbortMove(*this, FPathFollowingResultFlags::InvalidPath);
	}
	return true;
}

void UPFNavLinkProxy::OnLinkMoveFinished(UObject* PathComp)
{
	const UPathFollowingComponent* Following = Cast<UPathFollowingComponent>(PathComp);
	if (APFEnemyAIController* Controller = Following ? Cast<APFEnemyAIController>(Following->GetOwner()) : nullptr)
	{
		Controller->HandleNavigationJumpFinished(this);
	}
}
