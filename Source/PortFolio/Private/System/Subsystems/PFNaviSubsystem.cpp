#include "System/Subsystems/PFNaviSubsystem.h"


#include "Character/PFCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GraphAStar.h"

namespace PFNaviPrivate
{
	// A* 탐색 조건 클래스
	class FNaviQueryFilter
	{
	public:
		FNaviQueryFilter(const UPFNaviSubsystem& InGraph, bool bInAllowJump)
			: Graph(InGraph), bAllowJump(bInAllowJump)
		{
		}

		// 휴리스틱 배율 반환
		FVector::FReal GetHeuristicScale() const
		{
			return 1.0;
		}

		// 목표까지의 예상 거리 계산
		FVector::FReal GetHeuristicCost(int32 StartNodeRef, int32 EndNodeRef) const
		{
			return FVector::Distance(Graph.GetNodeLocation(StartNodeRef), Graph.GetNodeLocation(EndNodeRef));
		}

		// 그래프의 이동 비용 조회
		FVector::FReal GetTraversalCost(int32 StartNodeRef, int32 EndNodeRef) const
		{
			return Graph.GetTraversalCost(StartNodeRef, EndNodeRef);
		}

		// 점프 허용 여부에 따른 연결 확인
		bool IsTraversalAllowed(int32 StartNodeRef, int32 EndNodeRef) const
		{
			return bAllowJump || !Graph.DoesTraversalRequireJump(StartNodeRef, EndNodeRef);
		}

		// 부분 경로 허용 여부 반환
		bool WantsPartialSolution() const
		{
			return false;
		}

		// 시작 노드 포함 여부 반환
		bool ShouldIncludeStartNodeInPath() const
		{
			return true;
		}

		// 최대 탐색 노드 수 반환
		uint32 GetMaxSearchNodes() const
		{
			return 50000u;
		}

	private:
		const UPFNaviSubsystem& Graph;
		bool bAllowJump = true;
	};

	constexpr float SurfaceClearance = 4.f;
	constexpr float SurfaceScanHeight = 3000.f;
	constexpr float SurfaceScanDepth = 500.f;
	constexpr int32 MaxGridCellsPerAxis = 180;
	constexpr float CapsuleOffset = 4.f;
}

