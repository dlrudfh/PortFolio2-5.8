#include "Character/PFPlayer.h"
#include "GAS/PFGameplayTags.h"

#include "UI/HUD/PFCharacterWidget.h"
#include "Props/PFChest.h"
#include "UI/HUD/PFCrosshairWidget.h"
#include "Character/PFEnemy.h"
#include "Character/Kwang/PFEnemyKwang.h"
#include "Character/TwinBlast/PFEnemyTwinblast.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "System/Framework/PFGameMode.h"
#include "System/Framework/PFPlayerController.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

APFPlayer::APFPlayer()
	: CurrentControlMode(DefaultControlMode)
	, ViewpointFixed(true)
	, NearestChest(nullptr)
	, UpDownDir(IDLE)
	, LeftRightDir(IDLE)
{
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SPRINGARM"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CAMERA"));

	SpringArm->SetupAttachment(GetCapsuleComponent());
	Camera->SetupAttachment(SpringArm);
	SpringArm->TargetArmLength = 400.f;
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));

	ArmRotationSpeed = 10.f;

	static ConstructorHelpers::FClassFinder<UUserWidget> SELFUI(TEXT("/Game/GameData/UI/SelfUI.SelfUI_C"));
	if (SELFUI.Succeeded())
	{
		SelfWidgetClass = SELFUI.Class;
	}
	else
	{
		PFLOG(Warning, TEXT("SelfUI Failed"));
	}

	static ConstructorHelpers::FClassFinder<UGameplayAbility> JUMPABILITY(TEXT("/Game/GameData/Abilities/CharacterJump.CharacterJump_C"));
	if (JUMPABILITY.Succeeded())
	{
		JumpAbilityClass = JUMPABILITY.Class;
	}
	else
	{
		PFLOG(Warning, TEXT("CharacterJump Ability Failed"));
	}

	HPPotionUseNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Healing_1.NS_Healing_1"));
	MPPotionUseNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Energy_2.NS_Energy_2"));
	ShieldUseNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Shield_2.NS_Shield_2"));
	CoinUseNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_CoinBurst.NS_CoinBurst"));

}

void APFPlayer::InitAbilityActorInfo()
{
	Super::InitAbilityActorInfo();
	GiveAbilities();
}

// 점프, 궁극기 어빌리티 부여
void APFPlayer::GiveAbilities()
{
	if (!HasAuthority() || !ASC)
	{
		return;
	}

	if (JumpAbilityClass)
	{
		const FGameplayAbilitySpecHandle JumpAbilityHandle = GetOrGiveAbility(JumpAbilityClass, 0);
		if (!JumpAbilityHandle.IsValid())
		{
			PFLOG(Warning, TEXT("Failed to give CharacterJump ability"));
		}
	}
	else
	{
		PFLOG(Warning, TEXT("JumpAbilityClass is null"));
	}

	if (UltimateAbilityClass)
	{
		const FGameplayAbilitySpecHandle UltimateAbilityHandle = GetOrGiveAbility(UltimateAbilityClass, 1);
		if (!UltimateAbilityHandle.IsValid())
		{
			PFLOG(Warning, TEXT("Failed to give Ultimate ability"));
		}
	}
}

// 서버 제어 모드 변경
void APFPlayer::Server_SetControlMode_Implementation(ECONTROLMODE NewControlMode)
{
	CurrentControlMode = NewControlMode;
	OnRep_CurrentControlMode();
}

// 제어 모드 반영
void APFPlayer::OnRep_CurrentControlMode()
{
	SpringArm->ProbeChannel = ECC_Visibility;

	// 시점 고정 적용
	switch (CurrentControlMode)
	{
	case TOPVIEW:
		break;
	case TPS:
	case FPS:
		ViewpointFix();
		break;
	default:
		break;
	}

	// 모드별 카메라, 회전 설정
	switch (CurrentControlMode)
	{
	case TOPVIEW:
		ArmLengthTo = 800.f;
		ArmRotationTo = FRotator(-45.f, 0.f, 0.f);
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritPitch = false;
		SpringArm->bInheritRoll = true;
		SpringArm->bInheritYaw = true;
		SpringArm->bDoCollisionTest = false;
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = true;
		GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
		PFAnim->SetFPS(false);
		break;
	case TPS:
		ArmLengthTo = 200.f;
		SpringArm->SetRelativeLocation(FVector(0.f, 50.f, 70.f));
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritPitch = true;
		SpringArm->bInheritRoll = true;
		SpringArm->bInheritYaw = true;
		SpringArm->bDoCollisionTest = true;
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = true;
		GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
		PFAnim->SetFPS(false);
		break;
	case FPS:
		ArmLengthTo = 0.f;
		SpringArm->SetRelativeLocation(FVector(20.f, 0.f, 80.f));
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritPitch = true;
		SpringArm->bInheritRoll = true;
		SpringArm->bInheritYaw = true;
		SpringArm->bDoCollisionTest = true;
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = true;
		GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
		PFAnim->SetFPS(IsLocallyControlled());
		break;
	default:
		break;
	}
}

