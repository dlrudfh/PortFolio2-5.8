#include "System/Framework/PFEnemyAIController.h"

#include "Character/PFCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GAS/PFGameplayTags.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "System/Navigation/PFCrowdFollowingComponent.h"
#include "System/Navigation/PFNavLinkProxy.h"
#include "Templates/UnrealTemplate.h"

APFEnemyAIController::APFEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPFCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
	bSetControlRotationFromPawnOrientation = false;
	DefaultNavigationFilterClass = UPFNavigationQueryFilter::StaticClass();
}

void APFEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	APFCharacter* ControlledPawn = Cast<APFCharacter>(InPawn);
	ControlledCharacter = ControlledPawn;
	if (!ControlledPawn)
	{
		return;
	}
	bRetreating = false;
	bOutOfNavigation = false;
	SetLifeSpan(0.f);
	CombatSettings = ControlledPawn->GetAISettings();
	ControlledPawn->OnCharacterDied.AddUObject(this, &APFEnemyAIController::HandleCharacterDied);
	ControlledPawn->OnDeathAnimationEnded.AddUObject(this, &APFEnemyAIController::HandleDeathAnimationEnd);
	ControlledPawn->OnCharacterLanded.AddUObject(this, &APFEnemyAIController::HandleCharacterLanded);
	ControlledPawn->OnCharacterReady.AddUObject(this, &APFEnemyAIController::HandleCharacterReady);
	bSavedRVOAvoidance = ControlledPawn->GetCharacterMovement()->bUseRVOAvoidance;
	bSavedPhysicsInteraction = ControlledPawn->GetCharacterMovement()->bEnablePhysicsInteraction;
	ControlledPawn->GetCharacterMovement()->SetAvoidanceEnabled(false);
	ControlledPawn->GetCharacterMovement()->bEnablePhysicsInteraction = false;
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdObstacleAvoidance(true);
		Crowd->SetCrowdSeparation(true);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
		Crowd->SetCrowdAffectFallingVelocity(false);
	}
	HandleCharacterReady(ControlledPawn);
	ProgressOrigin = ControlledPawn->GetActorLocation();
	bNavigationRefreshRequested = true;
	NoProgressTime = 0.f;
	NextPathRequestTime = 0.f;
	NextRetryTime = 0.f;
	TargetCheckTimeRemaining = 0.f;
	bNavigationAbortPending = false;
	bMoveBlockedLastTick = false;
	FailedJumpLinks.Reset();
	TargetRefreshTimeRemaining = 0.f;
	AttackCommandTimeRemaining = 0.f;
}

void APFEnemyAIController::OnUnPossess()
{
	ClearMovementPath();
	ClearEnemyIntent();
	UnbindNavigationTags();
	if (APFCharacter* ControlledPawn = ControlledCharacter.Get())
	{
		ControlledPawn->OnCharacterDied.RemoveAll(this);
		ControlledPawn->OnDeathAnimationEnded.RemoveAll(this);
		ControlledPawn->OnCharacterLanded.RemoveAll(this);
		ControlledPawn->OnCharacterReady.RemoveAll(this);
		ControlledPawn->GetCharacterMovement()->SetAvoidanceEnabled(bSavedRVOAvoidance);
		ControlledPawn->GetCharacterMovement()->bEnablePhysicsInteraction = bSavedPhysicsInteraction;
	}
	ControlledCharacter.Reset();
	TargetCharacter.Reset();
	Super::OnUnPossess();
	if (!IsActorBeingDestroyed())
	{
		SetLifeSpan(0.1f);
	}
}

void APFEnemyAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (APFCharacter* ControlledPawn = ControlledCharacter.Get())
	{
		SetControlRotation(ControlledPawn->GetActorRotation());
	}
}

// 사망 후 추적, 공격 중단
void APFEnemyAIController::HandleCharacterDied(APFCharacter* ControlledPawn)
{
	if (ControlledPawn == ControlledCharacter.Get())
	{
		ClearEnemyIntent();
		TargetCharacter.Reset();
	}
}

// 사망 연출 후 제거 예약
void APFEnemyAIController::HandleDeathAnimationEnd(APFCharacter* ControlledPawn)
{
	if (HasAuthority() && ControlledPawn == ControlledCharacter.Get() && ControlledPawn->IsDeadCharacter())
	{
		ControlledPawn->SetLifeSpan(3.f);
	}
}

