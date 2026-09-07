#include "Character/PFEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

APFEnemy::APFEnemy()
{
	SetAbility();

	if (UCharacterMovementComponent* EnemyMovementComponent = GetCharacterMovement())
	{
		EnemyMovementComponent->bRunPhysicsWithNoController = true;
		EnemyMovementComponent->bOrientRotationToMovement = false;
	}
}

void APFEnemy::BeginPlay()
{
	Super::BeginPlay();
	InitAbilityActorInfo();
}

void APFEnemy::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 사망 연출 종료 이벤트 연결
	if (PFAnim)
	{
		PFAnim->DeathEnd.AddUObject(this, &APFEnemy::HandleDeathAnimationEnd);
	}
}

void APFEnemy::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	if (HasAuthority())
	{
		ReplicatedAimPitch = GetAimPitch();
	}
}

void APFEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APFEnemy, ReplicatedAimPitch);
}

float APFEnemy::GetAimPitch() const
{
	if (!HasAuthority())
	{
		return ReplicatedAimPitch;
	}

	const APFCharacter* Target = TargetCharacter.Get();
	if (!IsValid(Target))
	{
		return 0.f;
	}

	// 대상 방향의 조준 피치 계산
	const FVector AimDirection =
		Target->GetActorLocation() - GetActorLocation();

	if (AimDirection.IsNearlyZero())
	{
		return 0.f;
	}

	const FRotator AimRotation = AimDirection.Rotation();
	const FRotator DeltaRotation =
		(AimRotation - GetActorRotation()).GetNormalized();

	return DeltaRotation.Pitch;
}

// 사망 연출 후 제거 예약
void APFEnemy::HandleDeathAnimationEnd()
{
	if (!HasAuthority() || !IsDeadCharacter())
	{
		return;
	}

	SetLifeSpan(3.f);
}

// 적 ASC, 스탯 생성
void APFEnemy::SetAbility()
{
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(true);
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UPFAttributeSet>(TEXT("AttributeSet"));
}

void APFEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || IsDeadCharacter())
	{
		return;
	}

	// 판단 주기, 추적 대상 갱신
	TargetRefreshTimeRemaining -= DeltaTime;
	AttackCommandTimeRemaining -= DeltaTime;
	PathRefreshTimeRemaining -= DeltaTime;

	if (TargetRefreshTimeRemaining <= 0.f
		|| (!TargetCharacter.IsExplicitlyNull() && !IsPlayerTargetValid(TargetCharacter.Get())))
	{
		AcquireNearestPlayerTarget();
		TargetRefreshTimeRemaining = TargetRefreshInterval;
	}

	APFCharacter* Target = TargetCharacter.Get();
	if (!IsPlayerTargetValid(Target))
	{
		ClearEnemyIntent();
		return;
	}

	// 이동, 공격 의도 갱신
	const float SurfaceDistance = GetTargetSurfaceDistance(Target);
	UpdateEnemyMovement(Target, SurfaceDistance, DeltaTime);

	const bool bShouldAttack = ShouldAttackTarget(Target, SurfaceDistance);
	IsAttacking = bShouldAttack;
	if (bShouldAttack && !bExecutingPathJump)
	{
		FVector DirectionToTarget = Target->GetActorLocation() - GetActorLocation();
		DirectionToTarget.Z = 0.f;
		RotateEnemyTowards(DirectionToTarget, DeltaTime);
	}

	// 주기적으로 공격 실행
	if (bShouldAttack && AttackCommandTimeRemaining <= 0.f)
	{
		Attack();
		AttackCommandTimeRemaining = AttackCommandInterval;
	}
}

// 가장 가까운 플레이어 탐색
void APFEnemy::AcquireNearestPlayerTarget()
{
	const TWeakObjectPtr<APFCharacter> PreviousTarget = TargetCharacter;
	TargetCharacter.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		APFCharacter* Candidate = PlayerController ? Cast<APFCharacter>(PlayerController->GetPawn()) : nullptr;
		if (!IsPlayerTargetValid(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			TargetCharacter = Candidate;
		}
	}

	// 대상 변경 시 이전 경로 해제
	if (PreviousTarget.Get() != TargetCharacter.Get())
	{
		ClearMovementPath();
	}
}