// 점프 입력 시작
void APFPlayer::JumpStart()
{
	JumpButtonHeld = true;
	ActivateJumpAbility();
}

// 점프 입력 해제
void APFPlayer::JumpEnd()
{
	JumpButtonHeld = false;
	StopJumping();
}

// 점프 어빌리티 활성화
void APFPlayer::ActivateJumpAbility()
{
	if (!ASC || !JumpAbilityClass)
	{
		PFLOG(Warning, TEXT("JumpAbility Failed"));
		return;
	}

	ASC->TryActivateAbilityByClass(JumpAbilityClass);
}

void APFPlayer::BeginPlay()
{
	Super::BeginPlay();

	OnRep_CurrentControlMode();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 타이틀에서 플레이어 메시 숨김
	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		GetMesh()->SetVisibility(false);
	}
}

void APFPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseAttackAbilityInput();

	// 로컬 UI 제거
	if (SelfHPBar)
	{
		SelfHPBar->RemoveFromParent();
	}
	if (CrosshairWidget)
	{
		CrosshairWidget->RemoveFromParent();
		CrosshairWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void APFPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();
	OnRep_CurrentControlMode();
}

void APFPlayer::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void APFPlayer::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// 점프 입력 유지 시 착지 후 재점프
	if (PrevMovementMode == MOVE_Falling && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround()
		&& JumpButtonHeld && IsLocallyControlled())
	{
		ActivateJumpAbility();
	}
}

void APFPlayer::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// 시점 고정에 맞춰 회전 방식 설정
	GetCharacterMovement()->bOrientRotationToMovement = !ViewpointFixed;
}