bool APFEnemyAIController::TryGetCombatAim(FVector& OutAimPoint)
{
	const APFCharacter* Target = TargetCharacter.Get();
	if (!ControlledCharacter.IsValid() || ControlledCharacter->IsDeadCharacter() || !IsPlayerTargetValid(Target))
	{
		ClearEnemyIntent();
		return false;
	}
	OutAimPoint = Target->GetActorLocation();
	return true;
}

void APFEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || !ControlledCharacter.IsValid() || ControlledCharacter->IsDeadCharacter())
	{
		return;
	}

	// 판단 주기, 추적 대상 갱신
	TargetRefreshTimeRemaining -= DeltaTime;
	AttackCommandTimeRemaining -= DeltaTime;

	const bool bLostTarget = !TargetCharacter.IsExplicitlyNull()
		&& !IsPlayerTargetValid(TargetCharacter.Get());
	if (bLostTarget)
	{
		ClearEnemyIntent();
	}

	if (TargetRefreshTimeRemaining <= 0.f || bLostTarget)
	{
		AcquireNearestPlayerTarget();
		TargetRefreshTimeRemaining = TargetRefreshInterval;
	}

	APFCharacter* Target = TargetCharacter.Get();
	if (!IsPlayerTargetValid(Target))
	{
		ClearEnemyIntent();
		if (bExecutingPathJump)
		{
			UpdateEnemyMovement(nullptr, 0.f, DeltaTime);
		}
		return;
	}

	// 이동, 공격 의도 갱신
	const float SurfaceDistance = GetTargetSurfaceDistance(Target);
	UpdateEnemyMovement(Target, SurfaceDistance, DeltaTime);

	const bool bShouldAttack = ShouldAttackTarget(Target, SurfaceDistance);
	ControlledCharacter->SetAIAttackCommand(bShouldAttack, false);
	if (bShouldAttack && !bExecutingPathJump)
	{
		FVector DirectionToTarget = Target->GetActorLocation() - ControlledCharacter->GetActorLocation();
		DirectionToTarget.Z = 0.f;
		RotateEnemyTowards(DirectionToTarget, DeltaTime);
	}

	// 주기적으로 공격 실행
	if (bShouldAttack && AttackCommandTimeRemaining <= 0.f)
	{
		ControlledCharacter->SetAIAttackCommand(true, true);
		AttackCommandTimeRemaining = AttackCommandInterval;
	}
}

// 가장 가까운 플레이어 탐색
void APFEnemyAIController::AcquireNearestPlayerTarget()
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

		const float DistanceSquared = FVector::DistSquared2D(ControlledCharacter->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			TargetCharacter = Candidate;
		}
	}

	// 점프 중에는 착지 후 대상 변경 반영
	if (PreviousTarget.Get() != TargetCharacter.Get())
	{
		RequestNavigationRefresh();
	}
}

// 공격 대상 플레이어 확인
bool APFEnemyAIController::IsPlayerTargetValid(const APFCharacter* Candidate) const
{
	return IsValid(Candidate)
		&& Candidate != ControlledCharacter.Get()
		&& !Candidate->IsDeadCharacter()
		&& !Candidate->HasStateTag(PFGameplayTags::Character_State_Invulnerable)
		&& Candidate->IsPlayerCharacter()
		&& Cast<APlayerController>(Candidate->GetController()) != nullptr;
}

// 캡슐 표면 사이 거리 계산
float APFEnemyAIController::GetTargetSurfaceDistance(const APFCharacter* Candidate) const
{
	if (!Candidate)
	{
		return TNumericLimits<float>::Max();
	}

	const float CenterDistance = FVector::Dist2D(ControlledCharacter->GetActorLocation(), Candidate->GetActorLocation());
	const float OwnRadius = ControlledCharacter->GetCapsuleComponent() ? ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	const float TargetRadius = Candidate->GetCapsuleComponent() ? Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	const float HorizontalDistance = FMath::Max(0.f, CenterDistance - OwnRadius - TargetRadius);
	const float HeightDifference = FMath::Abs(ControlledCharacter->GetCharacterMovement()->GetActorFeetLocation().Z
		- Candidate->GetCharacterMovement()->GetActorFeetLocation().Z);
	return FMath::Sqrt(FMath::Square(HorizontalDistance) + FMath::Square(HeightDifference));
}

// 공격 거리 조건 확인
bool APFEnemyAIController::ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const
{
	return IsPlayerTargetValid(Target) && SurfaceDistance <= CombatSettings.AttackRange
		&& (!CombatSettings.bRequiresLineOfSight || HasClearSightToTarget(Target));
}

