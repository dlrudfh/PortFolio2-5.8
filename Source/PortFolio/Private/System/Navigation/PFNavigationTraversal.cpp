#include "System/Navigation/PFNavigationTraversal.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	// 캐릭터와 동일한 충돌 응답
	FCollisionQueryParams MakeQuery(const FPFTraversalSettings& Settings)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(PFNavigationTraversal), false, Settings.IgnoredActor);
		Query.bFindInitialOverlaps = true;
		return Query;
	}

	// 보행 가능한 고정 바닥
	bool IsFloor(const FHitResult& Hit, const FPFTraversalSettings& Settings)
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();
		const float FloorZ = Component ? Component->GetWalkableSlopeOverride().ModifyWalkableFloorZ(Settings.WalkableFloorZ)
			: Settings.WalkableFloorZ;
		return Hit.bBlockingHit && !Hit.bStartPenetrating && Hit.ImpactNormal.Z >= FloorZ
			&& Component && !Component->IsSimulatingPhysics();
	}

	// 캡슐 이동 구간 충돌
	bool Sweep(UWorld& World, const FVector& From, const FVector& To, const FPFTraversalSettings& Settings, FHitResult& Hit)
	{
		const FVector Offset(0, 0, Settings.HalfHeight);
		return World.SweepSingleByChannel(Hit, From + Offset, To + Offset, FQuat::Identity, Settings.Channel,
			FCollisionShape::MakeCapsule(Settings.Radius, Settings.HalfHeight), MakeQuery(Settings), Settings.Responses);
	}
}

// 캐릭터 이동, 캡슐 설정 수집
FPFTraversalSettings PFNavigationTraversal::GetSettings(const ACharacter& Character, const UWorld& World)
{
	FPFTraversalSettings Settings;
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	Settings.Radius = Capsule->GetScaledCapsuleRadius();
	Settings.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	Settings.MaxSpeed = Movement->MaxWalkSpeed;
	Settings.JumpSpeed = Movement->JumpZVelocity;
	Settings.Gravity = FMath::Abs(Character.HasAnyFlags(RF_ClassDefaultObject)
		? World.GetWorldSettings()->GetGravityZ() * Movement->GravityScale : Movement->GetGravityZ());
	Settings.StepHeight = Movement->MaxStepHeight;
	Settings.WalkableFloorZ = Movement->GetWalkableFloorZ();
	Settings.bCanWalkOffLedges = Movement->CanWalkOffLedges();
	Settings.Channel = Capsule->GetCollisionObjectType();
	Settings.Responses = FCollisionResponseParams(Capsule->GetCollisionResponseToChannels());
	Settings.IgnoredActor = &Character;
	if (const APhysicsVolume* Volume = Character.GetPhysicsVolume())
	{
		Settings.TerminalSpeed = FMath::Abs(Volume->TerminalVelocity);
	}
	else if (const APhysicsVolume* DefaultVolume = World.GetDefaultPhysicsVolume())
	{
		Settings.TerminalSpeed = FMath::Abs(DefaultVolume->TerminalVelocity);
	}
	return Settings;
}