// 목표까지의 보행, 점프 경로 탐색
EPFPathResult UPFNaviSubsystem::FindPath(const APFCharacter* Agent, const FVector& StartLocation,
	const FVector& GoalLocation, TArray<int32>& OutNodePath, FVector& OutGoalLocation)
{
	OutNodePath.Reset();
	OutGoalLocation = FVector::ZeroVector;
	if (!EnsureNaviGraph(Agent))
	{
		return EPFPathResult::Failed;
	}

	const int32 StartNode = FindNearestNode(StartLocation);
	const int32 NearestGoalNode = FindNearestNode(GoalLocation);
	if (!IsValidRef(StartNode))
	{
		return EPFPathResult::Failed;
	}

	// 캐릭터의 점프 차단 상태 반영
	bool bAllowJump = true;
	if (const UAbilitySystemComponent* AbilitySystem = Agent ? Agent->GetAbilitySystemComponent() : nullptr)
	{
		const FGameplayTag JumpBlockTag = FGameplayTag::RequestGameplayTag(FName("Character.Block.Jump"));
		bAllowJump = !AbilitySystem->HasMatchingGameplayTag(JumpBlockTag);
	}

	if (IsValidRef(NearestGoalNode) && StartNode == NearestGoalNode
		&& IsWalkCapsulePathClear(StartLocation, GoalLocation))
	{
		OutNodePath.Add(StartNode);
		OutGoalLocation = GoalLocation;
		return EPFPathResult::Complete;
	}

	const float MaximumHorizontalGoalConnectionDistance = FMath::Max(ActualGridSpacing * 2.5f, 300.f);
	const float MaximumVerticalGoalConnectionDistance = FMath::Max(
		FMath::Square(CachedJumpZVelocity) / FMath::Max(2.f * CachedGravityMagnitude, 1.f),
		CachedMaxStepHeight) + CachedCapsuleHalfHeight;
	// 목표점과 직접 연결 가능한 노드 수집
	TArray<int32> GoalConnectionNodes;
	for (int32 NodeIndex = 0; NodeIndex < NaviNodes.Num(); ++NodeIndex)
	{
		if (!IsValidRef(NodeIndex))
		{
			continue;
		}

		const FVector& NodeLocation = NaviNodes[NodeIndex].Location;
		if (FVector::Dist2D(NodeLocation, GoalLocation) > MaximumHorizontalGoalConnectionDistance
			|| FMath::Abs(NodeLocation.Z - GoalLocation.Z) > MaximumVerticalGoalConnectionDistance)
		{
			continue;
		}

		if (IsWalkCapsulePathClear(NodeLocation, GoalLocation))
		{
			GoalConnectionNodes.Add(NodeIndex);
		}
	}

	PFNaviPrivate::FNaviQueryFilter QueryFilter(*this, bAllowJump);
	TArray<int32> NodePath;
	bool bAppendExactGoalSegment = false;

	// 임시 목표 노드를 연결해 A* 탐색
	if (!GoalConnectionNodes.IsEmpty())
	{
		FPFNaviNode VirtualGoalNodeData;
		VirtualGoalNodeData.bActive = true;
		VirtualGoalNodeData.Location = GoalLocation;
		VirtualGoalNodeData.SurfaceLocation = GoalLocation;
		const int32 VirtualGoalNode = NaviNodes.Add(MoveTemp(VirtualGoalNodeData));

		for (const int32 GoalConnectionNode : GoalConnectionNodes)
		{
			FPFNaviEdge GoalConnectionEdge;
			GoalConnectionEdge.NextNode = VirtualGoalNode;
			GoalConnectionEdge.Cost = FVector::Distance(GetNodeLocation(GoalConnectionNode), GoalLocation);
			NaviNodes[GoalConnectionNode].Edges.Add(GoalConnectionEdge);
		}

		FGraphAStar<UPFNaviSubsystem> ExactGoalPathfinder(*this);
		const EGraphAStarResult ExactGoalSearchResult = ExactGoalPathfinder.FindPath(
			StartNode, VirtualGoalNode, QueryFilter, NodePath);
		if (ExactGoalSearchResult == SearchSuccess && NodePath.Num() >= 2
			&& NodePath.Last() == VirtualGoalNode)
		{
			NodePath.Pop(EAllowShrinking::No);
			bAppendExactGoalSegment = true;
		}
		else
		{
			NodePath.Reset();
		}

		for (const int32 GoalConnectionNode : GoalConnectionNodes)
		{
			TArray<FPFNaviEdge>& Edges = NaviNodes[GoalConnectionNode].Edges;
			if (!Edges.IsEmpty() && Edges.Last().NextNode == VirtualGoalNode)
			{
				Edges.Pop(EAllowShrinking::No);
			}
		}
		// 탐색용 임시 목표 노드 제거
		NaviNodes.Pop(EAllowShrinking::No);
	}

	// 도달 가능한 노드 중 목표에 가까운 대안 탐색
	if (!bAppendExactGoalSegment)
	{
		TBitArray<> bReachableNodes(false, NaviNodes.Num());
		TArray<int32> ReachableNodeQueue;
		ReachableNodeQueue.Reserve(NaviNodes.Num());
		bReachableNodes[StartNode] = true;
		ReachableNodeQueue.Add(StartNode);

		int32 BestReachableGoalNode = INDEX_NONE;
		float BestReachableGoalScore = TNumericLimits<float>::Max();
		for (int32 QueueIndex = 0; QueueIndex < ReachableNodeQueue.Num(); ++QueueIndex)
		{
			const int32 CurrentNode = ReachableNodeQueue[QueueIndex];
			if (!IsValidRef(CurrentNode))
			{
				continue;
			}

			const FVector& CurrentNodeLocation = NaviNodes[CurrentNode].Location;
			const float HorizontalDistance = FVector::Dist2D(CurrentNodeLocation, GoalLocation);
			const float VerticalDistance = FMath::Abs(CurrentNodeLocation.Z - GoalLocation.Z);
			const float GoalScore = FMath::Square(HorizontalDistance) + FMath::Square(VerticalDistance) * 4.f;
			if (GoalScore < BestReachableGoalScore)
			{
				BestReachableGoalScore = GoalScore;
				BestReachableGoalNode = CurrentNode;
			}

			for (const FPFNaviEdge& Edge : NaviNodes[CurrentNode].Edges)
			{
				if (!IsValidRef(Edge.NextNode)
					|| (!bAllowJump && Edge.bJump)
					|| bReachableNodes[Edge.NextNode])
				{
					continue;
				}

				bReachableNodes[Edge.NextNode] = true;
				ReachableNodeQueue.Add(Edge.NextNode);
			}
		}

		if (!IsValidRef(BestReachableGoalNode) || BestReachableGoalNode == StartNode)
		{
			return EPFPathResult::Failed;
		}

		FGraphAStar<UPFNaviSubsystem> FallbackPathfinder(*this);
		const EGraphAStarResult FallbackSearchResult = FallbackPathfinder.FindPath(
			StartNode, BestReachableGoalNode, QueryFilter, NodePath);
		if (FallbackSearchResult != SearchSuccess || NodePath.Num() < 2)
		{
			return EPFPathResult::Failed;
		}
	}

	if (NodePath.IsEmpty())
	{
		return EPFPathResult::Failed;
	}

	// 시작점 진입, 경로 연결 확인
	const FVector FirstNodeLocation = GetNodeLocation(NodePath[0]);
	if (FVector::Dist2D(StartLocation, FirstNodeLocation) > ActualGridSpacing * 0.2f)
	{
		if (!IsWalkCapsulePathClear(StartLocation, FirstNodeLocation))
		{
			return EPFPathResult::Failed;
		}
	}

	for (int32 PathIndex = 1; PathIndex < NodePath.Num(); ++PathIndex)
	{
		const int32 PreviousNode = NodePath[PathIndex - 1];
		const int32 CurrentNode = NodePath[PathIndex];
		if (!FindEdge(PreviousNode, CurrentNode))
		{
			return EPFPathResult::Failed;
		}
	}

	OutGoalLocation = bAppendExactGoalSegment ? GoalLocation : GetNodeLocation(NodePath.Last());
	OutNodePath = MoveTemp(NodePath);
	return bAppendExactGoalSegment ? EPFPathResult::Complete : EPFPathResult::Partial;
}

// 경로 그래프 무효화
void UPFNaviSubsystem::InvalidateNaviGraph()
{
	for (FPFNaviNode& Node : NaviNodes)
	{
		Node.bActive = false;
		Node.Edges.Reset();
	}
	bNaviGraphBuilt = false;
}