// 대상 접근 조건 확인
bool APFEnemyAIController::ShouldApproachTarget(const APFCharacter* Target, float SurfaceDistance) const
{
	return IsPlayerTargetValid(Target)
		&& (!IsTargetAtMovementHeight(Target)
			|| SurfaceDistance > CombatSettings.DesiredCombatDistance + CombatSettings.DistanceTolerance
			|| (CombatSettings.bRequiresLineOfSight && !HasClearSightToTarget(Target)));
}

// 대상과의 전투 거리 유지
void APFEnemyAIController::UpdateEnemyMovement(APFCharacter* Target, float SurfaceDistance, float DeltaTime)
{
	UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
	const float Now = GetWorld()->GetTimeSeconds();
	TargetCheckTimeRemaining -= DeltaTime;

	if (bNavigationAbortPending)
	{
		ClearMovementPath();
		bNavigationAbortPending = false;
		bNavigationRefreshRequested = true;
	}
	if (bExecutingPathJump)
	{
		ControlledCharacter->ConsumeMovementInputVector();
		JumpTimeRemaining -= DeltaTime;
		if (JumpTimeRemaining <= 0.f)
		{
			FailNavigationJump();
		}
		else if (bApproachingJumpStart)
		{
			if (!Movement->IsMovingOnGround() || IsPathJumpBlocked() || ControlledCharacter->IsMovementBlocked())
			{
				FailNavigationJump();
			}
			else
			{
				const FVector Delta = JumpTakeoffFeet - Movement->GetActorFeetLocation();
				if (Delta.Size2D() <= 3.f && FMath::Abs(Delta.Z) <= Movement->MaxStepHeight)
				{
					LaunchNavigationJump();
				}
				else
				{
					const float Speed = FMath::Min(Movement->MaxWalkSpeed, static_cast<float>(Delta.Size2D()) / FMath::Max(0.1f, DeltaTime));
					Movement->RequestDirectMove(Delta.GetSafeNormal2D() * Speed, false);
					RotateEnemyTowards(Delta, DeltaTime);
				}
				SetEnemyDirection(FWD);
				return;
			}
		}
		else
		{
			SetEnemyDirection(FWD);
			return;
		}
	}

	const bool bMoveBlocked = ControlledCharacter->IsMovementBlocked();
	if (bMoveBlocked)
	{
		if (!bMoveBlockedLastTick)
		{
			ClearMovementPath();
		}
		bMoveBlockedLastTick = true;
		NoProgressTime = 0.f;
		ProgressOrigin = ControlledCharacter->GetActorLocation();
		SetEnemyDirection(IDLE);
		return;
	}
	if (bMoveBlockedLastTick)
	{
		bMoveBlockedLastTick = false;
		RequestNavigationRefresh();
	}
	if (Movement->IsFalling())
	{
		NoProgressTime = 0.f;
		return;
	}
	if (!Target || bOutOfNavigation)
	{
		SetEnemyDirection(IDLE);
		return;
	}

	const FVector DirectionToTarget = (Target->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal2D();
	const bool bShouldApproach = ShouldApproachTarget(Target, SurfaceDistance);
	const bool bShouldRetreat = !bShouldApproach && CombatSettings.bRetreatWhenTooClose && IsTargetAtMovementHeight(Target)
		&& SurfaceDistance < FMath::Max(0.f, CombatSettings.DesiredCombatDistance - CombatSettings.DistanceTolerance);
	if (bShouldApproach || bShouldRetreat)
	{
		if (bRetreating != bShouldRetreat)
		{
			bRetreating = bShouldRetreat;
			RequestNavigationRefresh();
		}
		if (TargetCheckTimeRemaining <= 0.f)
		{
			TargetCheckTimeRemaining = PathTargetCheckInterval;
			if (FVector::DistSquared(LastRequestedTarget, Target->GetActorLocation()) >= FMath::Square(PathTargetRefreshDistance))
			{
				bNavigationRefreshRequested = true;
			}
		}
		if (Now >= NextPathRequestTime
			&& (bNavigationRefreshRequested || (!ActiveMoveRequest.IsValid() && Now >= NextRetryTime)))
		{
			RefreshMovementPath(Target);
		}
		WatchMovementProgress(DeltaTime);
		const FVector Velocity = ControlledCharacter->GetVelocity();
		if (bRetreating)
		{
			RotateEnemyTowards(DirectionToTarget, DeltaTime);
		}
		else if (!Velocity.IsNearlyZero())
		{
			RotateEnemyTowards(Velocity, DeltaTime);
		}
		SetEnemyDirection(Velocity.Size2D() > 5.f ? (bRetreating ? BWD : FWD) : IDLE);
	}
	else
	{
		bRetreating = false;
		ClearMovementPath();
		NoProgressTime = 0.f;
		ProgressOrigin = ControlledCharacter->GetActorLocation();
		SetEnemyDirection(IDLE);
	}
}

// 현재 NavMesh에서 추적, 후퇴 경로 요청
bool APFEnemyAIController::RefreshMovementPath(const APFCharacter* Target)
{
	if (!Target || !ControlledCharacter.IsValid() || bExecutingPathJump || bOutOfNavigation)
	{
		return false;
	}
	UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
	if (!Movement->IsMovingOnGround())
	{
		return false;
	}
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const float Now = GetWorld()->GetTimeSeconds();
	bNavigationRefreshRequested = false;
	NextPathRequestTime = Now + PathTargetCheckInterval;
	NextRetryTime = Now + 1.f;
	LastRequestedTarget = Target->GetActorLocation();
	for (auto Iterator = FailedJumpLinks.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value() <= Now)
		{
			Iterator.RemoveCurrent();
		}
	}
	const FVector Feet = Movement->GetActorFeetLocation();
	const ARecastNavMesh* NavMesh = Navigation
		? Cast<ARecastNavMesh>(Navigation->GetNavDataForProps(Movement->GetNavAgentPropertiesRef(), Feet)) : nullptr;
	const float HeightRange = Movement->MaxStepHeight + 6.f;
	FNavLocation NavStart;
	if (!NavMesh || !Navigation->ProjectPointToNavigation(Feet, NavStart, FVector(10.f, 10.f, HeightRange), NavMesh)
		|| FVector::Dist2D(Feet, NavStart.Location) > 10.f || FMath::Abs(NavStart.Location.Z - Feet.Z) > HeightRange)
	{
		ClearMovementPath();
		bOutOfNavigation = true;
		NoProgressTime = 0.f;
		Movement->StopMovementImmediately();
		PFLOG(Error, TEXT("Enemy is out of Navimesh"));
		return false;
	}

	FVector GoalFeet = Target->GetCharacterMovement()->GetActorFeetLocation();
	if (bRetreating)
	{
		FVector RetreatDirection = (Feet - GoalFeet).GetSafeNormal2D();
		if (RetreatDirection.IsNearlyZero())
		{
			RetreatDirection = -ControlledCharacter->GetActorForwardVector().GetSafeNormal2D();
		}
		const float RetreatDistance = FMath::Max(60.f, CombatSettings.DesiredCombatDistance - GetTargetSurfaceDistance(Target));
		FNavLocation RetreatGoal;
		if (!Navigation->ProjectPointToNavigation(Feet + RetreatDirection * RetreatDistance, RetreatGoal,
			FVector(50.f, 50.f, HeightRange), NavMesh)
			|| FMath::Abs(RetreatGoal.Location.Z - Feet.Z) > HeightRange
			|| FVector::DotProduct(RetreatGoal.Location - Feet, RetreatDirection) <= 5.f)
		{
			ClearMovementPath();
			return false;
		}
		GoalFeet = RetreatGoal.Location;
	}
	FAIMoveRequest Request(GoalFeet);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(true);
	Request.SetRequireNavigableEndLocation(false);
	Request.SetProjectGoalLocation(false);
	Request.SetCanStrafe(bRetreating);
	Request.SetNavigationFilter(UPFNavigationQueryFilter::StaticClass());
	Request.SetAcceptanceRadius(5.f);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetReachTestIncludesGoalRadius(false);
	// 다른 층을 도착으로 판정하지 않도록 높이 오차 제한
	GetPathFollowingComponent()->SetPreciseReachThreshold(0.f,
		ControlledCharacter->GetCharacterMovement()->MaxStepHeight
		/ FMath::Max(1.f, ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	TGuardValue<TOptional<FVector>> StartGuard(PathRequestStart, TOptional<FVector>(NavStart.Location));
	// 이전 요청의 완료 콜백을 분리하고 이동 중 새 경로로 교체
	ActiveMoveRequest = FAIRequestID::InvalidRequest;
	bWaitingAtPathEnd = false;
	const FPathFollowingRequestResult Result = MoveTo(Request);
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveMoveRequest = Result.MoveId;
		return true;
	}
	// 즉시 완료 요청은 진행 중인 요청으로 보관하지 않음
	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bWaitingAtPathEnd = true;
		NoProgressTime = 0.f;
		return true;
	}
	ClearMovementPath();
	return false;
}

