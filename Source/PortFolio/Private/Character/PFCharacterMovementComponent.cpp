#include "Character/PFCharacterMovementComponent.h"

#include "Character/PFCharacter.h"
#include "GameFramework/Controller.h"

// 이동 재처리용 회전 상태
class FSavedMove_PFCharacter : public FSavedMove_Character
{
public:
	virtual void Clear() override;
	virtual void SetInitialPosition(ACharacter* Character) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
	virtual void PostUpdate(ACharacter* Character, EPostUpdateMode PostUpdateMode) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* Character, APlayerController* PC, const FVector& OldStartLocation) override;

	float StartCameraRelativeYaw = 0.f;
	float TargetYaw = 0.f;
	bool bStartHasCameraRelativeYaw = false;
	bool bUseRelativeRotation = false;
	bool bMovementRotation = false;
	bool bHoldRotation = false;
	EPFDirection InputDirection = IDLE;
};

// 클라이언트 이동 예측 데이터
class FNetworkPredictionData_Client_PFCharacter : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_PFCharacter(const UCharacterMovementComponent& Movement)
		: FNetworkPredictionData_Client_Character(Movement) {}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_PFCharacter());
	}
};

void FSavedMove_PFCharacter::Clear()
{
	FSavedMove_Character::Clear();
	StartCameraRelativeYaw = 0.f;
	TargetYaw = 0.f;
	bStartHasCameraRelativeYaw = false;
	bUseRelativeRotation = false;
	bMovementRotation = false;
	bHoldRotation = false;
	InputDirection = IDLE;
}

void FSavedMove_PFCharacter::SetInitialPosition(ACharacter* Character)
{
	FSavedMove_Character::SetInitialPosition(Character);
	UPFCharacterMovementComponent* Movement = CastChecked<UPFCharacterMovementComponent>(Character->GetCharacterMovement());
	const APFCharacter* PFCharacter = CastChecked<APFCharacter>(Character);
	InputDirection = PFCharacter->GetMovementInputDirection();
	Movement->RotationInputDirection = InputDirection;
	StartCameraRelativeYaw = Movement->CameraRelativeYaw;
	bStartHasCameraRelativeYaw = Movement->bHasCameraRelativeYaw;
	Movement->GetRotationTarget(InputDirection, bUseRelativeRotation, bMovementRotation, TargetYaw);
	bHoldRotation = bMovementRotation && (InputDirection == IDLE || PFCharacter->IsMovementBlocked());
}

void FSavedMove_PFCharacter::PrepMoveFor(ACharacter* Character)
{
	FSavedMove_Character::PrepMoveFor(Character);
	UPFCharacterMovementComponent* Movement = CastChecked<UPFCharacterMovementComponent>(Character->GetCharacterMovement());
	Movement->CameraRelativeYaw = StartCameraRelativeYaw;
	Movement->bHasCameraRelativeYaw = bStartHasCameraRelativeYaw;
	Movement->RotationController = Character->GetController();
}

void FSavedMove_PFCharacter::PostUpdate(ACharacter* Character, EPostUpdateMode PostUpdateMode)
{
	const FRotator OriginalControlRotation = SavedControlRotation;
	FSavedMove_Character::PostUpdate(Character, PostUpdateMode);
	if (PostUpdateMode == PostUpdate_Replay)
	{
		SavedControlRotation = OriginalControlRotation;
	}
}

uint8 FSavedMove_PFCharacter::GetCompressedFlags() const
{
	return FSavedMove_Character::GetCompressedFlags() | (static_cast<uint8>(InputDirection) << 4);
}

bool FSavedMove_PFCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_PFCharacter* NewPFMove = static_cast<const FSavedMove_PFCharacter*>(NewMove.Get());
	return InputDirection == NewPFMove->InputDirection
		&& bUseRelativeRotation == NewPFMove->bUseRelativeRotation
		&& bMovementRotation == NewPFMove->bMovementRotation
		&& bHoldRotation == NewPFMove->bHoldRotation
		&& FMath::IsNearlyEqual(TargetYaw, NewPFMove->TargetYaw)
		&& FSavedMove_Character::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_PFCharacter::CombineWith(const FSavedMove_Character* OldMove, ACharacter* Character,
	APlayerController* PC, const FVector& OldStartLocation)
{
	FSavedMove_Character::CombineWith(OldMove, Character, PC, OldStartLocation);
	const FSavedMove_PFCharacter* OldPFMove = static_cast<const FSavedMove_PFCharacter*>(OldMove);
	UPFCharacterMovementComponent* Movement = CastChecked<UPFCharacterMovementComponent>(Character->GetCharacterMovement());
	Movement->CameraRelativeYaw = OldPFMove->StartCameraRelativeYaw;
	Movement->bHasCameraRelativeYaw = OldPFMove->bStartHasCameraRelativeYaw;
}

