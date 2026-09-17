#include "System/Navigation/PFNavigationLinkBuilder.h"

#if WITH_EDITOR && WITH_RECAST
#include "AI/Navigation/NavQueryFilter.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/ScopedSlowTask.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastQueryFilter.h"
#include "System/Navigation/PFNavigationLink.h"
#include "System/Navigation/PFNavigationTraversal.h"

namespace
{
	constexpr double BucketSize = 512.0;

	struct FSurface
	{
		NavNodeRef Ref = INVALID_NAVNODEREF;
		FVector Center = FVector::ZeroVector;
		FBox Bounds = FBox(ForceInit);
		TArray<FVector> Vertices;
	};

	struct FBoundary
	{
		FVector A;
		FVector B;
		FVector Outward;
		int32 Group = INDEX_NONE;
	};

	struct FSample
	{
		FVector Point;
		FVector Outward;
		int32 Group = INDEX_NONE;
		double Order = 0;
	};

	struct FCandidate
	{
		FVector Point;
		double DistanceSquared = 0;
	};

	// 출발 액터, 도착 액터, 수평 방향
	struct FActorDirectionKey
	{
		const AActor* Source = nullptr;
		const AActor* Destination = nullptr;
		int32 Direction = 0;

		// 동일한 액터 간 이동 방향
		bool operator==(const FActorDirectionKey& Other) const
		{
			return Source == Other.Source && Destination == Other.Destination && Direction == Other.Direction;
		}
	};

	struct FGeneratedLink
	{
		FVector Start;
		FVector End;
		FVector LastStart;
		FVector NavStart;
		FVector NavEnd;
		int32 Group = INDEX_NONE;
		int32 LastSample = INDEX_NONE;
		EPFNavigationTraversal Mode = EPFNavigationTraversal::Jump;
		FActorDirectionKey ActorDirection;
	};

	// 수평 이동을 90도 간격의 네 방향으로 구분
	int32 GetDirectionSector(const FVector& Start, const FVector& End)
	{
		const FVector Delta = End - Start;
		const double Degrees = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		return (FMath::RoundToInt(Degrees / 90.0) + 4) % 4;
	}

	// 공간 인덱스 좌표
	FIntPoint Bucket(const FVector& Point)
	{
		return FIntPoint(FMath::FloorToInt(Point.X / BucketSize), FMath::FloorToInt(Point.Y / BucketSize));
	}

	// 경계 접점, 생성 결과의 안정적인 위치 키
	FIntVector PositionKey(const FVector& Point)
	{
		return FIntVector(FMath::RoundToInt(Point.X), FMath::RoundToInt(Point.Y), FMath::RoundToInt(Point.Z));
	}

	// 링크 없는 직선 보행 연결
	bool CanWalkDirect(ARecastNavMesh& NavMesh, const FVector& A, const FVector& B,
		const FSharedConstNavQueryFilter& Filter, const ACharacter& Bot)
	{
		return NavMesh.IsSegmentOnNavmesh(A, B, Filter, &Bot);
	}