void APFEnemyAIController::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const
{
	if (MoveRequest.IsUsingPathfinding() && PathRequestStart.IsSet())
	{
		Query.StartLocation = PathRequestStart.GetValue();
	}
	Super::FindPathForMoveRequest(MoveRequest, Query, OutPath);
	if (MoveRequest.IsUsingPathfinding() && OutPath.IsValid())
	{
		// 재탐색 주기, 봇별 필터 갱신은 컨트롤러에서 관리
		OutPath->EnableRecalculationOnInvalidation(false);
	}
}

void APFEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID == ActiveMoveRequest)
	{
		ActiveMoveRequest = FAIRequestID::InvalidRequest;
		bWaitingAtPathEnd = Result.IsSuccess();
		NextRetryTime = GetWorld()->GetTimeSeconds() + 1.f;
		if (Result.IsSuccess())
		{
			NoProgressTime = 0.f;
		}
		if (!Result.IsSuccess())
		{
			bNavigationRefreshRequested = true;
		}
		if (bExecutingPathJump)
		{
			FailNavigationJump();
		}
	}
	Super::OnMoveCompleted(RequestID, Result);
}

// 이전 요청의 콜백과 새 요청 분리
void APFEnemyAIController::ClearMovementPath()
{
	ActiveMoveRequest = FAIRequestID::InvalidRequest;
	bWaitingAtPathEnd = false;
	if (bExecutingPathJump)
	{
		RestoreJumpMovement();
	}
	if (GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		const bool bAirborne = ControlledCharacter.IsValid() && ControlledCharacter->GetCharacterMovement()->IsFalling();
		GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::ForcedScript,
			FAIRequestID::CurrentRequest, bAirborne ? EPathFollowingVelocityMode::Keep : EPathFollowingVelocityMode::Reset);
	}
}