// 변경 영역의 노드, 연결 갱신
bool UPFNaviSubsystem::RefreshNaviGraphInBounds(const FBox& ChangedBounds)
{
	if (!ChangedBounds.IsValid)
	{
		return false;
	}

	if (!bNaviGraphBuilt)
	{
		return true;
	}

	if (ActualGridSpacing <= UE_SMALL_NUMBER)
	{
		return false;
	}

	AStaticMeshActor* NaviBoundsActor = FindNaviBoundsActor();
	if (!NaviBoundsActor)
	{
		return false;
	}

	const FBox NaviBounds = NaviBoundsActor->GetComponentsBoundingBox(true);
	if (!NaviBounds.IsValid)
	{
		return false;
	}

	const float GridMinX = NaviBounds.Min.X + CachedCapsuleRadius;
	const float GridMaxX = NaviBounds.Max.X - CachedCapsuleRadius;
	const float GridMinY = NaviBounds.Min.Y + CachedCapsuleRadius;
	const float GridMaxY = NaviBounds.Max.Y - CachedCapsuleRadius;
	if (GridMinX >= GridMaxX || GridMinY >= GridMaxY)
	{
		return false;
	}

	// 변경 영역에 걸친 격자 범위 계산
	const int32 CellCountX = FMath::FloorToInt((GridMaxX - GridMinX) / ActualGridSpacing) + 1;
	const int32 CellCountY = FMath::FloorToInt((GridMaxY - GridMinY) / ActualGridSpacing) + 1;
	const float SampleExpansion = CachedCapsuleRadius + PFNaviPrivate::SurfaceClearance;
	const float SampleMinX = FMath::Max(GridMinX, ChangedBounds.Min.X - SampleExpansion);
	const float SampleMaxX = FMath::Min(GridMaxX, ChangedBounds.Max.X + SampleExpansion);
	const float SampleMinY = FMath::Max(GridMinY, ChangedBounds.Min.Y - SampleExpansion);
	const float SampleMaxY = FMath::Min(GridMaxY, ChangedBounds.Max.Y + SampleExpansion);
	if (SampleMinX > SampleMaxX || SampleMinY > SampleMaxY)
	{
		return true;
	}

	const int32 MinGridX = FMath::Clamp(
		FMath::FloorToInt((SampleMinX - GridMinX) / ActualGridSpacing), 0, CellCountX - 1);
	const int32 MaxGridX = FMath::Clamp(
		FMath::CeilToInt((SampleMaxX - GridMinX) / ActualGridSpacing), 0, CellCountX - 1);
	const int32 MinGridY = FMath::Clamp(
		FMath::FloorToInt((SampleMinY - GridMinY) / ActualGridSpacing), 0, CellCountY - 1);
	const int32 MaxGridY = FMath::Clamp(
		FMath::CeilToInt((SampleMaxY - GridMinY) / ActualGridSpacing), 0, CellCountY - 1);
	const FCollisionQueryParams QueryParams = MakeSurfaceScanQueryParams(NaviBoundsActor, nullptr);

	// 변경된 지면 노드 재생성
	for (int32 GridX = MinGridX; GridX <= MaxGridX; ++GridX)
	{
		for (int32 GridY = MinGridY; GridY <= MaxGridY; ++GridY)
		{
			const FIntPoint Coordinate(GridX, GridY);
			FPFNaviNode UpdatedNode;
			const bool bHasWalkableSurface = TryBuildNodeAtCoordinate(
				Coordinate, NaviBounds, QueryParams, UpdatedNode);
			const int32* ExistingNodeIndexPtr = NodeByGridCoordinate.Find(Coordinate);
			if (ExistingNodeIndexPtr && NaviNodes.IsValidIndex(*ExistingNodeIndexPtr))
			{
				FPFNaviNode& ExistingNode = NaviNodes[*ExistingNodeIndexPtr];
				if (bHasWalkableSurface)
				{
					ExistingNode = MoveTemp(UpdatedNode);
				}
				else
				{
					ExistingNode.bActive = false;
					ExistingNode.Edges.Reset();
				}
				continue;
			}

			if (bHasWalkableSurface)
			{
				const int32 NewNodeIndex = NaviNodes.Add(MoveTemp(UpdatedNode));
				NodeByGridCoordinate.Add(Coordinate, NewNodeIndex);
			}
		}
	}

	// 점프 거리까지 확장해 주변 연결 갱신
	const float EdgeInfluenceDistance = ActualGridSpacing * static_cast<float>(GetMaxJumpCellOffset())
		+ CachedCapsuleRadius + PFNaviPrivate::SurfaceClearance;
	const float AffectedMinX = ChangedBounds.Min.X - EdgeInfluenceDistance;
	const float AffectedMaxX = ChangedBounds.Max.X + EdgeInfluenceDistance;
	const float AffectedMinY = ChangedBounds.Min.Y - EdgeInfluenceDistance;
	const float AffectedMaxY = ChangedBounds.Max.Y + EdgeInfluenceDistance;
	const int32 AffectedMinGridX = FMath::Clamp(
		FMath::FloorToInt((AffectedMinX - GridMinX) / ActualGridSpacing), 0, CellCountX - 1);
	const int32 AffectedMaxGridX = FMath::Clamp(
		FMath::CeilToInt((AffectedMaxX - GridMinX) / ActualGridSpacing), 0, CellCountX - 1);
	const int32 AffectedMinGridY = FMath::Clamp(
		FMath::FloorToInt((AffectedMinY - GridMinY) / ActualGridSpacing), 0, CellCountY - 1);
	const int32 AffectedMaxGridY = FMath::Clamp(
		FMath::CeilToInt((AffectedMaxY - GridMinY) / ActualGridSpacing), 0, CellCountY - 1);

	for (int32 GridX = AffectedMinGridX; GridX <= AffectedMaxGridX; ++GridX)
	{
		for (int32 GridY = AffectedMinGridY; GridY <= AffectedMaxGridY; ++GridY)
		{
			const int32* StartNodeIndexPtr = NodeByGridCoordinate.Find(FIntPoint(GridX, GridY));
			if (!StartNodeIndexPtr || !IsValidRef(*StartNodeIndexPtr))
			{
				continue;
			}

			BuildNaviEdgesForNode(*StartNodeIndexPtr);
		}
	}

	return true;
}