// 공격 대상 플레이어 확인
bool APFEnemy::IsPlayerTargetValid(const APFCharacter* Candidate) const
{
	return IsValid(Candidate)
		&& Candidate != this
		&& !Candidate->IsDeadCharacter()
		&& Cast<APlayerController>(Candidate->GetController()) != nullptr;
}

// 캡슐 표면 사이 거리 계산
float APFEnemy::GetTargetSurfaceDistance(const APFCharacter* Candidate) const
{
	if (!Candidate)
	{
		return TNumericLimits<float>::Max();
	}

	const float CenterDistance = FVector::Dist2D(GetActorLocation(), Candidate->GetActorLocation());
	const float OwnRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	const float TargetRadius = Candidate->GetCapsuleComponent() ? Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	return FMath::Max(0.f, CenterDistance - OwnRadius - TargetRadius);
}

// 공격 거리 조건 확인
bool APFEnemy::ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const
{
	return IsPlayerTargetValid(Target) && SurfaceDistance <= AttackRange;
}

// 대상과의 전투 거리 유지
void APFEnemy::UpdateEnemyMovement(APFCharacter* Target, float SurfaceDistance, float DeltaTime)
{
	if (!Target)
	{
		return;
	}

	if (IsMovementBlocked())
	{
		SetEnemyDirection(IDLE);
		return;
	}

	// 진행 중인 점프 완료까지 경로 유지
	if (bExecutingPathJump)
	{
		if (FollowMovementPath(DeltaTime))
		{
			SetEnemyDirection(FWD);
		}
		return;
	}

	FVector DirectionToTarget = Target->GetActorLocation() - GetActorLocation();
	DirectionToTarget.Z = 0.f;
	if (DirectionToTarget.IsNearlyZero())
	{
		ClearMovementPath();
		SetEnemyDirection(IDLE);
		return;
	}

	DirectionToTarget.Normalize();

	// 거리에 따라 접근, 후퇴, 정지
	if (SurfaceDistance > DesiredCombatDistance + DistanceTolerance)
	{
		if (RefreshMovementPath(Target) && FollowMovementPath(DeltaTime))
		{
			SetEnemyDirection(FWD);
		}
		else
		{
			SetEnemyDirection(IDLE);
		}
	}
	else if (bRetreatWhenTooClose && SurfaceDistance < FMath::Max(0.f, DesiredCombatDistance - DistanceTolerance))
	{
		ClearMovementPath();
		RotateEnemyTowards(DirectionToTarget, DeltaTime);
		AddMovementInput(-DirectionToTarget, 1.f);
		SetEnemyDirection(BWD);
	}
	else
	{
		ClearMovementPath();
		SetEnemyDirection(IDLE);
	}
}

// 추적 경로 갱신
bool APFEnemy::RefreshMovementPath(const APFCharacter* Target)
{
	if (!Target || bExecutingPathJump || bExecutingPathWalk)
	{
		return HasPendingMovementPath();
	}

	// 대상 이동량, 갱신 주기 확인
	const bool bPathFinished = !HasPendingMovementPath();
	const bool bTargetMoved = FVector::DistSquared(LastPathTargetLocation, Target->GetActorLocation())
		>= FMath::Square(PathTargetMoveThreshold);
	if (!bTargetMoved && PathRefreshTimeRemaining > 0.f)
	{
		return !bPathFinished;
	}

	UPFNaviSubsystem* NaviSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UPFNaviSubsystem>()
		: nullptr;
	// 새 경로 탐색, 진행 상태 교체
	TArray<int32> NewNodePath;
	FVector NewGoalLocation = FVector::ZeroVector;
	const EPFPathResult PathResult = NaviSubsystem
		? NaviSubsystem->FindPath(this, GetActorLocation(), Target->GetActorLocation(), NewNodePath, NewGoalLocation)
		: EPFPathResult::Failed;

	PathRefreshTimeRemaining = PathRefreshInterval;
	LastPathTargetLocation = Target->GetActorLocation();
	if (PathResult == EPFPathResult::Failed || NewNodePath.IsEmpty())
	{
		NodePath.Reset();
		CurNodeIdx = 0;
		GoalLocation = FVector::ZeroVector;
		return false;
	}

	NodePath = MoveTemp(NewNodePath);
	CurNodeIdx = 0;
	GoalLocation = NewGoalLocation;
	bExecutingPathJump = false;
	bPathJumpWasAirborne = false;
	bExecutingPathWalk = false;
	CurrentTraversalDestination = FVector::ZeroVector;
	LastPathWalkProgressLocation = FVector::ZeroVector;
	PathWalkStuckTimeRemaining = 0.f;
	PathJumpTimeRemaining = 0.f;
	return true;
}