// 정상 점프를 유지하며 새 경로 예약
void APFEnemyAIController::RequestNavigationRefresh()
{
	if (bOutOfNavigation)
	{
		return;
	}
	bNavigationRefreshRequested = true;
	if (!bExecutingPathJump)
	{
		ClearMovementPath();
	}
}

// 경로 유무와 관계없이 1초 정체 복구
void APFEnemyAIController::WatchMovementProgress(float DeltaTime)
{
	const FVector Location = ControlledCharacter->GetActorLocation();
	if (bWaitingAtPathEnd || bOutOfNavigation)
	{
		ProgressOrigin = Location;
		NoProgressTime = 0.f;
		return;
	}
	if (FVector::DistSquared(Location, ProgressOrigin) >= FMath::Square(10.f))
	{
		ProgressOrigin = Location;
		NoProgressTime = 0.f;
		return;
	}
	NoProgressTime += DeltaTime;
	if (NoProgressTime >= 1.f)
	{
		NoProgressTime = 0.f;
		ProgressOrigin = Location;
		RequestNavigationRefresh();
	}
}

// 출발 링크의 폴리곤 식별
NavNodeRef APFEnemyAIController::FindJumpLinkRef(const FVector& Destination) const
{
	const FNavPathSharedPtr Path = GetPathFollowingComponent()->GetPath();
	const FNavMeshPath* MeshPath = Path.IsValid() ? Path->CastPath<FNavMeshPath>() : nullptr;
	const ARecastNavMesh* NavMesh = MeshPath ? Cast<ARecastNavMesh>(MeshPath->GetNavigationDataUsed()) : nullptr;
	if (!NavMesh)
	{
		return INVALID_NAVNODEREF;
	}
	NavNodeRef BestRef = INVALID_NAVNODEREF;
	float BestDistance = TNumericLimits<float>::Max();
	const FVector Feet = ControlledCharacter->GetCharacterMovement()->GetActorFeetLocation();
	for (NavNodeRef Ref : MeshPath->PathCorridor)
	{
		FVector Start;
		FVector End;
		if (!NavMesh->GetLinkEndPoints(Ref, Start, End))
		{
			continue;
		}
		const float Distance = FMath::Min(
			FVector::DistSquared(End, Destination) + FVector::DistSquared(Start, Feet),
			FVector::DistSquared(Start, Destination) + FVector::DistSquared(End, Feet));
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestRef = Ref;
		}
	}
	return BestRef;
}