// 활성 노드 여부 확인
bool UPFNaviSubsystem::IsValidRef(FNodeRef NodeRef) const
{
	return NaviNodes.IsValidIndex(NodeRef) && NaviNodes[NodeRef].bActive;
}

// 연결된 노드 수 조회
int32 UPFNaviSubsystem::GetNeighbourCount(FNodeRef NodeRef) const
{
	return IsValidRef(NodeRef) ? NaviNodes[NodeRef].Edges.Num() : 0;
}

// 인접 노드 조회
UPFNaviSubsystem::FNodeRef UPFNaviSubsystem::GetNeighbour(FNodeRef NodeRef,
	int32 NeighbourIndex) const
{
	if (!IsValidRef(NodeRef) || !NaviNodes[NodeRef].Edges.IsValidIndex(NeighbourIndex))
	{
		return INDEX_NONE;
	}

	const int32 NextNode = NaviNodes[NodeRef].Edges[NeighbourIndex].NextNode;
	return IsValidRef(NextNode) ? NextNode : INDEX_NONE;
}

// 노드 위치 조회
FVector UPFNaviSubsystem::GetNodeLocation(FNodeRef NodeRef) const
{
	return IsValidRef(NodeRef) ? NaviNodes[NodeRef].Location : FVector::ZeroVector;
}

// 노드 사이 이동 비용 조회
float UPFNaviSubsystem::GetTraversalCost(FNodeRef StartNode, FNodeRef EndNode) const
{
	if (const FPFNaviEdge* Edge = FindEdge(StartNode, EndNode))
	{
		return Edge->Cost;
	}

	return TNumericLimits<float>::Max();
}

// 이동 구간의 점프 여부 조회
bool UPFNaviSubsystem::DoesTraversalRequireJump(FNodeRef StartNode, FNodeRef EndNode) const
{
	if (const FPFNaviEdge* Edge = FindEdge(StartNode, EndNode))
	{
		return Edge->bJump;
	}

	return false;
}

// 캐릭터 설정에 맞는 그래프 준비
bool UPFNaviSubsystem::EnsureNaviGraph(const APFCharacter* Agent)
{
	if (!Agent || !Agent->GetCapsuleComponent() || !Agent->GetCharacterMovement())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Agent->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = Agent->GetCharacterMovement();
	ECollisionChannel AStarTraceChannel;
	if (!GetCollisionChannel(PFCollisionChannelNames::AStarTrace, AStarTraceChannel))
	{
		return false;
	}

	// 캐릭터 크기, 이동 설정 변경 확인
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float GravityMagnitude = FMath::Abs(Movement->GetGravityZ());
	const bool bAgentSettingsChanged = !FMath::IsNearlyEqual(CachedCapsuleRadius, CapsuleRadius)
		|| !FMath::IsNearlyEqual(CachedCapsuleHalfHeight, CapsuleHalfHeight)
		|| !FMath::IsNearlyEqual(CachedMaxStepHeight, Movement->MaxStepHeight)
		|| !FMath::IsNearlyEqual(CachedJumpZVelocity, Movement->JumpZVelocity)
		|| !FMath::IsNearlyEqual(CachedGravityMagnitude, GravityMagnitude)
		|| !FMath::IsNearlyEqual(CachedMaxWalkSpeed, Movement->MaxWalkSpeed)
		|| !FMath::IsNearlyEqual(CachedWalkableFloorZ, Movement->GetWalkableFloorZ())
		|| CachedAStarTraceChannel != AStarTraceChannel;

	if (bNaviGraphBuilt && !bAgentSettingsChanged)
	{
		return true;
	}

	// 새 설정 보관, 그래프 재생성
	CachedCapsuleRadius = CapsuleRadius;
	CachedCapsuleHalfHeight = CapsuleHalfHeight;
	CachedMaxStepHeight = Movement->MaxStepHeight;
	CachedJumpZVelocity = Movement->JumpZVelocity;
	CachedGravityMagnitude = GravityMagnitude;
	CachedMaxWalkSpeed = Movement->MaxWalkSpeed;
	CachedWalkableFloorZ = Movement->GetWalkableFloorZ();
	CachedAStarTraceChannel = AStarTraceChannel;

	AStaticMeshActor* NaviBoundsActor = FindNaviBoundsActor();
	return NaviBoundsActor && BuildNaviGraph(Agent, NaviBoundsActor);
}