void APFPlayer::SetHPBar()
{
	Super::SetHPBar();

	if (!AttributeSet)
	{
		return;
	}

	// 로컬 HUD, 조준점 생성
	if (IsLocallyControlled() && SelfWidgetClass && !SelfHPBar)
	{
		SelfHPBar = CreateWidget<UPFCharacterWidget>(GetWorld(), SelfWidgetClass);
		bSelfHPBarBound = false;
	}
	if (IsLocallyControlled() && !CrosshairWidget)
	{
		CrosshairWidget = CreateWidget<UPFCrosshairWidget>(GetWorld(), UPFCrosshairWidget::StaticClass());
		if (CrosshairWidget)
		{
			CrosshairWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			CrosshairWidget->AddToViewport(etoi(PLAYERSTAT) + 3);
		}
	}

	// HUD 스탯 연결
	if (SelfHPBar && !bSelfHPBarBound)
	{
		SelfHPBar->BindAttributeSet(AttributeSet, true);
		SelfHPBar->SetPositionInViewport(FVector2D::ZeroVector);
		SelfHPBar->AddToViewport(etoi(PLAYERSTAT));
		bSelfHPBarBound = true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 맵, 소유 여부에 따라 UI 표시
	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		if (SelfHPBar)
		{
			SelfHPBar->SetVisibility(ESlateVisibility::Hidden);
		}
		if (CrosshairWidget)
		{
			CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	else if (IsLocallyControlled())
	{
		if (SelfHPBar)
		{
			SelfHPBar->SetVisibility(ESlateVisibility::Visible);
		}
		if (CrosshairWidget)
		{
			CrosshairWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	else
	{
		if (SelfHPBar)
		{
			SelfHPBar->SetVisibility(ESlateVisibility::Hidden);
		}
		if (CrosshairWidget)
		{
			CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void APFPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 카메라 거리, 회전 갱신
	SpringArm->TargetArmLength = ArmLengthTo;

	if (CurrentControlMode == TOPVIEW)
	{
		SpringArm->SetRelativeRotation(FMath::RInterpTo(SpringArm->GetRelativeRotation(), ArmRotationTo, DeltaTime, ArmRotationSpeed));
	}

	ManageSpeed();

	if (IsLocallyControlled())
	{
		SetDir();
	}

	// 질주 중 회전 방식 조정
	if (ViewpointFixed)
	{
		if (IsSprinting() && !IsAttackCommandActive() && (FinalDir == LEFT || FinalDir == RIGHT))
		{
			GetCharacterMovement()->bOrientRotationToMovement = true;
		}
		else
		{
			GetCharacterMovement()->bOrientRotationToMovement = false;
		}
	}
}

void APFPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 카메라 상하 회전 제한
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->ViewPitchMin = -60.f;
			PlayerController->PlayerCameraManager->ViewPitchMax = 60.f;
		}
	}

	// 행동, 이동 입력 연결
	PlayerInputComponent->BindAction(TEXT("ViewChange"), EInputEvent::IE_Pressed, this, &APFPlayer::ViewChange);
	PlayerInputComponent->BindAction(TEXT("Jump"), EInputEvent::IE_Pressed, this, &APFPlayer::JumpStart);
	PlayerInputComponent->BindAction(TEXT("Jump"), EInputEvent::IE_Released, this, &APFPlayer::JumpEnd);
	PlayerInputComponent->BindAction(TEXT("Attack"), EInputEvent::IE_Pressed, this, &APFPlayer::AttackStart);
	PlayerInputComponent->BindAction(TEXT("Attack"), EInputEvent::IE_Released, this, &APFPlayer::AttackEnd);
	PlayerInputComponent->BindAction(TEXT("Ultimate"), EInputEvent::IE_Pressed, this, &APFPlayer::Ultimate);
	PlayerInputComponent->BindAction(TEXT("Sprint"), EInputEvent::IE_Pressed, this, &APFPlayer::Sprint);
	PlayerInputComponent->BindAction(TEXT("ViewpointFix"), EInputEvent::IE_Pressed, this, &APFPlayer::ViewpointFix);
	PlayerInputComponent->BindAction(TEXT("Interaction"), EInputEvent::IE_Pressed, this, &APFPlayer::Interaction);
	PlayerInputComponent->BindAction(TEXT("Inventory"), EInputEvent::IE_Pressed, this, &APFPlayer::OpenInventory);
	PlayerInputComponent->BindAction(TEXT("ChangeCharacter"), EInputEvent::IE_Pressed, this, &APFPlayer::ChangeCharacter);
	PlayerInputComponent->BindAction(TEXT("SpawnTestTwinblastEnemy"), EInputEvent::IE_Pressed, this, &APFPlayer::SpawnTestTwinblastEnemy);
	PlayerInputComponent->BindAction(TEXT("SpawnTestKwangEnemy"), EInputEvent::IE_Pressed, this, &APFPlayer::SpawnTestKwangEnemy);

	PlayerInputComponent->BindAxis(TEXT("UpDown"), this, &APFPlayer::UpDown);
	PlayerInputComponent->BindAxis(TEXT("LeftRight"), this, &APFPlayer::LeftRight);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APFPlayer::LookUp);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APFPlayer::Turn);
}

// 체력 회복
void APFPlayer::GetHP(float Value)
{
	if (HasAuthority())
	{
		FPFGE_StatGameplayEffects::ApplyHeal(ASC, Value);
	}
}

// 마나 회복
void APFPlayer::GetMP(float Value)
{
	if (HasAuthority())
	{
		FPFGE_StatGameplayEffects::ApplyManaRestore(ASC, Value);
	}
}

// 실드 적용
void APFPlayer::GetShield()
{
	if (HasAuthority())
	{
		FPFGE_StatGameplayEffects::ApplyShield(ASC);
	}
}

// 코인 획득
void APFPlayer::GetCoin(float Value)
{
	if (HasAuthority())
	{
		FPFGE_StatGameplayEffects::ApplyCoin(ASC, Value);
	}
}

// 아이템 획득 이펙트 전파
void APFPlayer::PlayPickupNiagara(ENIAGARAID PickupNiagara)
{
	Multicast_Niagara(PickupNiagara);
}

// 마나 조회
float APFPlayer::GetMana() const
{
	return AttributeSet ? AttributeSet->GetMana() : 0.f;
}

// 마나 소모 시도
bool APFPlayer::TryUseMana(float ManaCost)
{
	return HasAuthority() && FPFGE_StatGameplayEffects::TryApplyManaCost(ASC, AttributeSet, ManaCost);
}

// 캐릭터 교체용 상태 저장
FPFCharacterSharedStateSnapshot APFPlayer::CreateSharedStateSnapshot() const
{
	FPFCharacterSharedStateSnapshot Snapshot;
	Snapshot.ControlMode = CurrentControlMode;
	Snapshot.bViewpointFixed = ViewpointFixed;
	Snapshot.FinalDirection = FinalDir;

	// 카메라 상태 저장
	if (SpringArm)
	{
		Snapshot.SpringArmLength = SpringArm->TargetArmLength;
		Snapshot.SpringArmRelativeLocation = SpringArm->GetRelativeLocation();
		Snapshot.SpringArmRelativeRotation = SpringArm->GetRelativeRotation();
	}

	Snapshot.DesiredSpringArmLength = ArmLengthTo;
	Snapshot.DesiredSpringArmRotation = ArmRotationTo;

	if (Camera)
	{
		Snapshot.CameraFieldOfView = Camera->FieldOfView;
	}

	return Snapshot;
}

// 캐릭터 교체 후 상태 복원
void APFPlayer::RestoreSharedStateSnapshot(const FPFCharacterSharedStateSnapshot& Snapshot)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentControlMode = Snapshot.ControlMode;
	FinalDir = Snapshot.FinalDirection;

	// 이동 입력, 속도 초기화
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
	}

	// 제어 설정 반영
	ViewpointFixed = true;
	OnRep_CurrentControlMode();
	ViewpointFixed = Snapshot.bViewpointFixed;
	OnRep_ViewpointFixed();
	OnRep_FinalDir();


	// 카메라 상태 복원
	ArmLengthTo = Snapshot.DesiredSpringArmLength;
	ArmRotationTo = Snapshot.DesiredSpringArmRotation;

	if (SpringArm)
	{
		SpringArm->TargetArmLength = Snapshot.SpringArmLength;
		SpringArm->SetRelativeLocation(Snapshot.SpringArmRelativeLocation);
		SpringArm->SetRelativeRotation(Snapshot.SpringArmRelativeRotation);
	}

	if (Camera)
	{
		Camera->SetFieldOfView(Snapshot.CameraFieldOfView);
	}
}