// 자동 링크 출발점 접근
void APFEnemyAIController::BeginNavigationJump(UPFNavLinkProxy* Link, const FVector& Destination)
{
	if (!ControlledCharacter.IsValid() || !HasAuthority())
	{
		bNavigationAbortPending = true;
		GetPathFollowingComponent()->PauseMove();
		return;
	}
	ActiveJumpRef = FindJumpLinkRef(Destination);
	const FNavPathSharedPtr Path = GetPathFollowingComponent()->GetPath();
	const ARecastNavMesh* NavMesh = Path.IsValid() ? Cast<ARecastNavMesh>(Path->GetNavigationDataUsed()) : nullptr;
	FVector LinkEnd;
	if (!Link || IsPathJumpBlocked() || ControlledCharacter->IsMovementBlocked()
		|| ActiveJumpRef == INVALID_NAVNODEREF
		|| !NavMesh || !NavMesh->GetLinkEndPoints(ActiveJumpRef, JumpTakeoffFeet, LinkEnd))
	{
		if (ActiveJumpRef != INVALID_NAVNODEREF)
		{
			FailedJumpLinks.Add(ActiveJumpRef, GetWorld()->GetTimeSeconds() + 1.f);
		}
		bNavigationAbortPending = true;
		GetPathFollowingComponent()->PauseMove();
		return;
	}
	if (FVector::DistSquared(JumpTakeoffFeet, Destination) < FVector::DistSquared(LinkEnd, Destination))
	{
		Swap(JumpTakeoffFeet, LinkEnd);
	}
	UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
	SavedAirControl = Movement->AirControl;
	SavedFallingFriction = Movement->FallingLateralFriction;
	SavedFallingBraking = Movement->BrakingDecelerationFalling;
	SavedBrakingFriction = Movement->BrakingFriction;
	Movement->StopActiveMovement();
	ControlledCharacter->ConsumeMovementInputVector();
	GetPathFollowingComponent()->PauseMove(FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Reset);
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SuspendCrowdSteering(true);
	}
	ActiveJumpLink = Link;
	JumpLinkDestination = Destination;
	bExecutingPathJump = true;
	bApproachingJumpStart = true;
	JumpTimeRemaining = 1.f;
}

// 자동 링크 목적지로 점프 발사
void APFEnemyAIController::LaunchNavigationJump()
{
	UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
	if (IsPathJumpBlocked() || ControlledCharacter->IsMovementBlocked() || !ControlledCharacter->CanJump())
	{
		FailedJumpLinks.Add(ActiveJumpRef, GetWorld()->GetTimeSeconds() + 1.f);
		Movement->StopMovementImmediately();
		bNavigationAbortPending = true;
		return;
	}
	JumpLandingFeet = JumpLinkDestination;
	FVector LaunchVelocity;
	float FlightTime;
	UPFNavLinkProxy::CalculateJump(Movement->GetActorFeetLocation(), JumpLandingFeet, Movement->MaxWalkSpeed,
		Movement->JumpZVelocity, FMath::Abs(Movement->GetGravityZ()), LaunchVelocity, FlightTime);
	bApproachingJumpStart = false;
	Movement->AirControl = 0.f;
	Movement->FallingLateralFriction = 0.f;
	Movement->BrakingDecelerationFalling = 0.f;
	Movement->BrakingFriction = 0.f;
	Movement->StopActiveMovement();
	ControlledCharacter->ConsumeMovementInputVector();
	JumpTimeRemaining = FMath::Max(2.5f, FlightTime + 0.5f);
	// 점프 발사 방향으로 수평 회전
	const FVector HorizontalLaunchVelocity(LaunchVelocity.X, LaunchVelocity.Y, 0.f);
	if (!HorizontalLaunchVelocity.IsNearlyZero())
	{
		ControlledCharacter->SetActorRotation(HorizontalLaunchVelocity.Rotation());
	}
	ControlledCharacter->LaunchCharacter(LaunchVelocity, true, true);
}

// 점프 중 이동 설정 복원
void APFEnemyAIController::RestoreJumpMovement()
{
	if (!bExecutingPathJump)
	{
		return;
	}
	bExecutingPathJump = false;
	bApproachingJumpStart = false;
	if (ControlledCharacter.IsValid())
	{
		UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
		Movement->AirControl = SavedAirControl;
		Movement->FallingLateralFriction = SavedFallingFriction;
		Movement->BrakingDecelerationFalling = SavedFallingBraking;
		Movement->BrakingFriction = SavedBrakingFriction;
		Movement->PendingLaunchVelocity = FVector::ZeroVector;
	}
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SuspendCrowdSteering(false);
	}
	ActiveJumpLink.Reset();
	ActiveJumpRef = INVALID_NAVNODEREF;
}