// 지면 격자 기반 그래프 생성
bool UPFNaviSubsystem::BuildNaviGraph(const APFCharacter* Agent,
	const AStaticMeshActor* NaviBoundsActor)
{
	for (FPFNaviNode& Node : NaviNodes)
	{
		Node.bActive = false;
		Node.Edges.Reset();
	}
	bNaviGraphBuilt = false;

	UWorld* World = GetWorld();
	if (!World || !Agent || !NaviBoundsActor)
	{
		return false;
	}

	const FBox Bounds = NaviBoundsActor->GetComponentsBoundingBox(true);
	if (!Bounds.IsValid)
	{
		return false;
	}

	const float MinX = Bounds.Min.X + CachedCapsuleRadius;
	const float MaxX = Bounds.Max.X - CachedCapsuleRadius;
	const float MinY = Bounds.Min.Y + CachedCapsuleRadius;
	const float MaxY = Bounds.Max.Y - CachedCapsuleRadius;
	if (MinX >= MaxX || MinY >= MaxY)
	{
		return false;
	}

	// 탐색 영역에 맞춰 격자 간격 결정
	const float LargestAxisSize = FMath::Max(MaxX - MinX, MaxY - MinY);
	ActualGridSpacing = FMath::Max(100.f, LargestAxisSize / static_cast<float>(PFNaviPrivate::MaxGridCellsPerAxis));
	ActualGridSpacing = FMath::GridSnap(ActualGridSpacing, 10.f);

	const int32 CellCountX = FMath::FloorToInt((MaxX - MinX) / ActualGridSpacing) + 1;
	const int32 CellCountY = FMath::FloorToInt((MaxY - MinY) / ActualGridSpacing) + 1;
	const FCollisionQueryParams QueryParams = MakeSurfaceScanQueryParams(NaviBoundsActor, Agent);

	// 기존 노드 인덱스를 유지하며 지면 재탐색
	bool bHasActiveNode = false;
	for (int32 GridX = 0; GridX < CellCountX; ++GridX)
	{
		for (int32 GridY = 0; GridY < CellCountY; ++GridY)
		{
			FPFNaviNode Node;
			const FIntPoint Coordinate(GridX, GridY);
			if (!TryBuildNodeAtCoordinate(Coordinate, Bounds, QueryParams, Node))
			{
				continue;
			}

			const int32* ExistingNodeIndexPtr = NodeByGridCoordinate.Find(Coordinate);
			if (ExistingNodeIndexPtr && NaviNodes.IsValidIndex(*ExistingNodeIndexPtr))
			{
				NaviNodes[*ExistingNodeIndexPtr] = MoveTemp(Node);
			}
			else
			{
				const int32 NewNodeIndex = NaviNodes.Add(MoveTemp(Node));
				NodeByGridCoordinate.Add(Coordinate, NewNodeIndex);
			}
			bHasActiveNode = true;
		}
	}

	if (!bHasActiveNode)
	{
		return false;
	}

	BuildNaviEdges(Agent->GetCharacterMovement());
	bNaviGraphBuilt = true;
	return true;
}

// 격자 지면에서 이동 노드 생성
bool UPFNaviSubsystem::TryBuildNodeAtCoordinate(const FIntPoint& Coordinate, const FBox& NaviBounds,
	const FCollisionQueryParams& QueryParams, FPFNaviNode& OutNode) const
{
	UWorld* World = GetWorld();
	if (!World || !NaviBounds.IsValid)
	{
		return false;
	}

	// 격자 위치의 지면, 경사 검사
	const float WorldX = NaviBounds.Min.X + CachedCapsuleRadius
		+ static_cast<float>(Coordinate.X) * ActualGridSpacing;
	const float WorldY = NaviBounds.Min.Y + CachedCapsuleRadius
		+ static_cast<float>(Coordinate.Y) * ActualGridSpacing;
	const float TraceTopZ = NaviBounds.Max.Z + PFNaviPrivate::SurfaceScanHeight;
	const float TraceBottomZ = NaviBounds.Min.Z - PFNaviPrivate::SurfaceScanDepth;
	FHitResult SurfaceHit;
	const bool bHitSurface = World->LineTraceSingleByChannel(
		SurfaceHit,
		FVector(WorldX, WorldY, TraceTopZ),
		FVector(WorldX, WorldY, TraceBottomZ),
		CachedAStarTraceChannel,
		QueryParams);
	if (!bHitSurface || SurfaceHit.ImpactNormal.Z < CachedWalkableFloorZ)
	{
		return false;
	}

	// 경사면 위 캡슐 중심 계산
	const float SafeSurfaceNormalZ = FMath::Max(SurfaceHit.ImpactNormal.Z, UE_SMALL_NUMBER);
	const float CapsuleCylinderHalfHeight = FMath::Max(0.f, CachedCapsuleHalfHeight - CachedCapsuleRadius);
	const float CapsuleCenterHeight = CapsuleCylinderHalfHeight
		+ (CachedCapsuleRadius + PFNaviPrivate::SurfaceClearance) / SafeSurfaceNormalZ;
	const FVector CapsuleLocation = SurfaceHit.ImpactPoint + FVector::UpVector * CapsuleCenterHeight;
	if (!IsCapsulePlacementClear(CapsuleLocation))
	{
		return false;
	}

	OutNode = FPFNaviNode();
	OutNode.bActive = true;
	OutNode.NodeIndex = Coordinate;
	OutNode.SurfaceLocation = SurfaceHit.ImpactPoint;
	OutNode.Location = CapsuleLocation;
	return true;
}

// 지면 검사 제외 대상 구성
FCollisionQueryParams UPFNaviSubsystem::MakeSurfaceScanQueryParams(
	const AStaticMeshActor* NaviBoundsActor, const AActor* IgnoredActor) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PFNaviSurfaceScan), true);
	if (IgnoredActor)
	{
		QueryParams.AddIgnoredActor(IgnoredActor);
	}

	UWorld* World = GetWorld();
	if (!World || !NaviBoundsActor)
	{
		return QueryParams;
	}

	// 영역 전체를 덮는 상부 구조물 제외
	const FBox NaviBounds = NaviBoundsActor->GetComponentsBoundingBox(true);
	for (TActorIterator<AStaticMeshActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		AStaticMeshActor* SurfaceActor = *ActorIterator;
		if (!IsValid(SurfaceActor) || SurfaceActor == NaviBoundsActor)
		{
			continue;
		}

		const FBox SurfaceActorBounds = SurfaceActor->GetComponentsBoundingBox(true);
		const bool bCoversNaviBoundsXY = SurfaceActorBounds.IsValid
			&& SurfaceActorBounds.Min.X <= NaviBounds.Min.X + PFNaviPrivate::SurfaceClearance
			&& SurfaceActorBounds.Max.X >= NaviBounds.Max.X - PFNaviPrivate::SurfaceClearance
			&& SurfaceActorBounds.Min.Y <= NaviBounds.Min.Y + PFNaviPrivate::SurfaceClearance
			&& SurfaceActorBounds.Max.Y >= NaviBounds.Max.Y - PFNaviPrivate::SurfaceClearance;
		const bool bIsOverheadCover = SurfaceActorBounds.Min.Z
			> NaviBounds.Max.Z + CachedCapsuleHalfHeight + PFNaviPrivate::SurfaceClearance;
		if (bCoversNaviBoundsXY && bIsOverheadCover)
		{
			QueryParams.AddIgnoredActor(SurfaceActor);
		}
	}

	return QueryParams;
}