	// 인접 경계 그룹에 남은 유사 링크 정리
	int32 CompactLinks(TArray<FGeneratedLink>& Links, ARecastNavMesh& NavMesh,
		const FSharedConstNavQueryFilter& Filter, const ACharacter& Bot, float MergeDistance)
	{
		Links.StableSort([](const FGeneratedLink& A, const FGeneratedLink& B)
		{
			if (A.Mode != B.Mode)
			{
				return A.Mode == EPFNavigationTraversal::Drop;
			}
			return FVector::DistSquared2D(A.Start, A.End) < FVector::DistSquared2D(B.Start, B.End);
		});
		TArray<FGeneratedLink> Kept;
		TMap<FIntPoint, TArray<int32>> ByStart;
		TMap<FIntPoint, TArray<int32>> ByEnd;
		const double MergeDistanceSquared = FMath::Square(MergeDistance);
		for (const FGeneratedLink& Link : Links)
		{
			auto HasDuplicateNearby = [&](const FVector& Point, const TMap<FIntPoint, TArray<int32>>& ByPoint)
			{
				const FIntPoint Min = Bucket(Point - FVector(MergeDistance, MergeDistance, 0));
				const FIntPoint Max = Bucket(Point + FVector(MergeDistance, MergeDistance, 0));
				for (int32 X = Min.X; X <= Max.X; ++X)
				{
					for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
					{
						const TArray<int32>* Nearby = ByPoint.Find(FIntPoint(X, Y));
						if (!Nearby)
						{
							continue;
						}
						for (int32 Index : *Nearby)
						{
							const FGeneratedLink& Previous = Kept[Index];
							if (Link.ActorDirection.Source != Previous.ActorDirection.Source
								|| Link.ActorDirection.Destination != Previous.ActorDirection.Destination
								|| (FVector::DistSquared2D(Link.Start, Previous.Start) > MergeDistanceSquared
									&& FVector::DistSquared2D(Link.End, Previous.End) > MergeDistanceSquared))
							{
								continue;
							}
							if (CanWalkDirect(NavMesh, Link.NavStart, Previous.NavStart, Filter, Bot)
								&& CanWalkDirect(NavMesh, Link.NavEnd, Previous.NavEnd, Filter, Bot))
							{
								return true;
							}
						}
					}
				}
				return false;
			};
			if (!HasDuplicateNearby(Link.Start, ByStart) && !HasDuplicateNearby(Link.End, ByEnd))
			{
				const int32 Index = Kept.Add(Link);
				ByStart.FindOrAdd(Bucket(Link.Start)).Add(Index);
				ByEnd.FindOrAdd(Bucket(Link.End)).Add(Index);
			}
		}
		const int32 Removed = Links.Num() - Kept.Num();
		Links = MoveTemp(Kept);
		return Removed;
	}

	// 일반 바닥 위의 엄폐물 모서리 점프 제외
	bool HasContinuousFloor(UWorld& World, const FVector& Start, const FVector& End,
		const FPFTraversalSettings& Settings, float Spacing)
	{
		if (FMath::Abs(Start.Z - End.Z) > Settings.StepHeight)
		{
			return false;
		}
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(Start, End) / Spacing));
		for (int32 Index = 0; Index <= Steps; ++Index)
		{
			const FVector Point = FMath::Lerp(Start, End, static_cast<double>(Index) / Steps);
			FHitResult Hit;
			if (!PFNavigationTraversal::FindFloor(World, Point, Settings, Settings.StepHeight + 2.f, Hit)
				|| FMath::Abs(Hit.ImpactPoint.Z + PFNavigationTraversal::FloorClearance - Point.Z) > Settings.StepHeight)
			{
				return false;
			}
		}
		return true;
	}

	// 명시적으로 제외한 엄폐물 주변
	bool HasNoJumpTag(UWorld& World, const FVector& Start, const FVector& End, const FPFTraversalSettings& Settings)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(PFNoAutoJump), false, Settings.IgnoredActor);
		TArray<FOverlapResult> Hits;
		for (const FVector& Point : { Start, End })
		{
			Hits.Reset();
			World.OverlapMultiByChannel(Hits, Point + FVector(0, 0, Settings.StepHeight), FQuat::Identity, Settings.Channel,
				FCollisionShape::MakeBox(FVector(Settings.Radius * 2.f, Settings.Radius * 2.f, Settings.StepHeight + 2.f)),
				Query, Settings.Responses);
			for (const FOverlapResult& Hit : Hits)
			{
				if ((Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("NoAutoJump")))
					|| (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("NoAutoJump"))))
				{
					return true;
				}
			}
		}
		return false;
	}

	// 타일 경계로 나뉜 동일 출발 구간 결합
	void GroupBoundaries(TArray<FBoundary>& Boundaries)
	{
		TArray<int32> Parents;
		TMap<FIntVector, TArray<int32>> AtVertex;
		for (int32 Index = 0; Index < Boundaries.Num(); ++Index)
		{
			Parents.Add(Index);
		}
		auto FindRoot = [&Parents](int32 Index)
		{
			while (Parents[Index] != Index)
			{
				Parents[Index] = Parents[Parents[Index]];
				Index = Parents[Index];
			}
			return Index;
		};
		for (int32 Index = 0; Index < Boundaries.Num(); ++Index)
		{
			const FBoundary& Edge = Boundaries[Index];
			for (const FVector& Point : { Edge.A, Edge.B })
			{
				TArray<int32>& Neighbors = AtVertex.FindOrAdd(PositionKey(Point));
				for (int32 Other : Neighbors)
				{
					const FBoundary& Previous = Boundaries[Other];
					if (FVector::DotProduct(Edge.Outward, Previous.Outward) > 0.999
						&& FMath::Abs(Edge.A.Z - Previous.A.Z) <= 2.f
						&& FMath::Min(FVector::DistSquared(Point, Previous.A), FVector::DistSquared(Point, Previous.B)) <= 1.f)
					{
						Parents[FindRoot(Index)] = FindRoot(Other);
					}
				}
				Neighbors.Add(Index);
			}
		}
		for (int32 Index = 0; Index < Boundaries.Num(); ++Index)
		{
			Boundaries[Index].Group = FindRoot(Index);
		}
	}

	// 실제 바닥 높이, 캡슐 공간 확인
	bool ResolveFeet(UWorld& World, const FVector& NavPoint, const FPFTraversalSettings& Settings,
		float HeightTolerance, FVector& OutFeet, const AActor*& OutFloorActor)
	{
		OutFloorActor = nullptr;
		FHitResult Hit;
		if (!PFNavigationTraversal::FindFloor(World, NavPoint, Settings, HeightTolerance, Hit))
		{
			return false;
		}
		const float SlopeOffset = Settings.Radius * (1.f / FMath::Max(Hit.ImpactNormal.Z, 0.1) - 1.f);
		OutFeet = Hit.ImpactPoint + FVector(0, 0, SlopeOffset + PFNavigationTraversal::FloorClearance);
		OutFloorActor = Hit.GetActor();
		return OutFloorActor && PFNavigationTraversal::HasClearance(World, OutFeet, Settings);
	}
}
#endif