// 실패 링크 재시도 제한, 실제 위치에서 복구 예약
void APFEnemyAIController::FailNavigationJump()
{
	if (ActiveJumpRef != INVALID_NAVNODEREF)
	{
		FailedJumpLinks.Add(ActiveJumpRef, GetWorld()->GetTimeSeconds() + 1.f);
	}
	RestoreJumpMovement();
	RequestNavigationRefresh();
}

// 엔진이 점프 요청을 중단한 경우 복구 예약
void APFEnemyAIController::HandleNavigationJumpFinished(UPFNavLinkProxy* Link)
{
	if (bExecutingPathJump && ActiveJumpLink.Get() == Link)
	{
		if (ActiveJumpRef != INVALID_NAVNODEREF)
		{
			FailedJumpLinks.Add(ActiveJumpRef, GetWorld()->GetTimeSeconds() + 1.f);
		}
		RestoreJumpMovement();
		bNavigationAbortPending = true;
	}
}

// 계획한 착지와 예상하지 않은 낙하 구분
void APFEnemyAIController::HandleCharacterLanded(APFCharacter* LandedCharacter)
{
	if (LandedCharacter != ControlledCharacter.Get() || !HasAuthority() || LandedCharacter->IsDeadCharacter())
	{
		return;
	}
	UCharacterMovementComponent* Movement = LandedCharacter->GetCharacterMovement();
	const FVector Feet = Movement->GetActorFeetLocation();
	bOutOfNavigation = false;
	NoProgressTime = 0.f;
	ProgressOrigin = LandedCharacter->GetActorLocation();
	if (bExecutingPathJump)
	{
		const bool bCorrectLanding = !bApproachingJumpStart && FVector::Dist2D(Feet, JumpLandingFeet) <= 20.f
			&& FMath::Abs(Feet.Z - JumpLandingFeet.Z) <= Movement->MaxStepHeight
			&& Movement->CurrentFloor.IsWalkableFloor()
			&& Movement->CurrentFloor.HitResult.GetComponent()
			&& !Movement->CurrentFloor.HitResult.GetComponent()->IsSimulatingPhysics();
		UPFNavLinkProxy* Link = ActiveJumpLink.Get();
		if (bCorrectLanding && Link)
		{
			RestoreJumpMovement();
			GetPathFollowingComponent()->FinishUsingCustomLink(Link);
			GetPathFollowingComponent()->ResumeMove(ActiveMoveRequest);
			ProgressOrigin = LandedCharacter->GetActorLocation();
			NoProgressTime = 0.f;
			return;
		}
		FailNavigationJump();
	}
	else
	{
		RequestNavigationRefresh();
	}
	NextPathRequestTime = 0.f;
	if (!LandedCharacter->IsMovementBlocked() && IsPlayerTargetValid(TargetCharacter.Get()))
	{
		RefreshMovementPath(TargetCharacter.Get());
	}
}

// ASC 준비 시 이동 태그 구독
void APFEnemyAIController::HandleCharacterReady(APFCharacter* ReadyCharacter)
{
	if (ReadyCharacter != ControlledCharacter.Get())
	{
		return;
	}
	UnbindNavigationTags();
	NavigationASC = ReadyCharacter->GetAbilitySystemComponent();
	if (UAbilitySystemComponent* ASC = NavigationASC.Get())
	{
		JumpTagHandle = ASC->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("Character.Block.Jump")))
			.AddUObject(this, &APFEnemyAIController::HandleNavigationTagChanged);
		MoveTagHandle = ASC->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("Character.Block.Move")))
			.AddUObject(this, &APFEnemyAIController::HandleNavigationTagChanged);
	}
	RequestNavigationRefresh();
}

// 이동 능력 변경 후 봇별 경로 갱신
void APFEnemyAIController::HandleNavigationTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RequestNavigationRefresh();
}

// ASC 이동 태그 구독 해제
void APFEnemyAIController::UnbindNavigationTags()
{
	if (UAbilitySystemComponent* ASC = NavigationASC.Get())
	{
		ASC->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("Character.Block.Jump"))).Remove(JumpTagHandle);
		ASC->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("Character.Block.Move"))).Remove(MoveTagHandle);
	}
	NavigationASC.Reset();
	JumpTagHandle.Reset();
	MoveTagHandle.Reset();
}