// 경로의 보행, 점프 구간 실행
bool APFEnemy::FollowMovementPath(float DeltaTime)
{
	if (IsMovementBlocked())
	{
		return false;
	}

	UCharacterMovementComponent* EnemyMovementComponent = GetCharacterMovement();
	UPFNaviSubsystem* NaviSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UPFNaviSubsystem>()
		: nullptr;
	if (!EnemyMovementComponent || !NaviSubsystem)
	{
		ClearMovementPath();
		return false;
	}

	while (HasPendingMovementPath())
	{
		// 점프 진행, 착지 확인
		if (bExecutingPathJump)
		{
			PathJumpTimeRemaining -= DeltaTime;
			if (PathJumpTimeRemaining <= 0.f)
			{
				ClearMovementPath();
				return false;
			}

			FVector DirectionToLanding = CurrentTraversalDestination - GetActorLocation();
			DirectionToLanding.Z = 0.f;
			if (EnemyMovementComponent->IsFalling())
			{
				bPathJumpWasAirborne = true;
				if (!DirectionToLanding.IsNearlyZero())
				{
					DirectionToLanding.Normalize();
					RotateEnemyTowards(DirectionToLanding, DeltaTime);
				}
				return true;
			}

			if (bPathJumpWasAirborne && EnemyMovementComponent->IsMovingOnGround())
			{
				const float LandingDistance = FVector::Dist2D(
					GetActorLocation(), CurrentTraversalDestination);
				bExecutingPathJump = false;
				bPathJumpWasAirborne = false;
				CurrentTraversalDestination = FVector::ZeroVector;
				if (LandingDistance > PathPointAcceptanceRadius * 3.f)
				{
					ClearMovementPath();
					return false;
				}

				++CurNodeIdx;
				return true;
			}

			return true;
		}

		// 보행 도착, 이동 정체 확인
		if (bExecutingPathWalk)
		{
			FVector DirectionToDestination = CurrentTraversalDestination - GetActorLocation();
			DirectionToDestination.Z = 0.f;
			if (DirectionToDestination.Size2D() <= PathPointAcceptanceRadius)
			{
				bExecutingPathWalk = false;
				CurrentTraversalDestination = FVector::ZeroVector;
				LastPathWalkProgressLocation = FVector::ZeroVector;
				PathWalkStuckTimeRemaining = 0.f;
				++CurNodeIdx;
				return true;
			}

			if (FVector::DistSquared(LastPathWalkProgressLocation, GetActorLocation())
				>= FMath::Square(PathWalkMinimumProgressDistance))
			{
				LastPathWalkProgressLocation = GetActorLocation();
				PathWalkStuckTimeRemaining = PathWalkStuckTimeout;
			}
			else
			{
				PathWalkStuckTimeRemaining -= DeltaTime;
				if (PathWalkStuckTimeRemaining <= 0.f)
				{
					ClearMovementPath();
					return false;
				}
			}

			DirectionToDestination.Normalize();
			RotateEnemyTowards(DirectionToDestination, DeltaTime);
			AddMovementInput(DirectionToDestination, 1.f);
			return true;
		}

		if (CurNodeIdx < NodePath.Num())
		{
			const int32 TargetNode = NodePath[CurNodeIdx];
			if (!NaviSubsystem->IsValidRef(TargetNode))
			{
				ClearMovementPath();
				return false;
			}

			const FVector TargetNodeLocation = NaviSubsystem->GetNodeLocation(TargetNode);
			// 첫 노드 진입 경로 확인
			if (CurNodeIdx == 0)
			{
				if (NodePath.Num() == 1
					&& FVector::Dist2D(TargetNodeLocation, GoalLocation) > PathPointAcceptanceRadius
					&& NaviSubsystem->IsWalkCapsulePathClear(GetActorLocation(), GoalLocation))
				{
					CurNodeIdx = NodePath.Num();
					continue;
				}

				FVector DirectionToFirstNode = TargetNodeLocation - GetActorLocation();
				DirectionToFirstNode.Z = 0.f;
				if (DirectionToFirstNode.Size2D() <= PathPointAcceptanceRadius)
				{
					++CurNodeIdx;
					continue;
				}

				if (!NaviSubsystem->IsWalkCapsulePathClear(GetActorLocation(), TargetNodeLocation))
				{
					ClearMovementPath();
					return false;
				}

				BeginPathWalk(TargetNodeLocation);
				continue;
			}

			const int32 StartNode = NodePath[CurNodeIdx - 1];
			const FPFNaviEdge* Edge = NaviSubsystem->FindEdge(StartNode, TargetNode);
			if (!NaviSubsystem->IsValidRef(StartNode) || !Edge)
			{
				ClearMovementPath();
				return false;
			}

			const FVector StartNodeLocation = NaviSubsystem->GetNodeLocation(StartNode);
			// 일반 보행 구간 진입
			if (!Edge->bJump)
			{
				FVector DirectionToTargetNode = TargetNodeLocation - GetActorLocation();
				DirectionToTargetNode.Z = 0.f;
				if (DirectionToTargetNode.Size2D() <= PathPointAcceptanceRadius)
				{
					++CurNodeIdx;
					continue;
				}

				BeginPathWalk(TargetNodeLocation);
				continue;
			}

			// 점프 출발점으로 이동
			FVector DirectionToTakeoff = StartNodeLocation - GetActorLocation();
			DirectionToTakeoff.Z = 0.f;
			if (DirectionToTakeoff.Size2D() > JumpTakeoffTolerance)
			{
				DirectionToTakeoff.Normalize();
				RotateEnemyTowards(DirectionToTakeoff, DeltaTime);
				AddMovementInput(DirectionToTakeoff, 1.f);
				return true;
			}

			if (IsPathJumpBlocked() || !EnemyMovementComponent->IsMovingOnGround())
			{
				ClearMovementPath();
				return false;
			}

			// 착지 방향에 맞춰 점프 속도 적용
			CurrentTraversalDestination = TargetNodeLocation;
			FVector DirectionToLanding = CurrentTraversalDestination - GetActorLocation();
			DirectionToLanding.Z = 0.f;
			const float HorizontalLaunchSpeed = Edge->JumpVelocity.Size2D();
			const FVector HorizontalLaunchVelocity = DirectionToLanding.GetSafeNormal() * HorizontalLaunchSpeed;
			const FVector LaunchVelocity = HorizontalLaunchVelocity
				+ FVector::UpVector * Edge->JumpVelocity.Z;
			RotateEnemyTowards(DirectionToLanding, DeltaTime);
			LaunchCharacter(LaunchVelocity, true, true);
			bExecutingPathJump = true;
			bPathJumpWasAirborne = false;
			PathJumpTimeRemaining = PathJumpTimeout;

			// 낮은 지점으로 점프할 때 대기 시간 보정
			if (CurrentTraversalDestination.Z < StartNodeLocation.Z)
			{
				const float GravityMagnitude = FMath::Abs(EnemyMovementComponent->GetGravityZ());
				const float VerticalDifference = CurrentTraversalDestination.Z - StartNodeLocation.Z;
				const float FlightDiscriminant = FMath::Square(Edge->JumpVelocity.Z)
					- 2.f * GravityMagnitude * VerticalDifference;
				if (GravityMagnitude > UE_SMALL_NUMBER && FlightDiscriminant >= 0.f)
				{
					const float PlannedFlightTime = (Edge->JumpVelocity.Z + FMath::Sqrt(FlightDiscriminant))
						/ GravityMagnitude;
					PathJumpTimeRemaining = FMath::Max(PathJumpTimeout, PlannedFlightTime + 0.5f);
				}
			}
			return true;
		}

		// 마지막 노드에서 최종 목적지로 이동
		if (CurNodeIdx == NodePath.Num())
		{
			const int32 LastNode = NodePath.Last();
			if (!NaviSubsystem->IsValidRef(LastNode))
			{
				ClearMovementPath();
				return false;
			}

			FVector DirectionToGoal = GoalLocation - GetActorLocation();
			DirectionToGoal.Z = 0.f;
			if (DirectionToGoal.Size2D() <= PathPointAcceptanceRadius)
			{
				++CurNodeIdx;
				return false;
			}

			if (!NaviSubsystem->IsWalkCapsulePathClear(GetActorLocation(), GoalLocation))
			{
				ClearMovementPath();
				return false;
			}

			BeginPathWalk(GoalLocation);
			continue;
		}
	}

	return false;
}