void APFPlayer::Jump()
{
	if (IsDeadCharacter())
	{
		return;
	}

	Super::Jump();
}

// 질주, 공격 상태에 맞춰 속도 조정
void APFPlayer::ManageSpeed()
{
	if (!GetMovementComponent()->IsFalling())
	{
		if (IsSprinting() && !IsAttackCommandActive())
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		}

		// 시점 고정 중 후진 질주 감속
		if (IsSprinting() && ViewpointFixed && etoi(FinalDir) >= etoi(BWD))
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed * 0.5f;
		}
	}
}

// 이동 방향 갱신 요청
void APFPlayer::SetDir()
{
	if (IsDeadCharacter() || IsMovementBlocked())
	{
		return;
	}

	Server_SetDir();
}

// 서버 이동 방향 갱신
void APFPlayer::Server_SetDir_Implementation()
{
	// 이동 차단 시 방향 초기화
	if (IsMovementBlocked())
	{
		UpDownDir = IDLE;
		LeftRightDir = IDLE;
		FinalDir = IDLE;
		PFAnim->SetCurrentDir(IDLE);
		return;
	}

	// 입력 방향 합산, 제어 모드 보정
	FinalDir = eAdd(UpDownDir, LeftRightDir);

	switch (CurrentControlMode)
	{
	case TOPVIEW:
		if (!ViewpointFixed)
		{
			FinalDir = FinalDir == IDLE ? IDLE : FWD;
		}
		break;
	case FPS:
		if (IsLocallyControlled())
		{
			FinalDir = FinalDir == IDLE ? IDLE : FWD;
		}
		break;
	default:
		break;
	}

	// 애니메이션 반영, 입력 방향 초기화
	PFAnim->SetCurrentDir(FinalDir);
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
}

void APFPlayer::OnRep_FinalDir()
{
	// 제어 모드에 맞춰 방향 보정
	switch (CurrentControlMode)
	{
	case TOPVIEW:
		if (!ViewpointFixed)
		{
			FinalDir = FinalDir == IDLE ? IDLE : FWD;
		}
		break;
	case FPS:
		if (IsLocallyControlled())
		{
			FinalDir = FinalDir == IDLE ? IDLE : FWD;
		}
		break;
	default:
		break;
	}

	Super::OnRep_FinalDir();
}