// 경로 탐색 영역 액터 조회
AStaticMeshActor* UPFNaviSubsystem::FindNaviBoundsActor() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AStaticMeshActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		AStaticMeshActor* StaticMeshActor = *ActorIterator;
		if (IsValid(StaticMeshActor)
			&& StaticMeshActor->GetActorNameOrLabel() == TEXT("SM_Cube"))
		{
			return StaticMeshActor;
		}
	}

	return nullptr;
}

// 전체 노드의 이동 연결 생성
void UPFNaviSubsystem::BuildNaviEdges(const UCharacterMovementComponent* MovementComponent)
{
	if (!MovementComponent)
	{
		return;
	}

	for (int32 StartNodeIndex = 0; StartNodeIndex < NaviNodes.Num(); ++StartNodeIndex)
	{
		BuildNaviEdgesForNode(StartNodeIndex);
	}
}

// 점프 가능한 격자 탐색 범위 계산
int32 UPFNaviSubsystem::GetMaxJumpCellOffset() const
{
	if (CachedGravityMagnitude <= UE_SMALL_NUMBER || CachedJumpZVelocity <= 0.f)
	{
		return 1;
	}

	const float LevelFlightTime = (2.f * CachedJumpZVelocity) / CachedGravityMagnitude;
	const float MaxHorizontalJumpDistance = CachedMaxWalkSpeed * LevelFlightTime;
	return FMath::Clamp(
		FMath::CeilToInt(MaxHorizontalJumpDistance / FMath::Max(ActualGridSpacing, 1.f)), 1, 8);
}

// 노드의 보행, 점프 연결 생성
void UPFNaviSubsystem::BuildNaviEdgesForNode(int32 StartNodeIndex)
{
	if (!NaviNodes.IsValidIndex(StartNodeIndex))
	{
		return;
	}

	FPFNaviNode& StartNode = NaviNodes[StartNodeIndex];
	StartNode.Edges.Reset();
	if (!StartNode.bActive)
	{
		return;
	}

	const int32 MaxJumpCellOffset = GetMaxJumpCellOffset();

	static const FIntPoint SearchDirections[] =
	{
		FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(0, 1), FIntPoint(-1, 1),
		FIntPoint(-1, 0), FIntPoint(-1, -1), FIntPoint(0, -1), FIntPoint(1, -1)
	};

	// 8방향 보행, 점프 연결 탐색
	for (const FIntPoint& SearchDirection : SearchDirections)
	{
		bool bHasContinuousWalkPath = true;
		int32 PreviousWalkNodeIndex = StartNodeIndex;
		for (int32 CellDistance = 1; CellDistance <= MaxJumpCellOffset; ++CellDistance)
		{
			const FIntPoint TargetCoordinate = StartNode.NodeIndex + SearchDirection * CellDistance;
			const int32* EndNodeIndexPtr = NodeByGridCoordinate.Find(TargetCoordinate);
			if (!EndNodeIndexPtr || !IsValidRef(*EndNodeIndexPtr))
			{
				bHasContinuousWalkPath = false;
				continue;
			}

			const int32 EndNodeIndex = *EndNodeIndexPtr;
			const FPFNaviNode& EndNode = NaviNodes[EndNodeIndex];

			if (bHasContinuousWalkPath
				&& IsValidRef(PreviousWalkNodeIndex)
				&& CanWalkBetweenNodes(NaviNodes[PreviousWalkNodeIndex], EndNode))
			{
				if (CellDistance == 1)
				{
					FPFNaviEdge WalkEdge;
					WalkEdge.NextNode = EndNodeIndex;
					WalkEdge.Cost = FVector::Distance(StartNode.Location, EndNode.Location);
					StartNode.Edges.Add(WalkEdge);
				}

				PreviousWalkNodeIndex = EndNodeIndex;
				continue;
			}

			bHasContinuousWalkPath = false;

			// 보행이 끊긴 구간의 점프 연결 시도
			FVector JumpLaunchVelocity;
			if (TryBuildJumpTraversal(StartNode, EndNode, JumpLaunchVelocity))
			{
				FPFNaviEdge JumpEdge;
				JumpEdge.NextNode = EndNodeIndex;
				JumpEdge.bJump = true;
				JumpEdge.JumpVelocity = JumpLaunchVelocity;
				JumpEdge.Cost = FVector::Distance(StartNode.Location, EndNode.Location) + ActualGridSpacing * 0.25f;
				StartNode.Edges.Add(JumpEdge);
				break;
			}
		}
	}
}