// NavMesh 경계에서 보행 낙하, 점프 링크 생성
bool PFNavigationLinkBuilder::Build(UWorld& World, ARecastNavMesh& NavMesh, const ACharacter& Bot, ULevel* Level,
	const FGuid& GeneratorId, FString& OutResult)
{
#if WITH_EDITOR && WITH_RECAST
	const FPFTraversalSettings Settings = PFNavigationTraversal::GetSettings(Bot, World);
	const float CellSize = NavMesh.GetCellSize(ENavigationDataResolution::Default);
	const float Spacing = FMath::Max(5.f, FMath::Min(CellSize, Settings.Radius));
	const float HeightTolerance = FMath::Max(4.f, NavMesh.GetCellHeight(ENavigationDataResolution::Default) * 2.f);
	FSharedNavQueryFilter MutableFilter = NavMesh.GetDefaultQueryFilter()->GetCopy();
	MutableFilter->SetFilterImplementation(ARecastNavMesh::GetNamedFilter(ERecastNamedFilter::FilterOutNavLinks));
	const FSharedConstNavQueryFilter GroundFilter = MutableFilter;
	TArray<FSurface> Surfaces;
	TArray<FBoundary> Boundaries;
	TMap<FIntPoint, TArray<int32>> Buckets;
	FBox Bounds(ForceInit);
	TArray<FNavTileRef> Tiles;
	NavMesh.GetAllNavMeshTiles(Tiles);
	for (FNavTileRef Tile : Tiles)
	{
		TArray<FNavPoly> Polys;
		NavMesh.GetPolysInTile(Tile, Polys);
		for (const FNavPoly& Poly : Polys)
		{
			FNavMeshNodeFlags Flags;
			if (!NavMesh.GetPolyFlags(Poly.Ref, Flags) || Flags.IsNavLink() || Flags.Area == RECAST_NULL_AREA)
			{
				continue;
			}
			FSurface Surface;
			Surface.Ref = Poly.Ref;
			Surface.Center = Poly.Center;
			if (!NavMesh.GetPolyVerts(Poly.Ref, Surface.Vertices) || Surface.Vertices.Num() < 3)
			{
				continue;
			}
			for (const FVector& Vertex : Surface.Vertices)
			{
				Surface.Bounds += Vertex;
			}
			Bounds += Surface.Bounds;
			const int32 SurfaceIndex = Surfaces.Add(MoveTemp(Surface));
			const FSurface& Added = Surfaces.Last();
			const FIntPoint Min = Bucket(Added.Bounds.Min);
			const FIntPoint Max = Bucket(Added.Bounds.Max);
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
				{
					Buckets.FindOrAdd(FIntPoint(X, Y)).Add(SurfaceIndex);
				}
			}
			TArray<FNavigationPortalEdge> Walls;
			NavMesh.GetPolyWallSegments(Poly.Ref, GroundFilter, &Bot, Walls);
			for (const FNavigationPortalEdge& Wall : Walls)
			{
				if (Wall.ToRef != INVALID_NAVNODEREF || FVector::DistSquared2D(Wall.Left, Wall.Right) < 1.f)
				{
					continue;
				}
				const FVector Along = (Wall.Right - Wall.Left).GetSafeNormal2D();
				FVector Outward(-Along.Y, Along.X, 0);
				if (FVector::DotProduct(Outward, (Wall.Left + Wall.Right) * 0.5 - Poly.Center) < 0)
				{
					Outward *= -1;
				}
				Boundaries.Add({ Wall.Left, Wall.Right, Outward, INDEX_NONE });
			}
		}
	}
	if (Surfaces.IsEmpty())
	{
		OutResult = TEXT("No built ground polygons. Existing project links were preserved.");
		return false;
	}
	GroupBoundaries(Boundaries);
	TArray<FSample> Samples;
	for (const FBoundary& Edge : Boundaries)
	{
		const int32 Count = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(Edge.A, Edge.B) / Spacing));
		const FVector Along(-Edge.Outward.Y, Edge.Outward.X, 0);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Point = FMath::Lerp(Edge.A, Edge.B, (Index + 0.5) / Count) - Edge.Outward * 2.f;
			Samples.Add({ Point, Edge.Outward, Edge.Group, FVector::DotProduct(Point, Along) });
		}
	}
	Samples.Sort([](const FSample& A, const FSample& B)
	{
		return A.Group == B.Group ? A.Order < B.Order : A.Group < B.Group;
	});
	TArray<FGeneratedLink> Results;
	int32 CheckedCandidates = 0;
	FScopedSlowTask Progress(Samples.Num(), NSLOCTEXT("PFNavigation", "BuildLinks", "Generating navigation traversal links"));
	Progress.MakeDialog(true);
	for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
	{
		if (Progress.ShouldCancel())
		{
			OutResult = TEXT("Link generation cancelled. Existing project links were preserved.");
			return false;
		}
		Progress.EnterProgressFrame();
		const FSample& Sample = Samples[SampleIndex];
		FVector Start;
		const AActor* StartActor = nullptr;
		if (!ResolveFeet(World, Sample.Point, Settings, HeightTolerance, Start, StartActor))
		{
			continue;
		}
		FPFTraversalSolution Longest;
		if (!PFNavigationTraversal::Solve(Start, FVector(Start.X, Start.Y, Bounds.Min.Z - HeightTolerance), Settings,
			Settings.JumpSpeed, Longest))
		{
			continue;
		}
		const double Range = Settings.MaxSpeed * Longest.Time;
		const FIntPoint Min = Bucket(Start - FVector(Range, Range, 0));
		const FIntPoint Max = Bucket(Start + FVector(Range, Range, 0));
		TSet<int32> Nearby;
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			{
				if (const TArray<int32>* InBucket = Buckets.Find(FIntPoint(X, Y)))
				{
					for (int32 Index : *InBucket)
					{
						Nearby.Add(Index);
					}
				}
			}
		}
		TArray<FCandidate> Candidates;
		const double Apex = FMath::Square(Settings.JumpSpeed) / (2.0 * Settings.Gravity);
		for (int32 SurfaceIndex : Nearby)
		{
			const FSurface& Surface = Surfaces[SurfaceIndex];
			if (Surface.Bounds.Min.Z > Start.Z + Apex + HeightTolerance)
			{
				continue;
			}
			TArray<FVector> Points;
			FVector Closest;
			if (NavMesh.GetClosestPointOnPoly(Surface.Ref, Start, Closest))
			{
				Points.Add(Closest + (Surface.Center - Closest).GetSafeNormal2D() * 2.f);
				const int32 InteriorSamples = FMath::CeilToInt(FVector::Dist2D(Closest, Surface.Center) / Spacing);
				for (int32 Index = 1; Index < InteriorSamples; ++Index)
				{
					Points.Add(FMath::Lerp(Closest, Surface.Center, static_cast<double>(Index) / InteriorSamples));
				}
			}
			Points.Add(Surface.Center);
			for (const FVector& Vertex : Surface.Vertices)
			{
				Points.Add(FMath::Lerp(Vertex, Surface.Center, 0.1));
			}
			for (FVector Point : Points)
			{
				if (!NavMesh.GetClosestPointOnPoly(Surface.Ref, Point, Point))
				{
					continue;
				}
				const double DistanceSquared = FVector::DistSquared2D(Start, Point);
				if (DistanceSquared < FMath::Square(Spacing) || DistanceSquared > Range * Range
					|| FVector::DotProduct(Point - Start, Sample.Outward) <= 0)
				{
					continue;
				}
				Candidates.Add({ Point, DistanceSquared });
			}
		}
		Candidates.Sort([](const FCandidate& A, const FCandidate& B)
		{
			if (A.DistanceSquared != B.DistanceSquared) return A.DistanceSquared < B.DistanceSquared;
			if (A.Point.Z != B.Point.Z) return A.Point.Z < B.Point.Z;
			if (A.Point.X != B.Point.X) return A.Point.X < B.Point.X;
			return A.Point.Y < B.Point.Y;
		});
		TArray<int32> Accepted;
		auto TryCandidate = [&](const FCandidate& Candidate, EPFNavigationTraversal Mode)
		{
			FVector End;
			const AActor* EndActor = nullptr;
			if (!ResolveFeet(World, Candidate.Point, Settings, HeightTolerance, End, EndActor))
			{
				return;
			}
			const FActorDirectionKey ActorDirection{ StartActor, EndActor, GetDirectionSector(Start, End) };
			bool bAlreadyConnected = false;
			for (int32 Index : Accepted)
			{
				if (Results[Index].ActorDirection == ActorDirection
					&& CanWalkDirect(NavMesh, Results[Index].NavEnd, Candidate.Point, GroundFilter, Bot))
				{
					bAlreadyConnected = true;
					break;
				}
			}
			if (bAlreadyConnected || CanWalkDirect(NavMesh, Sample.Point, Candidate.Point, GroundFilter, Bot))
			{
				return;
			}
			if (HasContinuousFloor(World, Start, End, Settings, Spacing))
			{
				return;
			}
			++CheckedCandidates;
			if (Mode == EPFNavigationTraversal::Drop)
			{
				float Duration;
				if (!PFNavigationTraversal::ValidateDrop(World, Start, End, Settings, Duration))
				{
					return;
				}
			}
			else
			{
				FPFTraversalSolution Jump;
				if (HasNoJumpTag(World, Start, End, Settings)
					|| !PFNavigationTraversal::ValidateJump(World, Start, End, Settings, Jump))
				{
					return;
				}
			}
			int32 Existing = INDEX_NONE;
			for (int32 Index = Results.Num() - 1; Existing == INDEX_NONE && Index >= 0; --Index)
			{
				const FGeneratedLink& Previous = Results[Index];
				if (Previous.ActorDirection == ActorDirection
					&& Previous.Group == Sample.Group && Previous.LastSample == SampleIndex - 1
					&& FVector::DistSquared2D(Previous.LastStart, Start) <= FMath::Square(Spacing * 2.f)
					&& CanWalkDirect(NavMesh, Previous.NavEnd, Candidate.Point, GroundFilter, Bot))
				{
					Existing = Index;
					break;
				}
			}
			if (Existing == INDEX_NONE)
			{
				Existing = Results.Add({ Start, End, Start, Sample.Point, Candidate.Point, Sample.Group, SampleIndex, Mode, ActorDirection });
			}
			else
			{
				FGeneratedLink& Previous = Results[Existing];
				Previous.LastSample = SampleIndex;
				Previous.LastStart = Start;
				if ((Mode == EPFNavigationTraversal::Drop && Previous.Mode == EPFNavigationTraversal::Jump)
					|| (Mode == Previous.Mode && FVector::DistSquared2D(Start, End) < FVector::DistSquared2D(Previous.Start, Previous.End)))
				{
					Previous.Start = Start;
					Previous.End = End;
					Previous.NavStart = Sample.Point;
					Previous.NavEnd = Candidate.Point;
					Previous.Mode = Mode;
				}
			}
			Accepted.Add(Existing);
		};
		// 같은 발판의 모든 착지 후보에서 낙하를 먼저 확인
		for (EPFNavigationTraversal Mode : { EPFNavigationTraversal::Drop, EPFNavigationTraversal::Jump })
		{
			for (const FCandidate& Candidate : Candidates)
			{
				TryCandidate(Candidate, Mode);
			}
		}
	}

	const float MergeDistance = FMath::Max(Settings.Radius * 4.f, Spacing * 4.f);
	const int32 MergedCount = CompactLinks(Results, NavMesh, GroundFilter, Bot, MergeDistance);
	TMap<FString, APFNavigationLink*> ExistingLinks;
	TArray<APFNavigationLink*> OwnedLinks;
	for (TActorIterator<APFNavigationLink> It(&World); It; ++It)
	{
		if (It->GetGeneratorId() == GeneratorId)
		{
			OwnedLinks.Add(*It);
			ExistingLinks.Add(It->GetGenerationKey(), *It);
		}
	}
	TSet<APFNavigationLink*> Kept;
	TSet<FString> EmittedKeys;
	int32 DropCount = 0;
	for (const FGeneratedLink& Result : Results)
	{
		const FString Key = FString::Printf(TEXT("%s:%s>%s"), *NavMesh.GetName(),
			*PositionKey(Result.Start).ToString(), *PositionKey(Result.End).ToString());
		if (EmittedKeys.Contains(Key))
		{
			continue;
		}
		EmittedKeys.Add(Key);
		APFNavigationLink* Link = ExistingLinks.FindRef(Key);
		if (!Link)
		{
			FActorSpawnParameters Spawn;
			Spawn.OverrideLevel = Level;
			Spawn.ObjectFlags |= RF_Transactional;
			Link = World.SpawnActor<APFNavigationLink>(APFNavigationLink::StaticClass(), Result.Start, FRotator::ZeroRotator, Spawn);
		}
		if (!Link)
		{
			OutResult = TEXT("Failed to create a project link. Rebuild navigation links before saving.");
			return false;
		}
		Link->Configure(GeneratorId, Key, Result.Start, Result.End, Result.Mode);
		Link->SetActorLabel(FString::Printf(TEXT("PF_%s_%d"), Result.Mode == EPFNavigationTraversal::Drop ? TEXT("Drop") : TEXT("Jump"), Kept.Num()));
		Kept.Add(Link);
		DropCount += Result.Mode == EPFNavigationTraversal::Drop ? 1 : 0;
	}
	for (APFNavigationLink* Link : OwnedLinks)
	{
		if (!Kept.Contains(Link))
		{
			Link->Modify();
			World.DestroyActor(Link);
		}
	}
	OutResult = FString::Printf(TEXT("%d -> %d total links, %d jump links, %d walk/drop links, %d similar links merged, %d checked candidates. Save the map."),
		OwnedLinks.Num(), Kept.Num(), Kept.Num() - DropCount, DropCount, MergedCount, CheckedCandidates);
	return true;
#else
	OutResult = TEXT("Link generation requires an editor build with Recast.");
	return false;
#endif
}