// 경로 보행 시작
void APFEnemy::BeginPathWalk(const FVector& Destination)
{
	CurrentTraversalDestination = Destination;
	bExecutingPathWalk = true;
	LastPathWalkProgressLocation = GetActorLocation();
	PathWalkStuckTimeRemaining = PathWalkStuckTimeout;
}

// 남은 이동 경로 확인
bool APFEnemy::HasPendingMovementPath() const
{
	return !NodePath.IsEmpty() && CurNodeIdx >= 0 && CurNodeIdx <= NodePath.Num();
}

// 이동 경로, 진행 상태 초기화
void APFEnemy::ClearMovementPath()
{
	NodePath.Reset();
	CurNodeIdx = 0;
	GoalLocation = FVector::ZeroVector;
	PathRefreshTimeRemaining = 0.f;
	LastPathTargetLocation = FVector::ZeroVector;
	bExecutingPathJump = false;
	bPathJumpWasAirborne = false;
	bExecutingPathWalk = false;
	CurrentTraversalDestination = FVector::ZeroVector;
	LastPathWalkProgressLocation = FVector::ZeroVector;
	PathWalkStuckTimeRemaining = 0.f;
	PathJumpTimeRemaining = 0.f;
}

// 경로 점프 차단 여부 조회
bool APFEnemy::IsPathJumpBlocked() const
{
	if (!ASC)
	{
		return false;
	}

	const FGameplayTag JumpBlockTag = FGameplayTag::RequestGameplayTag(FName("Character.Block.Jump"));
	return ASC->HasMatchingGameplayTag(JumpBlockTag);
}

// 이동 방향으로 회전 보간
void APFEnemy::RotateEnemyTowards(const FVector& WorldDirection, float DeltaTime)
{
	FVector HorizontalDirection = WorldDirection;
	HorizontalDirection.Z = 0.f;
	if (HorizontalDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation = HorizontalDirection.Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, RotationInterpSpeed));
}

// 이동 방향, 애니메이션 반영
void APFEnemy::SetEnemyDirection(EPFDirection NewDirection)
{
	if (FinalDir == NewDirection)
	{
		return;
	}

	FinalDir = NewDirection;
	if (PFAnim)
	{
		PFAnim->SetCurrentDir(FinalDir);
	}
}

// 공격, 이동 의도 해제
void APFEnemy::ClearEnemyIntent()
{
	IsAttacking = false;
	ClearMovementPath();
	SetEnemyDirection(IDLE);
}