// 노드 사이 보행 가능 여부 확인
bool UPFNaviSubsystem::CanWalkBetweenNodes(const FPFNaviNode& StartNode,
	const FPFNaviNode& EndNode) const
{
	if (!StartNode.bActive || !EndNode.bActive)
	{
		return false;
	}

	if (!IsWalkCapsulePathClear(StartNode.Location, EndNode.Location))
	{
		return false;
	}

	const float SurfaceHeightDifference = FMath::Abs(EndNode.SurfaceLocation.Z - StartNode.SurfaceLocation.Z);
	if (SurfaceHeightDifference <= CachedMaxStepHeight + PFNaviPrivate::SurfaceClearance)
	{
		return true;
	}

	return IsContinuousWalkableSurface(StartNode, EndNode);
}

// 경사면의 연속 보행 가능 여부 확인
bool UPFNaviSubsystem::IsContinuousWalkableSurface(const FPFNaviNode& StartNode,
	const FPFNaviNode& EndNode) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float HorizontalDistance = FVector::Dist2D(StartNode.SurfaceLocation, EndNode.SurfaceLocation);
	if (HorizontalDistance <= UE_SMALL_NUMBER)
	{
		return false;
	}

	// 경사, 계단 높이에 따른 표본 간격 설정
	const float SurfaceSampleSpacing = FMath::Max(25.f, CachedMaxStepHeight * 0.5f);
	const int32 SurfaceSampleCount = FMath::Clamp(
		FMath::CeilToInt(HorizontalDistance / SurfaceSampleSpacing), 2, 32);
	const float SurfaceProbeRange = FMath::Max(25.f, CachedMaxStepHeight)
		+ PFNaviPrivate::SurfaceClearance;
	const float SafeWalkableFloorZ = FMath::Max(CachedWalkableFloorZ, UE_SMALL_NUMBER);
	const float MaximumWalkableSlopeTangent = FMath::Sqrt(
		FMath::Max(0.f, 1.f - FMath::Square(SafeWalkableFloorZ))) / SafeWalkableFloorZ;
	const float ActualSurfaceSampleSpacing = HorizontalDistance / static_cast<float>(SurfaceSampleCount);
	const float AllowedSurfaceHeightChange = FMath::Max(
		CachedMaxStepHeight, ActualSurfaceSampleSpacing * MaximumWalkableSlopeTangent)
		+ PFNaviPrivate::SurfaceClearance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PFNaviSlopeScan), true);
	FVector PreviousSurfaceLocation = StartNode.SurfaceLocation;

	// 구간별 지면 높이, 경사 확인
	for (int32 SampleIndex = 1; SampleIndex < SurfaceSampleCount; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SurfaceSampleCount);
		const FVector ExpectedSurfaceLocation = FMath::Lerp(
			StartNode.SurfaceLocation, EndNode.SurfaceLocation, Alpha);
		FHitResult SurfaceHit;
		const bool bHitSurface = World->LineTraceSingleByChannel(
			SurfaceHit,
			ExpectedSurfaceLocation + FVector::UpVector * SurfaceProbeRange,
			ExpectedSurfaceLocation - FVector::UpVector * SurfaceProbeRange,
			CachedAStarTraceChannel,
			QueryParams);
		if (!bHitSurface || SurfaceHit.ImpactNormal.Z < CachedWalkableFloorZ
			|| FMath::Abs(SurfaceHit.ImpactPoint.Z - PreviousSurfaceLocation.Z) > AllowedSurfaceHeightChange)
		{
			return false;
		}

		PreviousSurfaceLocation = SurfaceHit.ImpactPoint;
	}

	return FMath::Abs(EndNode.SurfaceLocation.Z - PreviousSurfaceLocation.Z) <= AllowedSurfaceHeightChange;
}

// 착지점에 도달할 점프 속도 계산
bool UPFNaviSubsystem::TryBuildJumpTraversal(const FPFNaviNode& StartNode,
	const FPFNaviNode& EndNode, FVector& OutJumpLaunchVelocity) const
{
	OutJumpLaunchVelocity = FVector::ZeroVector;
	if (!StartNode.bActive || !EndNode.bActive
		|| CachedGravityMagnitude <= UE_SMALL_NUMBER || CachedJumpZVelocity <= 0.f || CachedMaxWalkSpeed <= 0.f)
	{
		return false;
	}

	const FVector HorizontalOffset(
		EndNode.Location.X - StartNode.Location.X,
		EndNode.Location.Y - StartNode.Location.Y,
		0.f);
	const float HorizontalDistance = HorizontalOffset.Size();
	if (HorizontalDistance <= UE_SMALL_NUMBER)
	{
		return false;
	}

	// 도달 높이, 비행 시간 계산
	const float VerticalDifference = EndNode.Location.Z - StartNode.Location.Z;
	const float MaximumJumpHeight = FMath::Square(CachedJumpZVelocity) / (2.f * CachedGravityMagnitude);
	const bool bIsDescending = VerticalDifference < 0.f;
	if (!bIsDescending && VerticalDifference > MaximumJumpHeight - PFNaviPrivate::SurfaceClearance)
	{
		return false;
	}

	const float Discriminant = FMath::Square(CachedJumpZVelocity)
		- 2.f * CachedGravityMagnitude * VerticalDifference;
	if (Discriminant < 0.f)
	{
		return false;
	}

	const float FlightTime = (CachedJumpZVelocity + FMath::Sqrt(Discriminant)) / CachedGravityMagnitude;
	if (FlightTime <= UE_SMALL_NUMBER || (!bIsDescending && FlightTime > 2.5f))
	{
		return false;
	}

	// 필요한 수평 속도, 궤적 확인
	const float RequiredHorizontalSpeed = HorizontalDistance / FlightTime;
	if (RequiredHorizontalSpeed > CachedMaxWalkSpeed)
	{
		return false;
	}

	const FVector HorizontalVelocity = HorizontalOffset.GetSafeNormal() * RequiredHorizontalSpeed;
	if (!IsJumpArcClear(StartNode.Location, HorizontalVelocity, CachedJumpZVelocity,
		CachedGravityMagnitude, FlightTime))
	{
		return false;
	}

	OutJumpLaunchVelocity = HorizontalVelocity + FVector::UpVector * CachedJumpZVelocity;
	return true;
}

