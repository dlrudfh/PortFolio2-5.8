#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "PFNaviSubsystem.generated.h"

// 노드 사이 이동 연결
struct FPFNaviEdge
{
	int32 NextNode = INDEX_NONE;
	float Cost = 0.f;
	bool bJump = false;
	FVector JumpVelocity = FVector::ZeroVector;
};

// 경로 탐색 노드
struct FPFNaviNode
{
	bool bActive = false;
	FVector Location = FVector::ZeroVector;
	FVector SurfaceLocation = FVector::ZeroVector;
	FIntPoint NodeIndex = FIntPoint::ZeroValue;
	// 보행, 점프 연결 목록
	TArray<FPFNaviEdge> Edges;
};

class APFCharacter;
class AStaticMeshActor;
class AActor;
class UCharacterMovementComponent;

enum class EPFPathResult : uint8
{
	Failed,
	Partial,
	Complete
};

// 보행, 점프 경로 탐색 클래스
UCLASS()
class PORTFOLIO_API UPFNaviSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	EPFPathResult FindPath(const APFCharacter* Agent, const FVector& StartLocation, const FVector& GoalLocation,
		TArray<int32>& OutNodePath, FVector& OutGoalLocation);

	bool RefreshNaviGraphInBounds(const FBox& ChangedBounds);

	void InvalidateNaviGraph();

	using FNodeRef = int32;

	bool IsValidRef(FNodeRef NodeRef) const;
	int32 GetNeighbourCount(FNodeRef NodeRef) const;
	FNodeRef GetNeighbour(FNodeRef NodeRef, int32 NeighbourIndex) const;
	FVector GetNodeLocation(FNodeRef NodeRef) const;
	float GetTraversalCost(FNodeRef StartNode, FNodeRef EndNode) const;
	bool DoesTraversalRequireJump(FNodeRef StartNode, FNodeRef EndNode) const;
	const FPFNaviEdge* FindEdge(int32 StartNode, int32 EndNode) const;
	bool IsWalkCapsulePathClear(const FVector& StartLocation, const FVector& EndLocation) const;

private:
	bool EnsureNaviGraph(const APFCharacter* Agent);
	bool BuildNaviGraph(const APFCharacter* Agent, const AStaticMeshActor* NaviBoundsActor);
	AStaticMeshActor* FindNaviBoundsActor() const;
	void BuildNaviEdges(const UCharacterMovementComponent* MovementComponent);
	void BuildNaviEdgesForNode(int32 StartNodeIndex);
	int32 GetMaxJumpCellOffset() const;
	bool TryBuildNodeAtCoordinate(const FIntPoint& Coordinate, const FBox& NaviBounds,
		const FCollisionQueryParams& QueryParams, FPFNaviNode& OutNode) const;
	FCollisionQueryParams MakeSurfaceScanQueryParams(const AStaticMeshActor* NaviBoundsActor,
		const AActor* IgnoredActor) const;
	bool CanWalkBetweenNodes(const FPFNaviNode& StartNode, const FPFNaviNode& EndNode) const;
	bool IsContinuousWalkableSurface(const FPFNaviNode& StartNode,
		const FPFNaviNode& EndNode) const;
	bool TryBuildJumpTraversal(const FPFNaviNode& StartNode, const FPFNaviNode& EndNode,
		FVector& OutJumpLaunchVelocity) const;
	bool IsJumpArcClear(const FVector& StartLocation, const FVector& HorizontalVelocity, float JumpZVelocity,
		float GravityMagnitude, float FlightTime) const;
	bool IsCapsulePlacementClear(const FVector& CapsuleCenter) const;
	int32 FindNearestNode(const FVector& WorldLocation) const;
private:
	// 탐색 노드 목록
	TArray<FPFNaviNode> NaviNodes;
	// 격자 좌표별 노드 인덱스
	TMap<FIntPoint, int32> NodeByGridCoordinate;
	float ActualGridSpacing = 100.f;
	// 경로 생성에 사용한 캐릭터 설정
	float CachedCapsuleRadius = 0.f;
	float CachedCapsuleHalfHeight = 0.f;
	float CachedMaxStepHeight = 0.f;
	float CachedJumpZVelocity = 0.f;
	float CachedGravityMagnitude = 0.f;
	float CachedMaxWalkSpeed = 0.f;
	float CachedWalkableFloorZ = 0.f;
	ECollisionChannel CachedAStarTraceChannel = ECC_MAX;
	bool bNaviGraphBuilt = false;
};