// 메시 부착 이펙트 생성
UNiagaraComponent* APFPlayer::SpawnAttachedNiagara(UNiagaraSystem* NiagaraSystem)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(NiagaraSystem) || !IsValid(GetMesh()))
	{
		return nullptr;
	}

	UNiagaraComponent* SpawnedNiagaraCom = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem,
		GetMesh(),
		NAME_None,
		FVector(0.f, 0.f, 50.f),
		FRotator::ZeroRotator,
		FVector(2.f),
		EAttachLocation::KeepRelativeOffset,
		true,
		ENCPoolMethod::None,
		true,
		true);

	if (!IsValid(SpawnedNiagaraCom))
	{
		return nullptr;
	}

	SpawnedNiagaraCom->SetCustomTimeDilation(2.f);
	return SpawnedNiagaraCom;
}

// 아이템 획득 이펙트 재생
void APFPlayer::Multicast_Niagara_Implementation(ENIAGARAID NiagaraID)
{
	SpawnAttachedNiagara(GETNIAGARA(NiagaraID));
}

// 체력 회복 이펙트 재생
void APFPlayer::GameplayCue_Item_Use_HP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		SpawnAttachedNiagara(HPPotionUseNiagara);
	}
}

// 마나 회복 이펙트 재생
void APFPlayer::GameplayCue_Item_Use_MP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		SpawnAttachedNiagara(MPPotionUseNiagara);
	}
}

// 실드 이펙트 시작, 종료
void APFPlayer::GameplayCue_Item_Use_Shield(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Removed)
	{
		if (IsValid(ShieldNiagaraCom))
		{
			ShieldNiagaraCom->DeactivateImmediate();
			ShieldNiagaraCom = nullptr;
		}
		return;
	}

	if ((EventType == EGameplayCueEvent::OnActive || EventType == EGameplayCueEvent::WhileActive)
		&& !IsValid(ShieldNiagaraCom))
	{
		ShieldNiagaraCom = SpawnAttachedNiagara(ShieldUseNiagara);
	}
}

// 코인 획득 이펙트 재생
void APFPlayer::GameplayCue_Item_Use_Coin(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		SpawnAttachedNiagara(CoinUseNiagara);
	}
}

// 전후 이동 입력 처리
void APFPlayer::UpDown(float NewAxisValue)
{
	if (IsDeadCharacter() || IsMovementBlocked())
	{
		return;
	}

	AddMovementInput(FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X), NewAxisValue);
	Server_UpDown(NewAxisValue);
}

// 서버 전후 입력 방향 갱신
void APFPlayer::Server_UpDown_Implementation(float NewAxisValue)
{
	if (IsMovementBlocked())
	{
		UpDownDir = IDLE;
		return;
	}

	if (NewAxisValue > 0.f)
	{
		UpDownDir = FWD;
	}
	else if (NewAxisValue < 0.f)
	{
		UpDownDir = BWD;
	}
}

// 좌우 이동 입력 처리
void APFPlayer::LeftRight(float NewAxisValue)
{
	if (IsDeadCharacter() || IsMovementBlocked())
	{
		return;
	}

	AddMovementInput(FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y), NewAxisValue);
	Server_LeftRight(NewAxisValue);
}

// 서버 좌우 입력 방향 갱신
void APFPlayer::Server_LeftRight_Implementation(float NewAxisValue)
{
	if (IsMovementBlocked())
	{
		LeftRightDir = IDLE;
		return;
	}

	if (NewAxisValue > 0.f)
	{
		LeftRightDir = RIGHT;
	}
	else if (NewAxisValue < 0.f)
	{
		LeftRightDir = LEFT;
	}
}

// 카메라 상하 회전
void APFPlayer::LookUp(float NewAxisValue)
{
	if (IsDeadCharacter())
	{
		return;
	}

	if (CurrentControlMode == TPS)
	{
		AddControllerPitchInput(NewAxisValue * LookUpSpeed);
	}
}

// 카메라 좌우 회전
void APFPlayer::Turn(float NewAxisValue)
{
	if (!IsDeadCharacter())
	{
		AddControllerYawInput(NewAxisValue * TurnSpeed);
	}
}

// 공격 입력 시작
void APFPlayer::AttackStart()
{
	if (IsDeadCharacter())
	{
		return;
	}
	Attack();
}