// 점프 궤적의 캡슐 충돌 검사
bool UPFNaviSubsystem::IsJumpArcClear(const FVector& StartLocation, const FVector& HorizontalVelocity,
	float JumpZVelocity, float GravityMagnitude, float FlightTime) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float TraceRadius = FMath::Max(1.f, CachedCapsuleRadius + PFNaviPrivate::CapsuleOffset);
	const float TraceHalfHeight = FMath::Max(TraceRadius,
		CachedCapsuleHalfHeight + PFNaviPrivate::CapsuleOffset);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(TraceRadius, TraceHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PFNaviJumpArc), true);
	const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(FlightTime / 0.05f), 6, 50);
	const FVector ArcOrigin = StartLocation + FVector::UpVector * PFNaviPrivate::SurfaceClearance;
	FVector PreviousSample = ArcOrigin;

	// 포물선을 나눠 캡슐 스윕 검사
	for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
	{
		const float Time = FlightTime * static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
		const FVector CurrentSample = ArcOrigin
			+ HorizontalVelocity * Time
			+ FVector::UpVector * (JumpZVelocity * Time - 0.5f * GravityMagnitude * FMath::Square(Time));

		FHitResult BlockingHit;
		if (World->SweepSingleByChannel(BlockingHit, PreviousSample, CurrentSample, FQuat::Identity,
			CachedAStarTraceChannel, CapsuleShape, QueryParams))
		{
			return false;
		}

		PreviousSample = CurrentSample;
	}

	return true;
}

// 보행 구간의 캡슐 충돌 검사
bool UPFNaviSubsystem::IsWalkCapsulePathClear(const FVector& StartLocation,
	const FVector& EndLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 양 끝점 중 높은 위치에서 보행 통로 검사
	const float TraceRadius = FMath::Max(1.f, CachedCapsuleRadius + PFNaviPrivate::CapsuleOffset);
	const float TraceHalfHeight = FMath::Max(TraceRadius,
		CachedCapsuleHalfHeight + PFNaviPrivate::CapsuleOffset);
	const float TraceCenterZ = FMath::Max(StartLocation.Z, EndLocation.Z)
		+ PFNaviPrivate::SurfaceClearance;
	const FVector TraceStart(StartLocation.X, StartLocation.Y, TraceCenterZ);
	const FVector TraceEnd(EndLocation.X, EndLocation.Y, TraceCenterZ);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(TraceRadius, TraceHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PFNaviWalk), true);
	FHitResult BlockingHit;
	return !World->SweepSingleByChannel(BlockingHit, TraceStart, TraceEnd, FQuat::Identity,
		CachedAStarTraceChannel, CapsuleShape, QueryParams);
}

// 노드 위치의 캡슐 배치 검사
bool UPFNaviSubsystem::IsCapsulePlacementClear(const FVector& CapsuleCenter) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float TraceRadius = FMath::Max(1.f, CachedCapsuleRadius + PFNaviPrivate::CapsuleOffset);
	const float TraceHalfHeight = FMath::Max(TraceRadius,
		CachedCapsuleHalfHeight + PFNaviPrivate::CapsuleOffset);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(TraceRadius, TraceHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PFNaviPlacement), true);
	const FVector TestLocation = CapsuleCenter + FVector::UpVector * PFNaviPrivate::SurfaceClearance;
	return !World->OverlapBlockingTestByChannel(TestLocation, FQuat::Identity,
		CachedAStarTraceChannel, CapsuleShape, QueryParams);
}

// 높이 차이를 고려한 가까운 노드 탐색
int32 UPFNaviSubsystem::FindNearestNode(const FVector& WorldLocation) const
{
	int32 BestNode = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	const float MaximumHorizontalSnapDistance = FMath::Max(ActualGridSpacing * 2.5f, 300.f);
	const float MaximumVerticalSnapDistance = FMath::Max(
		FMath::Square(CachedJumpZVelocity) / FMath::Max(2.f * CachedGravityMagnitude, 1.f),
		CachedMaxStepHeight) + CachedCapsuleHalfHeight;

	for (int32 NodeIndex = 0; NodeIndex < NaviNodes.Num(); ++NodeIndex)
	{
		if (!IsValidRef(NodeIndex))
		{
			continue;
		}

		const FVector& NodeLocation = NaviNodes[NodeIndex].Location;
		const float HorizontalDistance = FVector::Dist2D(WorldLocation, NodeLocation);
		const float VerticalDistance = FMath::Abs(WorldLocation.Z - NodeLocation.Z);
		if (HorizontalDistance > MaximumHorizontalSnapDistance || VerticalDistance > MaximumVerticalSnapDistance)
		{
			continue;
		}

		const float Score = FMath::Square(HorizontalDistance) + FMath::Square(VerticalDistance) * 4.f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestNode = NodeIndex;
		}
	}

	return BestNode;
}

// 두 노드 사이 이동 연결 조회
const FPFNaviEdge* UPFNaviSubsystem::FindEdge(int32 StartNode, int32 EndNode) const
{
	if (!IsValidRef(StartNode) || !IsValidRef(EndNode))
	{
		return nullptr;
	}

	return NaviNodes[StartNode].Edges.FindByPredicate(
		[EndNode](const FPFNaviEdge& Edge)
		{
			return Edge.NextNode == EndNode;
		});
}