// 아직 재시도할 수 없는 링크 스냅샷
void APFEnemyAIController::GetExcludedJumpLinks(TSet<NavNodeRef>& OutLinks) const
{
	OutLinks.Reset();
	const float Now = GetWorld()->GetTimeSeconds();
	for (const TPair<NavNodeRef, float>& Pair : FailedJumpLinks)
	{
		if (Pair.Value > Now)
		{
			OutLinks.Add(Pair.Key);
		}
	}
}

// 전투 대기와 다른 층 접근 구분
bool APFEnemyAIController::IsTargetAtMovementHeight(const APFCharacter* Target) const
{
	return Target && ControlledCharacter.IsValid()
		&& FMath::Abs(Target->GetCharacterMovement()->GetActorFeetLocation().Z
			- ControlledCharacter->GetCharacterMovement()->GetActorFeetLocation().Z)
			<= ControlledCharacter->GetCharacterMovement()->MaxStepHeight;
}

// 경로 점프 차단 여부 조회
bool APFEnemyAIController::IsPathJumpBlocked() const
{
	const UAbilitySystemComponent* ASC = ControlledCharacter.IsValid() ? ControlledCharacter->GetAbilitySystemComponent() : nullptr;
	return bRetreating || (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.Block.Jump"))));
}

// 이동 방향으로 회전 보간
void APFEnemyAIController::RotateEnemyTowards(const FVector& WorldDirection, float DeltaTime)
{
	FVector HorizontalDirection = WorldDirection;
	HorizontalDirection.Z = 0.f;
	if (HorizontalDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation = HorizontalDirection.Rotation();
	ControlledCharacter->SetActorRotation(FMath::RInterpTo(ControlledCharacter->GetActorRotation(), TargetRotation, DeltaTime, RotationInterpSpeed));
}

// 이동 방향, 애니메이션 반영
void APFEnemyAIController::SetEnemyDirection(EPFDirection NewDirection)
{
	if (ControlledCharacter.IsValid())
	{
		ControlledCharacter->SetAIMovementDirection(NewDirection);
	}
}

// 공격, 이동 의도 해제
void APFEnemyAIController::ClearEnemyIntent()
{
	if (!HasAuthority() || !ControlledCharacter.IsValid())
	{
		return;
	}

	ControlledCharacter->SetAIAttackCommand(false, false);
	if (bExecutingPathJump && !ControlledCharacter->IsDeadCharacter())
	{
		bNavigationRefreshRequested = true;
		return;
	}
	ClearMovementPath();
	NoProgressTime = 0.f;
	ProgressOrigin = ControlledCharacter->GetActorLocation();
	ControlledCharacter->ConsumeMovementInputVector();
	SetEnemyDirection(IDLE);

	// 수직 속도를 유지하며 추적 이동 중단
	if (UCharacterMovementComponent* EnemyMovementComponent = ControlledCharacter->GetCharacterMovement())
	{
		EnemyMovementComponent->StopActiveMovement();
		EnemyMovementComponent->Velocity.X = 0.f;
		EnemyMovementComponent->Velocity.Y = 0.f;
		EnemyMovementComponent->PendingLaunchVelocity = FVector::ZeroVector;
		EnemyMovementComponent->UpdateComponentVelocity();
	}
}

// 대상까지 시야 확인
bool APFEnemyAIController::HasClearSightToTarget(const APFCharacter* Target) const
{
	if (!ControlledCharacter.IsValid() || !IsPlayerTargetValid(Target))
	{
		return false;
	}

	const FVector SightStart = ControlledCharacter->GetPawnViewLocation();
	const FVector SightEnd = Target->GetPawnViewLocation();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams SightQueryParams(SCENE_QUERY_STAT(EnemyTwinblastSight), false, ControlledCharacter.Get());
	SightQueryParams.AddIgnoredActor(ControlledCharacter.Get());
	FHitResult SightHit;
	const bool bHasBlockingHit = World->LineTraceSingleByChannel(
		SightHit,
		SightStart,
		SightEnd,
		ECC_Visibility,
		SightQueryParams);

	return !bHasBlockingHit || SightHit.GetActor() == Target;
}

// 대상 방향의 조준 피치 조회
float APFEnemyAIController::GetAimPitch() const
{
	const APFCharacter* ControlledPawn = ControlledCharacter.Get();
	const APFCharacter* Target = TargetCharacter.Get();
	if (!ControlledPawn || !IsPlayerTargetValid(Target))
	{
		return 0.f;
	}
	const FVector Direction = Target->GetActorLocation() - ControlledPawn->GetActorLocation();
	return Direction.IsNearlyZero() ? 0.f
		: (Direction.Rotation() - ControlledPawn->GetActorRotation()).GetNormalized().Pitch;
}