// 공격 입력 해제
void APFPlayer::AttackEnd()
{
	ReleaseAttackAbilityInput();
}

void APFPlayer::Attack()
{
	if (IsDeadCharacter() || !IsLocallyControlled())
	{
		return;
	}

	PressAttackAbilityInput();
}

void APFPlayer::SetBlockTags(bool bBlocked)
{
	Super::SetBlockTags(bBlocked);

	// 차단 해제 시 유지된 공격 입력 재시도
	if (!bBlocked && HasAuthority() && GetPlayerState<APFPlayerState>() && ASC)
	{
		const TSubclassOf<UGameplayAbility> InputAbilityClass = GetAttackAbilityClass();
		FGameplayAbilitySpec* AttackSpec = PressedAttackAbilityHandle.IsValid()
			? ASC->FindAbilitySpecFromHandle(PressedAttackAbilityHandle)
			: (InputAbilityClass ? ASC->FindAbilitySpecFromClass(InputAbilityClass) : nullptr);

		if (AttackSpec && AttackSpec->InputPressed && !AttackSpec->IsActive())
		{
			ASC->TryActivateAbility(AttackSpec->Handle);
		}
	}
}

bool APFPlayer::IsAttackCommandActive() const
{
	if (!ASC)
	{
		return false;
	}

	if (!HasAuthority() && !IsLocallyControlled())
	{
		return ASC->HasMatchingGameplayTag(PFGameplayTags::Character_State_Attacking);
	}

	// 공격 Spec의 입력 상태 조회
	const TSubclassOf<UGameplayAbility> InputAbilityClass = GetAttackAbilityClass();
	const FGameplayAbilitySpec* AttackSpec = PressedAttackAbilityHandle.IsValid()
		? ASC->FindAbilitySpecFromHandle(PressedAttackAbilityHandle)
		: (InputAbilityClass ? ASC->FindAbilitySpecFromClass(InputAbilityClass) : nullptr);
	return AttackSpec && AttackSpec->InputPressed;
}

// 공격 Spec에 누름 입력 전달
void APFPlayer::PressAttackAbilityInput()
{
	if (!IsLocallyControlled() || !ASC)
	{
		return;
	}

	const TSubclassOf<UGameplayAbility> InputAbilityClass = GetAttackAbilityClass();
	if (!InputAbilityClass)
	{
		return;
	}

	FGameplayAbilitySpec* AttackSpec = ASC->FindAbilitySpecFromClass(InputAbilityClass);
	if (!AttackSpec)
	{
		return;
	}

	// 해제할 Spec 보관, 누름 입력 반영
	PressedAttackAbilityHandle = AttackSpec->Handle;
	ASC->AbilitySpecInputPressed(*AttackSpec);

	// 권한에 따라 공격 활성화, 입력 전송
	if (HasAuthority())
	{
		if (!AttackSpec->IsActive())
		{
			ASC->TryActivateAbility(AttackSpec->Handle);
		}
		return;
	}

	ASC->TryActivateAbility(AttackSpec->Handle);
	ASC->ServerSetInputPressed(AttackSpec->Handle);
}

// 공격 Spec에 해제 입력 전달
void APFPlayer::ReleaseAttackAbilityInput()
{
	if (!IsLocallyControlled() || !ASC)
	{
		PressedAttackAbilityHandle = FGameplayAbilitySpecHandle();
		return;
	}

	// 누를 때 저장한 Spec 우선 조회
	const TSubclassOf<UGameplayAbility> InputAbilityClass = GetAttackAbilityClass();
	FGameplayAbilitySpec* AttackSpec = PressedAttackAbilityHandle.IsValid()
		? ASC->FindAbilitySpecFromHandle(PressedAttackAbilityHandle)
		: (InputAbilityClass ? ASC->FindAbilitySpecFromClass(InputAbilityClass) : nullptr);
	PressedAttackAbilityHandle = FGameplayAbilitySpecHandle();
	if (!AttackSpec)
	{
		return;
	}

	// 로컬, 서버 입력 해제
	ASC->AbilitySpecInputReleased(*AttackSpec);
	if (!HasAuthority())
	{
		ASC->ServerSetInputReleased(AttackSpec->Handle);
	}
}

// 서버 조준점 갱신
void APFPlayer::Server_UpdateAimPoint_Implementation(FVector AimPoint_Client)
{
	AimPoint = AimPoint_Client;
}