// 하강 도착 시간, 필요한 수평 속도 계산
bool PFNavigationTraversal::Solve(const FVector& Start, const FVector& End, const FPFTraversalSettings& Settings,
	float VerticalSpeed, FPFTraversalSolution& OutSolution)
{
	OutSolution = FPFTraversalSolution();
	if (Settings.Gravity <= UE_SMALL_NUMBER || Settings.TerminalSpeed <= UE_SMALL_NUMBER || Settings.MaxSpeed <= 0.f)
	{
		return false;
	}
	const double DeltaZ = End.Z - Start.Z;
	const double Discriminant = FMath::Square(VerticalSpeed) - 2.0 * Settings.Gravity * DeltaZ;
	if (Discriminant < 0.0)
	{
		return false;
	}
	const double TerminalTime = FMath::Max(0.0, (static_cast<double>(VerticalSpeed) + Settings.TerminalSpeed) / Settings.Gravity);
	const double TerminalZ = VerticalSpeed * TerminalTime - 0.5 * Settings.Gravity * FMath::Square(TerminalTime);
	const double Time = DeltaZ < TerminalZ
		? TerminalTime + (TerminalZ - DeltaZ) / Settings.TerminalSpeed
		: (VerticalSpeed + FMath::Sqrt(Discriminant)) / Settings.Gravity;
	if (!FMath::IsFinite(Time) || Time <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	FVector Velocity = (End - Start) / Time;
	Velocity.Z = VerticalSpeed;
	if (Velocity.SizeSquared2D() > FMath::Square(Settings.MaxSpeed))
	{
		return false;
	}
	OutSolution.Velocity = Velocity;
	OutSolution.Time = static_cast<float>(Time);
	return true;
}

// 종단 속도를 반영한 탄도 위치
FVector PFNavigationTraversal::Evaluate(const FVector& Start, const FPFTraversalSolution& Solution,
	const FPFTraversalSettings& Settings, float Time)
{
	const double TerminalTime = FMath::Max(0.0, (Solution.Velocity.Z + Settings.TerminalSpeed) / Settings.Gravity);
	const double FallingTime = FMath::Min(static_cast<double>(Time), TerminalTime);
	FVector Position = Start + FVector(Solution.Velocity.X, Solution.Velocity.Y, 0) * Time;
	Position.Z += Solution.Velocity.Z * FallingTime - 0.5 * Settings.Gravity * FallingTime * FallingTime
		- Settings.TerminalSpeed * FMath::Max(0.0, Time - TerminalTime);
	return Position;
}

// 후보 위치의 실제 바닥 확인
bool PFNavigationTraversal::FindFloor(UWorld& World, const FVector& Point, const FPFTraversalSettings& Settings,
	float SearchHeight, FHitResult& OutHit)
{
	return World.LineTraceSingleByChannel(OutHit, Point + FVector(0, 0, SearchHeight),
		Point - FVector(0, 0, SearchHeight), Settings.Channel, MakeQuery(Settings), Settings.Responses)
		&& IsFloor(OutHit, Settings);
}

// 서 있는 캡슐의 공간 확인
bool PFNavigationTraversal::HasClearance(UWorld& World, const FVector& Feet, const FPFTraversalSettings& Settings)
{
	return !World.OverlapBlockingTestByChannel(Feet + FVector(0, 0, Settings.HalfHeight), FQuat::Identity,
		Settings.Channel, FCollisionShape::MakeCapsule(Settings.Radius, Settings.HalfHeight), MakeQuery(Settings), Settings.Responses);
}

// 실제 캡슐의 점프 궤적 검증
bool PFNavigationTraversal::ValidateJump(UWorld& World, const FVector& Start, const FVector& End,
	const FPFTraversalSettings& Settings, FPFTraversalSolution& OutSolution)
{
	if (!Solve(Start, End, Settings, Settings.JumpSpeed, OutSolution)
		|| !HasClearance(World, Start, Settings) || !HasClearance(World, End, Settings))
	{
		return false;
	}
	FHitResult Floor;
	const float FloorSearch = FloorClearance + 2.f + Settings.Radius
		* (1.f / FMath::Max(Settings.WalkableFloorZ, 0.1f) - 1.f);
	if (!FindFloor(World, End, Settings, FloorSearch, Floor))
	{
		return false;
	}
	const float Interval = FMath::Min(1.f / 60.f, FMath::Max(1.f, Settings.Radius * 0.5f) / Settings.MaxSpeed);
	const int32 Steps = FMath::CeilToInt(OutSolution.Time / Interval);
	FVector Previous = Start;
	for (int32 Index = 1; Index <= Steps; ++Index)
	{
		const FVector Next = Evaluate(Start, OutSolution, Settings, OutSolution.Time * Index / Steps);
		FHitResult Hit;
		if (Sweep(World, Previous, Next, Settings, Hit))
		{
			return false;
		}
		Previous = Next;
	}
	return true;
}

// 낙하 목적지에 접근하는 수평 속도
FVector PFNavigationTraversal::GetDropVelocity(const FVector& Feet, const FVector& Destination, float MaxSpeed, float DeltaTime)
{
	const FVector Delta = FVector(Destination.X - Feet.X, Destination.Y - Feet.Y, 0);
	if (Delta.SizeSquared() <= 1.0)
	{
		return FVector::ZeroVector;
	}
	return Delta.GetSafeNormal() * FMath::Min(static_cast<double>(MaxSpeed),
		Delta.Size() / FMath::Max(0.1, static_cast<double>(DeltaTime)));
}

// NavMesh 출발점 기준 낙하 거리, 캡슐 이동 검증
bool PFNavigationTraversal::ValidateDrop(UWorld& World, const FVector& Start, const FVector& End,
	const FPFTraversalSettings& Settings, float& OutDuration)
{
	OutDuration = 0.f;
	if (!Settings.bCanWalkOffLedges || End.Z > Start.Z + UE_KINDA_SMALL_NUMBER || !HasClearance(World, Start, Settings)
		|| !HasClearance(World, End, Settings))
	{
		return false;
	}
	FPFTraversalSolution Falling;
	const bool bHasFall = Solve(Start, End, Settings, 0.f, Falling);
	if (!bHasFall && FMath::Abs(Start.Z - End.Z) > FloorClearance)
	{
		return false;
	}
	const float Limit = FVector::Dist2D(Start, End) / FMath::Max(Settings.MaxSpeed, 1.f)
		+ (bHasFall ? Falling.Time : 0.f) + 2.f;
	const float Step = FMath::Min(1.f / 60.f, FMath::Max(1.f, Settings.Radius * 0.5f) / FMath::Max(Settings.MaxSpeed, 1.f));
	FVector Feet = Start;
	float VerticalSpeed = 0.f;
	for (float Time = Step; Time <= Limit; Time += Step)
	{
		FVector Next = Feet + GetDropVelocity(Feet, End, Settings.MaxSpeed, Step) * Step;
		const float NewSpeed = FMath::Max(-Settings.TerminalSpeed, VerticalSpeed - Settings.Gravity * Step);
		Next.Z += (VerticalSpeed + NewSpeed) * 0.5f * Step;
		VerticalSpeed = NewSpeed;
		FHitResult Hit;
		if (Sweep(World, Feet, Next, Settings, Hit))
		{
			if (IsFloor(Hit, Settings) && FVector::Dist2D(Hit.Location, End) <= 3.f
				&& FMath::Abs(Hit.Location.Z - Settings.HalfHeight + FloorClearance - End.Z) <= FloorClearance)
			{
				OutDuration = Time;
				return true;
			}
			if (!IsFloor(Hit, Settings) || Hit.ImpactPoint.Z + FloorClearance < Start.Z - Settings.StepHeight)
			{
				return false;
			}
			// 출발 바닥과의 접촉은 보행으로 이어지고 중간 발판 착지는 제외
			Feet = Hit.Location - FVector(0, 0, Settings.HalfHeight - FloorClearance);
			VerticalSpeed = 0.f;
		}
		else
		{
			Feet = Next;
		}
		if (Feet.Z < End.Z - Settings.StepHeight)
		{
			return false;
		}
	}
	return false;
}