FNetworkPredictionData_Client* UPFCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UPFCharacterMovementComponent* MutableThis = const_cast<UPFCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_PFCharacter(*this);
	}
	return ClientPredictionData;
}

void UPFCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	const uint8 Direction = Flags >> 4;
	RotationInputDirection = Direction < static_cast<uint8>(EDIRECTION_END) ? static_cast<EPFDirection>(Direction) : IDLE;
}

void UPFCharacterMovementComponent::OnTeleported()
{
	Super::OnTeleported();
	bHasCameraRelativeYaw = false;
}

// 입력 기준 목표 회전 조회
void UPFCharacterMovementComponent::GetRotationTarget(EPFDirection Direction, bool& bUseRelativeRotation,
	bool& bMovementRotation, float& TargetYaw) const
{
	const APFCharacter* Character = Cast<APFCharacter>(CharacterOwner);
	bUseRelativeRotation = Character && Character->IsPlayerCharacter() && Character->GetController()
		&& !Character->IsDeadCharacter() && !Character->IsLevelStartActive() && !Character->IsJumpPadFlightActive()
		&& Character->GetCurrentControlMode() != FPS
		&& (!Character->IsViewpointFixed() || !Character->IsAttackCommandActive())
		&& !HasCustomGravity();
	bMovementRotation = false;
	TargetYaw = 0.f;
	if (!bUseRelativeRotation) return;

	bMovementRotation = (Character->GetCurrentControlMode() == TOPVIEW && !Character->IsViewpointFixed())
		|| (Character->IsSprinting() && Character->CanSprint()
			&& !Character->IsAttackCommandActive() && !Character->IsMovementBlocked()
			&& (Direction == LEFT || Direction == RIGHT));
	if (bMovementRotation)
	{
		// 정지 중에는 마지막 이동 방향 유지
		static constexpr float DirectionYaw[] = { 0.f, -90.f, 90.f, 0.f, -45.f, 45.f, 180.f, -135.f, 135.f };
		TargetYaw = Direction == IDLE || Character->IsMovementBlocked()
			? CameraRelativeYaw : DirectionYaw[static_cast<uint8>(Direction)];
	}
}

void UPFCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!HasValidData()) return;
	const APFCharacter* Character = Cast<APFCharacter>(CharacterOwner);
	const FSavedMove_PFCharacter* ReplayedMove = static_cast<const FSavedMove_PFCharacter*>(GetCurrentReplayedSavedMove());
	if (!ReplayedMove && Character && Character->IsLocallyControlled())
	{
		RotationInputDirection = Character->GetMovementInputDirection();
	}

	bool bUseRelativeRotation = false;
	bool bMovementRotation = false;
	float TargetYaw = 0.f;
	GetRotationTarget(RotationInputDirection, bUseRelativeRotation, bMovementRotation, TargetYaw);
	bool bHoldRotation = bMovementRotation && (RotationInputDirection == IDLE || Character->IsMovementBlocked());
	if (ReplayedMove)
	{
		bUseRelativeRotation = ReplayedMove->bUseRelativeRotation;
		bMovementRotation = ReplayedMove->bMovementRotation;
		bHoldRotation = ReplayedMove->bHoldRotation;
		TargetYaw = ReplayedMove->TargetYaw;
	}
	if (!bUseRelativeRotation || !Character || !Character->GetController())
	{
		bHasCameraRelativeYaw = false;
		Super::PhysicsRotation(DeltaTime);
		return;
	}

	const float CameraYaw = ReplayedMove ? ReplayedMove->SavedControlRotation.Yaw : Character->GetControlRotation().Yaw;
	if (bHoldRotation)
	{
		CameraRelativeYaw = FRotator::NormalizeAxis(UpdatedComponent->GetComponentRotation().Yaw - CameraYaw);
		bHasCameraRelativeYaw = true;
		RotationController = Character->GetController();
		return;
	}
	if (!bHasCameraRelativeYaw || RotationController.Get() != Character->GetController())
	{
		CameraRelativeYaw = bMovementRotation
			? FRotator::NormalizeAxis(UpdatedComponent->GetComponentRotation().Yaw - CameraYaw) : 0.f;
		bHasCameraRelativeYaw = true;
		RotationController = Character->GetController();
	}

	// 카메라 회전은 즉시 반영, 이동 방향만 초당 720도 제한
	CameraRelativeYaw = FRotator::NormalizeAxis(FMath::FixedTurn(CameraRelativeYaw, TargetYaw, 720.f * DeltaTime));
	const FRotator DesiredRotation(0.f, FRotator::NormalizeAxis(CameraYaw + CameraRelativeYaw), 0.f);
	if (!UpdatedComponent->GetComponentRotation().Equals(DesiredRotation, 1.e-3f))
	{
		MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);
	}
}