// 화면 중앙의 조준 위치 계산
FVector APFPlayer::CalculateAimPoint() const
{
	constexpr float MaxAimTraceDistance = 4000.f;
	FVector TraceStart = GetPawnViewLocation();
	FVector TraceDirection = GetBaseAimRotation().Vector();

	// 화면 중앙을 월드 조준선으로 변환
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		FVector ScreenCenterLocation;
		FVector ScreenCenterDirection;

		if (PlayerController->DeprojectScreenPositionToWorld(static_cast<float>(ViewportSizeX) * 0.5f,
			static_cast<float>(ViewportSizeY) * 0.5f, ScreenCenterLocation, ScreenCenterDirection))
		{
			TraceStart = ScreenCenterLocation;
			TraceDirection = ScreenCenterDirection;
		}
	}

	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * MaxAimTraceDistance;
	ECollisionChannel AimTraceChannel;
	if (!GetCollisionChannel(PFCollisionChannelNames::AimTrace, AimTraceChannel))
	{
		return TraceEnd;
	}

	// 자신, 부착 액터를 조준 검사에서 제외
	FCollisionQueryParams AimTraceParams(SCENE_QUERY_STAT(TwinBlastAimTrace), false, this);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	AimTraceParams.AddIgnoredActors(AttachedActors);
	AimTraceParams.AddIgnoredActor(this);

	// 조준선 충돌 위치 반환
	FHitResult AimHit;
	if (UWorld* World = GetWorld();
		World && World->LineTraceSingleByChannel(AimHit, TraceStart, TraceEnd, AimTraceChannel, AimTraceParams))
	{
		return AimHit.ImpactPoint;
	}

	return TraceEnd;
}

// 궁극기 어빌리티 활성화
void APFPlayer::Ultimate()
{
	if (!ASC || !UltimateAbilityClass)
	{
		return;
	}

	ASC->TryActivateAbilityByClass(UltimateAbilityClass);
}

// 서버 궁극기 활성화
void APFPlayer::Server_Ultimate_Implementation()
{
	if (ASC && UltimateAbilityClass)
	{
		ASC->TryActivateAbilityByClass(UltimateAbilityClass);
	}
}

// 질주 전환 요청
void APFPlayer::Sprint()
{
	if (IsDeadCharacter() || IsMovementBlocked())
	{
		return;
	}

	if (CurrentControlMode == FPS && !IsSprinting())
	{
		return;
	}

	Server_Sprint();
}

// 서버 질주 상태 전환
void APFPlayer::Server_Sprint_Implementation()
{
	if (IsMovementBlocked())
	{
		return;
	}

	SetReplicatedStateTag(PFGameplayTags::Character_State_Sprinting, !IsSprinting());
}

// 질주 상태 조회
bool APFPlayer::IsSprinting() const
{
	return HasStateTag(PFGameplayTags::Character_State_Sprinting);
}

// 시점 고정 전환 요청
void APFPlayer::ViewpointFix()
{
	if (IsDeadCharacter())
	{
		return;
	}

	if ((CurrentControlMode == TPS || CurrentControlMode == FPS) && ViewpointFixed)
	{
		return;
	}

	Server_ViewpointFix();
}

// 서버 시점 고정 전환
void APFPlayer::Server_ViewpointFix_Implementation()
{
	// 시점 고정 시 질주 해제
	if (!ViewpointFixed && IsSprinting())
	{
		Server_Sprint();
	}

	ViewpointFixed = !ViewpointFixed;
	GetCharacterMovement()->bOrientRotationToMovement = !ViewpointFixed;
}

// 시점 고정에 따른 회전 방식 반영
void APFPlayer::OnRep_ViewpointFixed()
{
	GetCharacterMovement()->bOrientRotationToMovement = !ViewpointFixed;
}

// 상호작용 대상 상자 갱신
void APFPlayer::SetChest(APFChest* Chest)
{
	if (!Chest && IsValid(NearestChest) && IsValid(NearestChest->Trigger)
		&& NearestChest->Trigger->IsOverlappingActor(this))
	{
		return;
	}

	NearestChest = Chest;
}

// 상자 상호작용 요청
void APFPlayer::Interaction()
{
	if (IsDeadCharacter())
	{
		return;
	}
	Server_Interaction();
}

// 상호작용 RPC 허용
bool APFPlayer::Server_Interaction_Validate()
{
	return true;
}

// 서버에서 상자 열기
void APFPlayer::Server_Interaction_Implementation()
{
	if (!HasAuthority() || IsDeadCharacter() || !IsValid(NearestChest)
		|| !IsValid(NearestChest->Trigger) || !NearestChest->Trigger->IsOverlappingActor(this))
	{
		return;
	}

	NearestChest->ChestOpen();
}

// 캐릭터 교체 요청
void APFPlayer::ChangeCharacter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!GetCharacterMovement() || GetCharacterMovement()->IsFalling() || HasAirborneTag())
	{
		return;
	}

	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (!LevelName.Contains(TEXT("Title")))
	{
		Server_ChangeCharacter();
	}
}

// 인벤토리 표시 전환
void APFPlayer::OpenInventory()
{
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetController()))
	{
		PFLOG(Warning, TEXT("Inventory Input Detected In Character"));
		PlayerController->ToggleInventory();
	}
	else
	{
		PFLOG(Warning, TEXT("Inventory Input Failed To Find PlayerController"));
	}
}

// 테스트용 트윈블라스트 생성 요청
void APFPlayer::SpawnTestTwinblastEnemy()
{
	if (!IsLocallyControlled() || IsDeadCharacter())
	{
		return;
	}
	Server_SpawnTestEnemy(true);
}

// 테스트용 광 생성 요청
void APFPlayer::SpawnTestKwangEnemy()
{
	if (!IsLocallyControlled() || IsDeadCharacter())
	{
		return;
	}
	Server_SpawnTestEnemy(false);
}

// 서버 테스트 적 생성
void APFPlayer::Server_SpawnTestEnemy_Implementation(bool bSpawnTwinblast)
{
	UWorld* World = GetWorld();
	if (!World || IsDeadCharacter())
	{
		return;
	}

	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		return;
	}

	// 적 종류, 생성 조건 설정
	UClass* EnemyClass = bSpawnTwinblast
		? APFEnemyTwinblast::StaticClass()
		: APFEnemyKwang::StaticClass();
	const APFEnemy* EnemyDefaultObject = EnemyClass ? EnemyClass->GetDefaultObject<APFEnemy>() : nullptr;
	const UCapsuleComponent* EnemyCapsule = EnemyDefaultObject ? EnemyDefaultObject->GetCapsuleComponent() : nullptr;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TestEnemySpawn), false, this);
	QueryParams.AddIgnoredActor(this);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	// 배치 가능한 위치를 찾아 생성
	int32 RemainingAttempts = 30;
	FTransform SpawnTransform;
	while (APFGameMode::FindSpawnTransform(World, EnemyCapsule, QueryParams, RemainingAttempts, false, SpawnTransform))
	{
		if (APFEnemy* SpawnedEnemy = World->SpawnActor<APFEnemy>(
			EnemyClass, SpawnTransform.GetLocation(), SpawnTransform.Rotator(), SpawnParameters))
		{
			PFLOG(Warning, TEXT("Test enemy spawned: %s"), *SpawnedEnemy->GetClass()->GetName());
			return;
		}
	}

	PFLOG(Warning, TEXT("Test enemy spawn failed: no spawnable ground found inside SM_Cube bounds"));
}

// 서버 캐릭터 교체
void APFPlayer::Server_ChangeCharacter_Implementation()
{
	if (!GetCharacterMovement() || GetCharacterMovement()->IsFalling() || HasAirborneTag())
	{
		return;
	}

	AController* MyController = GetController();
	if (!MyController)
	{
		return;
	}

	if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GameMode->ChangeCharacter(MyController);
	}
}

// 탑뷰, 3인칭, 1인칭 전환
void APFPlayer::ViewChange()
{
	if (IsDeadCharacter())
	{
		return;
	}

	switch (CurrentControlMode)
	{
	case TOPVIEW:
		GetController()->SetControlRotation(SpringArm->GetRelativeRotation());
		Server_SetControlMode(TPS);
		break;
	case TPS:
		GetController()->SetControlRotation(GetActorRotation());
		Server_SetControlMode(FPS);
		break;
	case FPS:
		GetController()->SetControlRotation(GetActorRotation());
		Server_SetControlMode(TOPVIEW);
		break;
	default:
		break;
	}
}

void APFPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 제어 모드, 시점 고정 복제 등록
	DOREPLIFETIME(APFPlayer, CurrentControlMode);
	DOREPLIFETIME(APFPlayer, ViewpointFixed);
}
