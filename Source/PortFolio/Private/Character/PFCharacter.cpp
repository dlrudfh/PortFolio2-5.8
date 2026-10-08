#include "Character/PFCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Campaign/PFCampaignDirector.h"
#include "Campaign/PFCampaignEnemyController.h"
#include "Character/PFCharacterMovementComponent.h"
#include "Character/PFFirstPersonMeshComponent.h"
#include "Character/PFShrubStealthComponent.h"

#include "UI/HUD/PFCharacterWidget.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFGameMode.h"
#include "System/Framework/PFSessionGameState.h"
#include "System/Framework/PFEnemyAIController.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Character/PFCombatAimProvider.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

APFCharacter::APFCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPFCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
	, IsAttacking(false)
	, PFAnim(nullptr)
	, FinalDir(IDLE)
{
	PrimaryActorTick.bCanEverTick = true;
	ShrubStealth = CreateDefaultSubobject<UPFShrubStealthComponent>(TEXT("ShrubStealth"));

	GetCapsuleComponent()->InitCapsuleSize(20.f, 88.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), FRotator(0.f, -90.f, 0.f));
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->AirControl = 0.7f;

	OtherHPBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPBAR"));
	OtherHPBar->SetupAttachment(GetMesh());

	static ConstructorHelpers::FClassFinder<UUserWidget> OTHERUI(TEXT("/Game/GameData/UI/OtherUI.OtherUI_C"));
	if (OTHERUI.Succeeded())
	{
		OtherHPBar->InitWidget();
		OtherHPBar->SetWidgetClass(OTHERUI.Class);
		OtherHPBar->SetWidgetSpace(EWidgetSpace::World);
		OtherHPBar->SetDrawAtDesiredSize(true);
		OtherHPBar->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	}
	else
	{
		PFLOG(Warning, TEXT("OtherUI Failed"));
	}

	bReplicates = true;
	SetReplicateMovement(true);
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetIsReplicated(false);


	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = APFEnemyAIController::StaticClass();

	OwnedASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	OwnedASC->SetIsReplicated(true);
	OwnedASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	OwnedAttributeSet = CreateDefaultSubobject<UPFAttributeSet>(TEXT("AttributeSet"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SPRINGARM"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CAMERA"));

	SpringArm->SetupAttachment(GetCapsuleComponent());
	Camera->SetupAttachment(SpringArm);
	SpringArm->TargetArmLength = 400.f;
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));


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

UAbilitySystemComponent* APFCharacter::GetAbilitySystemComponent() const
{
	if (IsPlayerCharacter())
	{
		const APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>();
		if (PFPlayerState)
		{
			return PFPlayerState->GetAbilitySystemComponent();
		}
		return ASC && ASC != OwnedASC && ASC->GetAvatarActor() == this ? ASC : nullptr;
	}
	return IsEnemyCharacter() ? OwnedASC : nullptr;
}

// 상태 태그 조회
bool APFCharacter::HasStateTag(const FGameplayTag& StateTag) const
{
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(StateTag);
}

// 사망 여부 조회
bool APFCharacter::IsDeadCharacter() const
{
	return HasStateTag(PFGameplayTags::Character_State_Dead);
}

// 수풀 은신 여부 조회
bool APFCharacter::IsShrubConcealed() const
{
	return ShrubStealth->IsConcealed();
}

// 공격 시 수풀 은신 해제
void APFCharacter::BreakShrubConcealmentForAttack()
{
	ShrubStealth->BreakForAttack();
}

void APFCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndJumpPadFlight();
	ClearControlCommands();

	// 이벤트 구독 해제
	UnbindStateTagEvents();
	if (AttributeSet)
	{
		AttributeSet->OnHealthIsZero.RemoveAll(this);
	}
	if (PFAnim)
	{
		PFAnim->OnMontageEnded.RemoveDynamic(this, &APFCharacter::OnMontageEnd);
		PFAnim->DeathEnd.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

// 서버 상태 태그 변경 및 복제
void APFCharacter::SetReplicatedStateTag(const FGameplayTag& StateTag, bool bEnabled)
{
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
		{
			AbilitySystem->SetLooseGameplayTagCount(StateTag, bEnabled ? 1 : 0, EGameplayTagReplicationState::TagOnly);
		}
	}
}

// 상태 태그 이벤트 연결
void APFCharacter::BindStateTagEvents()
{
	if (BoundStateASC.Get() == ASC)
	{
		return;
	}
	// ASC 이벤트 구독 갱신
	UnbindStateTagEvents();
	BoundStateASC = ASC;
	DeadStateTagHandle = ASC->RegisterGameplayTagEvent(PFGameplayTags::Character_State_Dead)
		.AddUObject(this, &APFCharacter::DeadTagChanged);
	UltimateStateTagHandle = ASC->RegisterGameplayTagEvent(PFGameplayTags::Character_State_Ultimate)
		.AddUObject(this, &APFCharacter::UltimateTagChanged);

	// 기존 태그 상태 반영
	DeadTagChanged(PFGameplayTags::Character_State_Dead, ASC->GetTagCount(PFGameplayTags::Character_State_Dead));
	if (ASC->HasMatchingGameplayTag(PFGameplayTags::Character_State_Ultimate))
	{
		UltimateTagChanged(PFGameplayTags::Character_State_Ultimate, ASC->GetTagCount(PFGameplayTags::Character_State_Ultimate));
	}
}

// 상태 태그 이벤트 해제
void APFCharacter::UnbindStateTagEvents()
{
	// 기존 ASC의 콜백 해제
	if (UAbilitySystemComponent* AbilitySystem = BoundStateASC.Get())
	{
		AbilitySystem->UnregisterGameplayTagEvent(DeadStateTagHandle, PFGameplayTags::Character_State_Dead);
		AbilitySystem->UnregisterGameplayTagEvent(UltimateStateTagHandle, PFGameplayTags::Character_State_Ultimate);
	}
	// 구독 정보 초기화
	DeadStateTagHandle.Reset();
	UltimateStateTagHandle.Reset();
	BoundStateASC.Reset();
}

// 사망 상태 반영
void APFCharacter::DeadTagChanged(FGameplayTag StateTag, int32 NewCount)
{
	ShrubStealth->RefreshState();
	if (NewCount > 0) EndJumpPadFlight();
	SetActorEnableCollision(NewCount == 0);
	if (NewCount > 0)
	{
		PFAnim->Dead();
	}
	UpdateFirstPersonPresentation();
}

// 궁극기 상태 처리 (파생 클래스 구현)
void APFCharacter::UltimateTagChanged(FGameplayTag StateTag, int32 NewCount)
{
}

void APFCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서 등장 연출 상태 시작
	UWorld* World = GetWorld();
	if (HasAuthority()
		&& !FPackageName::GetShortName(World->GetMapName()).Contains(TEXT("Title")))
	{
		bLevelStartActive = true;
		OnRep_LevelStartActive();
		ForceNetUpdate();
	}

	RefreshControlRole();
	SetHPBar();
}

// ASC 연결 및 전투 초기화
void APFCharacter::InitAbilityActorInfo()
{
	if (CharacterRole == EPFCharacterRole::ROLE_END)
	{
		return;
	}
	if (ASC && ASC == GetAbilitySystemComponent() && ASC->GetAvatarActor() == this && bAttributeDelegatesBound)
	{
		SetHPBar();
		OnCharacterReady.Broadcast(this);
		return;
	}
	if (IsPlayerCharacter())
	{
		APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>();
		if (!PFPlayerState) return;
		// PlayerState의 ASC, 스탯 참조
		ASC = PFPlayerState->GetAbilitySystemComponent();
		AttributeSet = PFPlayerState->GetAttributeSet();
		// 플레이어 ASC 연결 및 스탯 초기화
		ASC->InitAbilityActorInfo(PFPlayerState, this);
		PFPlayerState->InitializeGASStats();
	}
	else if (IsEnemyCharacter())
	{
		ASC = OwnedASC;
		AttributeSet = OwnedAttributeSet;
		// 봇 ASC 연결 및 스탯 초기화
		ASC->InitAbilityActorInfo(this, this);
		InitGASStats();
	}
	else
	{
		return;
	}

	// 등장 몽타주에 맞춰 무적 설정
	if (HasAuthority())
	{
		const UAnimMontage* ActiveMontage = PFAnim->GetCurrentActiveMontage();
		const bool bPlayingLevelStart = ActiveMontage && PFAnim->IsLevelStartMontage(ActiveMontage)
			&& PFAnim->Montage_IsPlaying(ActiveMontage);
		ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Invulnerable, bPlayingLevelStart ? 1 : 0);
	}

	// 전투 상태, 이벤트, UI 준비
	GiveAttackAbility();
	GivePlayerAbilities();
	SetBlockTags(bLevelStartActive);
	BindAttributeDelegates();
	SetHPBar();
	BindStateTagEvents();
	OnCharacterReady.Broadcast(this);
}

// 기본 공격 어빌리티 부여
void APFCharacter::GiveAttackAbility()
{
	if (!HasAuthority() || !AttackAbilityClass)
	{
		return;
	}

	const FGameplayAbilitySpecHandle AttackAbilityHandle = GetOrGiveAbility(AttackAbilityClass, 1);
	if (!AttackAbilityHandle.IsValid())
	{
		PFLOG(Warning, TEXT("Failed to give BasicAttack ability"));
	}
}

// 어빌리티 조회 또는 부여
FGameplayAbilitySpecHandle APFCharacter::GetOrGiveAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel)
{
	// 기존 Spec 재사용
	if (FGameplayAbilitySpec* ExistingSpec = ASC->FindAbilitySpecFromClass(AbilityClass))
	{
		return ExistingSpec->Handle;
	}

	// 새 어빌리티 부여
	return ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, AbilityLevel, INDEX_NONE));
}

// 봇 스탯, 마나 재생 초기화
void APFCharacter::InitGASStats()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bOwnedGASStatsInitialized)
	{
		// 1레벨 스탯 조회
		UPFGameInstance* PFGameInstance = Cast<UPFGameInstance>(GetGameInstance());
		constexpr int32 InitialLevel = 1;
		FPFCharacterData* InitialData = PFGameInstance ? PFGameInstance->GetPFCharacterData(InitialLevel) : nullptr;
		if (!InitialData)
		{
			PFLOG(Warning, TEXT("Bot GAS stat initialization failed: level %d data doesn't exist"), InitialLevel);
			return;
		}

		// 초기 스탯 적용
		bOwnedGASStatsInitialized = FPFGE_StatGameplayEffects::InitializeStats(ASC, FPFStatValues(*InitialData));
	}

	// 마나 재생 중복 적용 방지
	if (bOwnedGASStatsInitialized && !ASC->GetActiveGameplayEffect(OwnedManaRegenEffectHandle))
	{
		OwnedManaRegenEffectHandle = FPFGE_StatGameplayEffects::ApplyManaRegen(ASC);
	}
}

// 체력 소진 시 사망 처리 연결
void APFCharacter::BindAttributeDelegates()
{
	if (bAttributeDelegatesBound)
	{
		return;
	}

	AttributeSet->OnHealthIsZero.AddUObject(this, &APFCharacter::Dead);
	bAttributeDelegatesBound = true;
}



// 행동 차단 태그 설정
void APFCharacter::SetBlockTags(bool bBlocked)
{
	if (!ASC) return;
	ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_Block_Attack, bBlocked);
	ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_Block_Jump, bBlocked);
	ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_Block_Move, bBlocked);
	ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_Block_Ultimate, bBlocked);

	// 차단 해제 시 유지된 공격 입력 재시도
	if (!bBlocked && HasAuthority() && IsPlayerCharacter())
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

// 이동 차단 여부 조회
bool APFCharacter::IsMovementBlocked() const
{
	if (APFCampaignDirector::BlocksInput(GetController())) return true;
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	return bJumpPadFlightActive || (AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(
			PFGameplayTags::Character_Block_Move));
}

// 공중 상태 태그 조회
bool APFCharacter::HasAirborneTag() const
{
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	return AbilitySystemComponent
		&& (AbilitySystemComponent->HasMatchingGameplayTag(
			PFGameplayTags::Character_State_Jumping)
			|| AbilitySystemComponent->HasMatchingGameplayTag(
				PFGameplayTags::Character_State_Falling));
}

// 점프, 낙하 태그 갱신
void APFCharacter::UpdateAirborneTag()
{
	// 상승, 하강 구분
	const UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	const bool bIsAirborne = CharacterMovementComponent->IsFalling();
	const bool bIsJumping = bIsAirborne && GetVelocity().Z > 0.f;
	const bool bIsFalling = bIsAirborne && !bIsJumping;

	// 서버 복제와 소유 클라이언트 예측 반영
	if (ASC && (HasAuthority() || IsLocallyControlled()))
	{
		const EGameplayTagReplicationState ReplicationState = HasAuthority()
			? EGameplayTagReplicationState::TagOnly : EGameplayTagReplicationState::None;
		ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Jumping, bIsJumping ? 1 : 0, ReplicationState);
		ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Falling, bIsFalling ? 1 : 0, ReplicationState);
	}
}

void APFCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 서버 몽타주의 본, 노티파이 갱신
	if (HasAuthority())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
	}

	// 몽타주 종료 이벤트 연결
	PFAnim->OnMontageEnded.AddDynamic(this, &APFCharacter::OnMontageEnd);
	PFAnim->DeathEnd.AddUObject(this, &APFCharacter::HandleDeathAnimationEnd);

	// 캐릭터 충돌 설정
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("PFCharacter"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(GetCollisionChannel(TEXT("Chest")), ECR_Overlap);
	GetCapsuleComponent()->SetCollisionResponseToChannel(GetCollisionChannel(PFCollisionChannelNames::Item), ECR_Overlap);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
}

// 체력바 연결 및 표시 설정
void APFCharacter::SetHPBar()
{
	if (GetNetMode() == NM_DedicatedServer || !AttributeSet)
	{
		return;
	}

	// 체력바 스탯 연결
	UPFCharacterWidget* CharacterWidget = Cast<UPFCharacterWidget>(OtherHPBar->GetUserWidgetObject());
	if (CharacterWidget && !bOtherHPBarBound)
	{
		CharacterWidget->BindAttributeSet(AttributeSet, false);
		bOtherHPBarBound = true;
	}

	UWorld* World = GetWorld();
	// 타이틀, 로컬 캐릭터 체력바 숨김
	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")) || IsLocalPlayerCharacter())
	{
		OtherHPBar->SetVisibility(false);
	}
	else
	{
		OtherHPBar->SetVisibility(true);
	}
}

void APFCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateAirborneTag();
	if (GetNetMode() != NM_DedicatedServer && !IsLocalPlayerCharacter() && OtherHPBar->IsVisible())
	{
		TickHPBar();
	}
	if (!IsPlayerCharacter())
	{
		return;
	}

	// 카메라 거리, 회전 갱신
	SpringArm->TargetArmLength = ArmLengthTo;

	if (CurrentControlMode == TOPVIEW)
	{
		SpringArm->SetRelativeRotation(FMath::RInterpTo(SpringArm->GetRelativeRotation(), ArmRotationTo, DeltaTime, ArmRotationSpeed));
	}

	if (IsLocallyControlled() || HasAuthority())
	{
		SetDir();
	}

	ManageSpeed();

	// 질주 중 회전 방식 조정
	if (ViewpointFixed)
	{
		if (IsSprinting() && CanSprint() && !IsAttackCommandActive() && (FinalDir == LEFT || FinalDir == RIGHT))
		{
			GetCharacterMovement()->bOrientRotationToMovement = true;
		}
		else
		{
			GetCharacterMovement()->bOrientRotationToMovement = false;
		}
	}

}

void APFCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!HasAuthority() || IsDeadCharacter())
	{
		return;
	}
	if (!IsValid(ASC) || !IsValid(AttributeSet))
	{
		Super::FellOutOfWorld(DamageType);
		return;
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 실드, 무적과 무관하게 체력 소진
	AttributeSet->SetHealth(0.f);
	AttributeSet->OnHealthChanged.Broadcast();
	Dead();
	ForceNetUpdate();
}

float APFCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (const APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld()); Director && Director->IsEncounterPaused()) return 0.f;
	const float FinalDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (HasAuthority() && FinalDamage > 0.f && ASC && AttributeSet)
	{
		// 공격 캐릭터 조회
		APFCharacter* SourceCharacter = nullptr;
		if (EventInstigator)
		{
			SourceCharacter = Cast<APFCharacter>(EventInstigator->GetPawn());
		}

		// 공격 출처에 따라 피해 경로 선택
		if (SourceCharacter)
		{
			SourceCharacter->ApplyAttackDamageTo(this, FinalDamage, DamageCauser);
		}
		else if (FPFGE_StatGameplayEffects::ApplyDamage(
			nullptr, ASC, FinalDamage, FinalDamage, EventInstigator, DamageCauser, DamageCauser))
		{
			Multicast_PostHitProcessing();
		}
	}
	return FinalDamage;
}

// GAS 피해 적용
bool APFCharacter::ApplyAttackDamageTo(APFCharacter* Target, float Damage, AActor* Attacker,
	const UGameplayAbility* AttackAbility, const FHitResult* HitResult)
{
	if (APFCampaignDirector::AreFriendly(this, Target)) return false;
	if (const APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld()); Director && Director->IsEncounterPaused()) return false;
	if (!HasAuthority() || !IsValid(Target) || Damage <= 0.f || !ASC || !Target->GetAbilitySystemComponent())
	{
		return false;
	}

	// 기본 공격으로 출처 보완
	const UGameplayAbility* FinalAttackAbility = AttackAbility;
	if (!FinalAttackAbility && AttackAbilityClass)
	{
		if (const FGameplayAbilitySpec* AttackSpec = ASC->FindAbilitySpecFromClass(AttackAbilityClass))
		{
			FinalAttackAbility = AttackSpec->GetPrimaryInstance();
			if (!FinalAttackAbility)
			{
				FinalAttackAbility = AttackSpec->Ability.Get();
			}
		}
	}

	// 피해 적용 및 피격 후처리
	const bool bApplied = FPFGE_StatGameplayEffects::ApplyDamage(ASC, Target->GetAbilitySystemComponent(),
		GetDamage(), Damage, this, Attacker, Attacker, FinalAttackAbility, HitResult);
	if (bApplied)
	{
		if (APFCampaignEnemyController* CampaignAI = Cast<APFCampaignEnemyController>(Target->GetController()))
		{
			CampaignAI->NotifyDamageFrom(this);
		}
		Target->Multicast_PostHitProcessing();
	}
	return bApplied;
}

// 피격 후처리 전파
void APFCharacter::Multicast_PostHitProcessing_Implementation()
{
	PostHitProcessing();
}

// 피격 후처리 (파생 클래스 구현)
void APFCharacter::PostHitProcessing()
{
}

// 조준 피치 반환
float APFCharacter::GetAimPitch() const
{
	if (IsEnemyCharacter())
	{
		if (HasAuthority())
		{
			const APFEnemyAIController* EnemyController = Cast<APFEnemyAIController>(GetController());
			return EnemyController ? EnemyController->GetAimPitch() : 0.f;
		}
		return ReplicatedAimPitch;
	}
	return CurrentControlMode == TOPVIEW ? 0.f : GetBaseAimRotation().GetNormalized().Pitch;
}

// 공격력 반환
float APFCharacter::GetDamage() const
{
	return AttributeSet ? AttributeSet->GetAttackPower() * AttackDamageMultiplier : 0.f;
}

// 튜토리얼 플레이어의 전투 피해 차단
float APFCharacter::GetIncomingDamageMultiplier() const
{
	if (IsPlayerCharacter() && APFSessionGameState::IsTraining(GetWorld())) return 0.f;
	return IncomingDamageMultiplier;
}

// 이동 방향을 애니메이션에 반영
void APFCharacter::OnRep_FinalDir()
{
	if (IsPlayerCharacter())
	{
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
	}
	PFAnim->SetCurrentDir(FinalDir);
}

// 체력바 크기, 방향 갱신
void APFCharacter::TickHPBar()
{
	// 다른 플레이어의 복제된 이름 표시
	if (UPFCharacterWidget* CharacterWidget = Cast<UPFCharacterWidget>(OtherHPBar->GetUserWidgetObject()))
	{
		const APFPlayerState* PFPlayerState = IsPlayerCharacter() ? GetPlayerState<APFPlayerState>() : nullptr;
		CharacterWidget->SetDisplayUsername(PFPlayerState ? PFPlayerState->GetPlayerName() : FString());
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	ULocalPlayer* LocalPlayer = OtherHPBar->GetOwnerPlayer();
	if (!LocalPlayer && PlayerController) LocalPlayer = PlayerController->GetLocalPlayer();
	if (LocalPlayer && LocalPlayer->ViewportClient)
	{
		FVector2D ViewportSize;
		LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);
		if (ViewportSize.X > 0.f && ViewportSize.Y > 0.f)
		{
			OtherHPBar->SetRelativeScale3D(FVector(1.f, ViewportSize.X / 1920.f * .25f, ViewportSize.Y / 1080.f * .25f));
		}
	}

	// 카메라 방향으로 회전
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		const FVector CameraBackward = -PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
		OtherHPBar->SetWorldRotation(CameraBackward.Rotation());
	}
}

// AI 공격 실행
void APFCharacter::Attack()
{
	if (!HasAuthority() || !ASC || !AttackAbilityClass)
	{
		return;
	}

	// 기본 공격 Spec 조회
	FGameplayAbilitySpec* AttackSpec = ASC->FindAbilitySpecFromClass(AttackAbilityClass);
	if (!AttackSpec)
	{
		return;
	}
	// 공격 중 추가 입력 전달
	if (AttackSpec->IsActive())
	{
		ASC->AbilitySpecInputPressed(*AttackSpec);
		ASC->AbilitySpecInputReleased(*AttackSpec);
		return;
	}

	// 기본 공격 활성화
	ASC->TryActivateAbility(AttackSpec->Handle);
}

// 기본 공격 클래스 반환
TSubclassOf<UGameplayAbility> APFCharacter::GetAttackAbilityClass() const
{
	return AttackAbilityClass;
}

// 공격 의도, 상태 조회
bool APFCharacter::IsAttackCommandActive() const
{
	if (IsEnemyCharacter())
	{
		return HasAuthority() ? IsAttacking
			: HasStateTag(PFGameplayTags::Character_State_Attacking);
	}
	if (!IsPlayerCharacter() || !ASC)
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

// 사망 상태, 제어 종료
void APFCharacter::Dead()
{
	if (!HasAuthority() || !IsValid(AttributeSet) || AttributeSet->GetHealth() > 0.f || IsDeadCharacter())
	{
		return;
	}
	SetReplicatedStateTag(PFGameplayTags::Character_State_Dead, true);
	ClearControlCommands();
	OnCharacterDied.Broadcast(this);
}

// 등장 시 무적 활성화
void APFCharacter::OnLevelStartMontageStarted(UAnimMontage* Montage)
{
	if (!HasAuthority() || !Montage)
	{
		return;
	}

	UPFAnimInstance* CharacterAnim = Cast<UPFAnimInstance>(GetMesh()->GetAnimInstance());
	if (!CharacterAnim || !CharacterAnim->IsLevelStartMontage(Montage) || !CharacterAnim->Montage_IsPlaying(Montage))
	{
		return;
	}

	// 서버 무적 태그 설정
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Invulnerable, 1);
	}
}

// 복제된 등장 연출 상태 반영
void APFCharacter::OnRep_LevelStartActive()
{
	UpdateFirstPersonPresentation();
	if (IsPlayerCharacter())
	{
		GetCharacterMovement()->bUseControllerDesiredRotation = !bLevelStartActive;
	}


	UAnimMontage* ActiveMontage = PFAnim->GetCurrentActiveMontage();
	const bool bPlayingLevelStart = ActiveMontage && PFAnim->IsLevelStartMontage(ActiveMontage)
		&& PFAnim->Montage_IsPlaying(ActiveMontage);

	if (bLevelStartActive)
	{
		if (!bPlayingLevelStart)
		{
			PFAnim->PlayMontage(PFAnim->LEVELSTART_GLOBAL);
		}
		SetBlockTags(true);
		return;
	}

	if (bPlayingLevelStart)
	{
		PFAnim->Montage_Stop(0.f, ActiveMontage);
	}
	SetBlockTags(false);
}

// 등장 종료 처리
void APFCharacter::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	if (PFAnim->IsLevelStartMontage(Montage))
	{
		// 등장 무적 해제
		if (HasAuthority() && !PFAnim->Montage_IsPlaying(Montage))
		{
			bLevelStartActive = false;
			UpdateFirstPersonPresentation();
			if (IsPlayerCharacter())
			{
				GetCharacterMovement()->bUseControllerDesiredRotation = true;
			}
			ForceNetUpdate();

			if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
			{
				AbilitySystem->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Invulnerable, 0);
			}
		}

		SetBlockTags(false);
	}
}

void APFCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 이동 방향 복제 등록
	DOREPLIFETIME_CONDITION(APFCharacter, FinalDir, COND_SkipOwner);
	DOREPLIFETIME(APFCharacter, CharacterRole);
	DOREPLIFETIME(APFCharacter, ReplicatedAimPitch);
	DOREPLIFETIME(APFCharacter, CurrentControlMode);
	DOREPLIFETIME(APFCharacter, ViewpointFixed);
	DOREPLIFETIME(APFCharacter, bLevelStartActive);
	DOREPLIFETIME(APFCharacter, bJumpPadFlightActive);
}

// 점프 어빌리티 활성화
void APFCharacter::ActivateJumpAbility()
{
	if (!IsLocalPlayerCharacter() || IsDeadCharacter() || bJumpPadFlightActive || !ASC || !JumpAbilityClass)
	{
		PFLOG(Warning, TEXT("JumpAbility Failed"));
		return;
	}

	ASC->TryActivateAbilityByClass(JumpAbilityClass);
}

// 체력 회복
bool APFCharacter::RestoreHealth(float Value)
{
	return HasAuthority() && FPFGE_StatGameplayEffects::ApplyHeal(ASC, Value);
}

// 마나 회복
bool APFCharacter::RestoreMana(float Value)
{
	return HasAuthority() && FPFGE_StatGameplayEffects::ApplyManaRestore(ASC, Value);
}

// 실드 적용
bool APFCharacter::GrantShield()
{
	return HasAuthority() && FPFGE_StatGameplayEffects::ApplyShield(ASC);
}

// 코인 획득
bool APFCharacter::AddCoin(float Value)
{
	return HasAuthority() && FPFGE_StatGameplayEffects::ApplyCoin(ASC, Value);
}


// 마나 조회
float APFCharacter::GetMana() const
{
	return AttributeSet ? AttributeSet->GetMana() : 0.f;
}

// 마나 소모 시도
bool APFCharacter::TryUseMana(float ManaCost)
{
	return HasAuthority() && FPFGE_StatGameplayEffects::TryApplyManaCost(ASC, AttributeSet, ManaCost);
}

// 질주, 공격 상태에 맞춰 속도 조정
void APFCharacter::ManageSpeed()
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

// 메시 부착 이펙트 생성
UNiagaraComponent* APFCharacter::SpawnAttachedNiagara(UNiagaraSystem* NiagaraSystem)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(NiagaraSystem))
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


// 체력 회복 이펙트 재생
void APFCharacter::GameplayCue_Item_Use_HP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		SpawnAttachedNiagara(HPPotionUseNiagara);
	}
}

// 마나 회복 이펙트 재생
void APFCharacter::GameplayCue_Item_Use_MP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		SpawnAttachedNiagara(MPPotionUseNiagara);
	}
}

// 실드 이펙트 시작, 종료
void APFCharacter::GameplayCue_Item_Use_Shield(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
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
void APFCharacter::GameplayCue_Item_Use_Coin(EGameplayCueEvent::Type EventType, const FGameplayCueParameters&)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		SpawnAttachedNiagara(CoinUseNiagara);
	}
}

void APFCharacter::Jump()
{
	if (IsDeadCharacter())
	{
		return;
	}

	Super::Jump();
}

// 질주 허용 여부 조회
bool APFCharacter::CanSprint() const
{
	return true;
}

// 질주 상태 조회
bool APFCharacter::IsSprinting() const
{
	return HasStateTag(PFGameplayTags::Character_State_Sprinting);
}

// 공격 Spec에 누름 입력 전달
void APFCharacter::PressAttackAbilityInput()
{
	if (!IsLocalPlayerCharacter() || !ASC)
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

	// 공격 활성화, 소유 클라이언트 입력 전송
	if (!AttackSpec->IsActive())
	{
		ASC->TryActivateAbility(AttackSpec->Handle);
	}
	if (!HasAuthority())
	{
		ASC->ServerSetInputPressed(AttackSpec->Handle);
	}
}

// 공격 Spec에 해제 입력 전달
void APFCharacter::ReleaseAttackAbilityInput()
{
	if (!IsLocalPlayerCharacter() || !ASC)
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

// 시점 고정에 따른 회전 방식 반영
void APFCharacter::OnRep_ViewpointFixed()
{
	if (!IsPlayerCharacter())
	{
		return;
	}
	GetCharacterMovement()->bOrientRotationToMovement = !ViewpointFixed;
}

// 플레이어 점프, 궁극기 부여
void APFCharacter::GivePlayerAbilities()
{
	if (!HasAuthority() || !IsPlayerCharacter())
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

// 제어 모드의 카메라, 몸 회전 반영
void APFCharacter::OnRep_CurrentControlMode()
{
	if (!IsPlayerCharacter())
	{
		return;
	}
	SpringArm->ProbeChannel = ECC_Visibility;
	SpringArm->SetRelativeLocation(SpringArm->GetRelativeLocation() + SpringArm->SocketOffset);
	SpringArm->SocketOffset = FVector::ZeroVector;

	// 공통 카메라, 몸 회전 설정
	if (CurrentControlMode == TOPVIEW || CurrentControlMode == TPS || CurrentControlMode == FPS)
	{
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritRoll = true;
		SpringArm->bInheritYaw = true;
		SpringArm->bDoCollisionTest = false;
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = !bLevelStartActive;
		GetCharacterMovement()->RotationRate = FRotator(0.f, -1.f, 0.f);
	}

	// 모드별 카메라, 회전 설정
	switch (CurrentControlMode)
	{
	case TOPVIEW:
		ArmLengthTo = 800.f;
		ArmRotationTo = FRotator(-45.f, 0.f, 0.f);
		SpringArm->bInheritPitch = false;
		PFAnim->SetFPS(false);
		break;
	case TPS:
		ArmLengthTo = 200.f;
		// 몸 회전과 분리된 어깨 너머 시점
		SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
		SpringArm->SocketOffset = FVector(0.f, 50.f, 0.f);
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		SpringArm->bInheritPitch = true;
		PFAnim->SetFPS(false);
		break;
	case FPS:
		ArmLengthTo = 0.f;
		SpringArm->SetRelativeLocation(FVector(20.f, 0.f, 80.f));
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		SpringArm->bInheritPitch = true;
		PFAnim->SetFPS(IsLocallyControlled());
		break;
	default:
		break;
	}
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetController()))
	{
		PlayerController->BindCharacterHUD();
	}
	UpdateFirstPersonPresentation();
}

// 시점에 맞는 연출 메시 선택
USkeletalMeshComponent* APFCharacter::GetPresentationMesh() const
{
	return bFirstPersonPresentationActive ? FirstPersonMesh.Get() : GetMesh();
}

// 공통 카메라에서 전체 캐릭터 표시
void APFCharacter::SetCampaignCameraActive(bool bActive)
{
	if (bCampaignCameraActive == bActive) return;
	bCampaignCameraActive = bActive;
	UpdateFirstPersonPresentation();
}

// 소유 플레이어의 1인칭 표시 전환
void APFCharacter::UpdateFirstPersonPresentation()
{
	const bool bEnable = !bCampaignCameraActive && IsLocalPlayerCharacter() && CurrentControlMode == FPS && FirstPersonMeshAsset
		&& GetNetMode() != NM_DedicatedServer
		&& !FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title"));
	if (bEnable && !FirstPersonMesh)
	{
		FirstPersonMesh = NewObject<UPFFirstPersonMeshComponent>(this, TEXT("FirstPersonArms"));
		FirstPersonMesh->SetupAttachment(GetRootComponent());
		FirstPersonMesh->SetSkeletalMesh(FirstPersonMeshAsset);
		FirstPersonMesh->SetVisibility(false);
		FirstPersonMesh->RegisterComponent();
		FirstPersonMesh->InitializeView(GetMesh(), Camera, FirstPersonNeckOffset);
	}
	if (bEnable != bFirstPersonPresentationActive)
	{
		bFirstPersonPresentationActive = bEnable;
		if (bEnable)
		{
			bSavedBodyOwnerNoSee = GetMesh()->bOwnerNoSee;
			SavedBodyForcedLOD = GetMesh()->GetForcedLOD();
			SavedBodyAnimTickOption = GetMesh()->VisibilityBasedAnimTickOption;
			SavedBodyPrimitiveType = GetMesh()->FirstPersonPrimitiveType;
			GetMesh()->SetOwnerNoSee(true);
			GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
			GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			GetMesh()->SetForcedLOD(1);
			Camera->SetFirstPersonFieldOfView(FirstPersonFieldOfView);
			Camera->SetFirstPersonScale(0.3f);
		}
		else
		{
			GetMesh()->SetOwnerNoSee(bSavedBodyOwnerNoSee);
			GetMesh()->SetFirstPersonPrimitiveType(SavedBodyPrimitiveType);
			GetMesh()->VisibilityBasedAnimTickOption = SavedBodyAnimTickOption;
			GetMesh()->SetForcedLOD(SavedBodyForcedLOD);
		}
		Camera->SetEnableFirstPersonFieldOfView(bEnable);
		Camera->SetEnableFirstPersonScale(bEnable);
		FirstPersonMesh->SetComponentTickEnabled(bEnable);
	}
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(bEnable && !IsDeadCharacter() && !bLevelStartActive);
	}
}

// 조종 역할, GAS 연결 갱신
void APFCharacter::RefreshControlRole()
{
	ShrubStealth->RefreshState();
	UpdateFirstPersonPresentation();
	if (CharacterRole == EPFCharacterRole::ROLE_END)
	{
		return;
	}
	if (IsPlayerCharacter())
	{
		OnRep_CurrentControlMode();
		OnRep_ViewpointFixed();
		if (GetWorld() && FPackageName::GetShortName(GetWorld()->GetMapName()).Contains(TEXT("Title")))
		{
			GetMesh()->SetVisibility(false);
		}
	}
	else
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;
		bUseControllerRotationYaw = false;
	}
	InitAbilityActorInfo();
	SetHPBar();
}

void APFCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	CharacterRole = NewController && NewController->IsPlayerController()
		? EPFCharacterRole::PLAYER : EPFCharacterRole::ENEMY;
	RefreshControlRole();
	ForceNetUpdate();
}

void APFCharacter::UnPossessed()
{
	EndJumpPadFlight();
	ClearControlCommands();
	Super::UnPossessed();
	UpdateFirstPersonPresentation();
}

bool APFCharacter::CanJumpInternal_Implementation() const
{
	return !bJumpPadFlightActive && Super::CanJumpInternal_Implementation();
}

// 서버 점프대 비행 시작
bool APFCharacter::BeginJumpPadFlight(const FVector& LaunchVelocity, float Duration)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!GetController()) return false;
	bJumpPadFlightActive = true;
	SetReplicatedStateTag(PFGameplayTags::Character_State_Sprinting, false);
	OnRep_JumpPadFlightActive();
	Movement->StopMovementImmediately();
	const FVector StartLocation = GetActorLocation();
	LaunchCharacter(LaunchVelocity, true, true);
	GetWorldTimerManager().SetTimer(JumpPadFlightTimer,
		FTimerDelegate::CreateUObject(this, &APFCharacter::EndJumpPadFlight, false), Duration + 1.f, false);
	ForceNetUpdate();
	Client_BeginJumpPadFlight(LaunchVelocity, StartLocation, GetWorld()->GetTimeSeconds(), Duration);
	return true;
}

// 비행 중 이동 설정, 보관된 입력 정리
void APFCharacter::OnRep_JumpPadFlightActive()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bJumpPadFlightActive)
	{
		if (!bJumpPadMovementSaved)
		{
			SavedJumpPadAirControl = Movement->AirControl;
			SavedJumpPadFallingFriction = Movement->FallingLateralFriction;
			SavedJumpPadFallingBraking = Movement->BrakingDecelerationFalling;
			bJumpPadMovementSaved = true;
			Movement->AirControl = 0.f;
			Movement->FallingLateralFriction = 0.f;
			Movement->BrakingDecelerationFalling = 0.f;
		}
		ClearControlCommands();
		ConsumeMovementInputVector();
	}
	else if (bJumpPadMovementSaved)
	{
		Movement->AirControl = SavedJumpPadAirControl;
		Movement->FallingLateralFriction = SavedJumpPadFallingFriction;
		Movement->BrakingDecelerationFalling = SavedJumpPadFallingBraking;
		bJumpPadMovementSaved = false;
	}
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(GetController()))
	{
		PlayerController->ResetGameplayInput();
	}
}

// 소유 클라이언트의 발사 위치, 속도 동기화
void APFCharacter::Client_BeginJumpPadFlight_Implementation(FVector LaunchVelocity, FVector StartLocation,
	float ServerStartTime, float Duration)
{
	if (HasAuthority() || !IsLocallyControlled() || IsDeadCharacter()) return;
	bJumpPadFlightActive = true;
	OnRep_JumpPadFlightActive();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Elapsed = GameState
		? FMath::Clamp(static_cast<float>(GameState->GetServerWorldTimeSeconds()) - ServerStartTime, 0.f, Duration) : 0.f;
	const float GravityZ = GetCharacterMovement()->GetGravityZ();
	SetActorLocation(StartLocation + LaunchVelocity * Elapsed + FVector(0.f, 0.f, 0.5f * GravityZ * Elapsed * Elapsed),
		false, nullptr, ETeleportType::TeleportPhysics);
	LaunchVelocity.Z += GravityZ * Elapsed;
	LaunchCharacter(LaunchVelocity, true, true);
}

// 착지, 중단 시 비행 잠금 해제
void APFCharacter::EndJumpPadFlight(bool bStopMovement)
{
	if (!bJumpPadFlightActive && !bJumpPadMovementSaved) return;
	const bool bWasActive = bJumpPadFlightActive;
	bJumpPadFlightActive = false;
	if (GetWorld()) GetWorldTimerManager().ClearTimer(JumpPadFlightTimer);
	if (bStopMovement) GetCharacterMovement()->StopMovementImmediately();
	OnRep_JumpPadFlightActive();
	if (HasAuthority() && bWasActive && !IsActorBeingDestroyed())
	{
		Client_EndJumpPadFlight(bStopMovement);
		ForceNetUpdate();
	}
}

// 소유 클라이언트의 비행 종료
void APFCharacter::Client_EndJumpPadFlight_Implementation(bool bStopMovement)
{
	if (!HasAuthority()) EndJumpPadFlight(bStopMovement);
}

void APFCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshControlRole();
}

void APFCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	RefreshControlRole();
}

// 복제된 역할 반영
void APFCharacter::OnRep_CharacterRole()
{
	RefreshControlRole();
}

void APFCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (bJumpPadFlightActive && PrevMovementMode == MOVE_Falling && GetCharacterMovement()->MovementMode != MOVE_Falling)
	{
		EndJumpPadFlight(GetCharacterMovement()->IsMovingOnGround());
	}
	if (PrevMovementMode == MOVE_Falling && GetCharacterMovement()->IsMovingOnGround())
	{
		OnCharacterLanded.Broadcast(this);
	}
}

void APFCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	if (HasAuthority() && IsEnemyCharacter())
	{
		ReplicatedAimPitch = GetAimPitch();
	}
}

// 컨트롤러의 발사 조준점 조회
bool APFCharacter::TryGetAttackAim(FVector& OutAimPoint)
{
	IPFCombatAimProvider* Provider = Cast<IPFCombatAimProvider>(GetController());
	return Provider && Provider->TryGetCombatAim(OutAimPoint);
}

// 플레이어 공격 입력 전달
void APFCharacter::SetAttackInputPressed(bool bPressed)
{
	if (!IsPlayerCharacter())
	{
		return;
	}
	if (bPressed && !IsDeadCharacter() && !bJumpPadFlightActive)
	{
		PressAttackAbilityInput();
	}
	else
	{
		ReleaseAttackAbilityInput();
	}
}

// AI 공격 의도, 실행 요청
void APFCharacter::SetAIAttackCommand(bool bRequested, bool bExecute)
{
	if (!HasAuthority() || !IsEnemyCharacter())
	{
		return;
	}
	IsAttacking = bRequested && !IsDeadCharacter();
	if (IsAttacking && bExecute)
	{
		Attack();
	}
}

// 이전 조종 입력, 아바타 어빌리티 정리
void APFCharacter::ClearControlCommands()
{
	IsAttacking = false;
	MovementInputDirection = IDLE;
	FinalDir = IDLE;
	StopJumping();
	OnRep_FinalDir();
	if (!ASC || ASC->GetAvatarActor() != this)
	{
		PressedAttackAbilityHandle = FGameplayAbilitySpecHandle();
		return;
	}
	ReleaseAttackAbilityInput();
	if (HasAuthority())
	{
		FGameplayTagContainer AvatarAbilityTags;
		AvatarAbilityTags.AddTag(PFGameplayTags::Character_Ability_Attack);
		AvatarAbilityTags.AddTag(PFGameplayTags::Character_Ability_Jump);
		AvatarAbilityTags.AddTag(PFGameplayTags::Character_Ability_Ultimate);
		TArray<FGameplayAbilitySpecHandle> ActiveHandles;
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.Ability || !Spec.Ability->GetAssetTags().HasAny(AvatarAbilityTags))
			{
				continue;
			}
			Spec.InputPressed = false;
			if (Spec.IsActive())
			{
				ActiveHandles.Add(Spec.Handle);
			}
		}
		for (const FGameplayAbilitySpecHandle Handle : ActiveHandles)
		{
			ASC->CancelAbilityHandle(Handle);
		}
	}
}

// 사망 애니메이션 완료 전달
void APFCharacter::HandleDeathAnimationEnd()
{
	OnDeathAnimationEnded.Broadcast(this);
}

// 이동 입력 방향 반영
void APFCharacter::SetMovementInputDirection(EPFDirection NewDirection)
{
	if (!IsPlayerCharacter() || NewDirection >= EPFDirection::EDIRECTION_END)
	{
		return;
	}
	MovementInputDirection = NewDirection;
	SetDir();
}

// 이동 상태를 애니메이션 방향으로 변환
void APFCharacter::SetDir()
{
	FinalDir = IsDeadCharacter() || IsMovementBlocked() ? IDLE : MovementInputDirection;
	OnRep_FinalDir();
}

// AI 이동 방향 반영
void APFCharacter::SetAIMovementDirection(EPFDirection NewDirection)
{
	if (HasAuthority() && IsEnemyCharacter() && NewDirection < EPFDirection::EDIRECTION_END)
	{
		FinalDir = NewDirection;
		OnRep_FinalDir();
	}
}

// 서버 제어 모드 반영
void APFCharacter::SetControlMode(ECONTROLMODE NewControlMode)
{
	if (!HasAuthority() || !IsPlayerCharacter() || bJumpPadFlightActive
		|| (NewControlMode != TOPVIEW && NewControlMode != TPS && NewControlMode != FPS))
	{
		return;
	}
	CurrentControlMode = NewControlMode;
	if (NewControlMode != TOPVIEW && !ViewpointFixed)
	{
		if (IsSprinting() && !IsMovementBlocked() && CanSprint())
		{
			SetReplicatedStateTag(PFGameplayTags::Character_State_Sprinting, false);
		}
		ViewpointFixed = true;
	}
	OnRep_CurrentControlMode();
	OnRep_ViewpointFixed();
}

// 서버 시점 고정 전환
void APFCharacter::ToggleViewpointFixed()
{
	if (!HasAuthority() || !IsPlayerCharacter() || IsDeadCharacter() || bJumpPadFlightActive
		|| ((CurrentControlMode == TPS || CurrentControlMode == FPS) && ViewpointFixed))
	{
		return;
	}
	if (!ViewpointFixed && IsSprinting())
	{
		SetReplicatedStateTag(PFGameplayTags::Character_State_Sprinting, false);
	}
	ViewpointFixed = !ViewpointFixed;
	OnRep_ViewpointFixed();
}

// 서버 질주 전환
void APFCharacter::ToggleSprint()
{
	if (!HasAuthority() || !IsPlayerCharacter() || IsDeadCharacter() || IsMovementBlocked() || !CanSprint()
		|| (CurrentControlMode == FPS && !IsSprinting()))
	{
		return;
	}
	SetReplicatedStateTag(PFGameplayTags::Character_State_Sprinting, !IsSprinting());
}

// 플레이어 궁극기 실행
void APFCharacter::ActivateUltimateAbility()
{
	if (IsPlayerCharacter() && !IsDeadCharacter() && !bJumpPadFlightActive && ASC && UltimateAbilityClass)
	{
		ASC->TryActivateAbilityByClass(UltimateAbilityClass);
	}
}

// 카메라 상태 저장
FPFCharacterSharedStateSnapshot APFCharacter::CaptureViewState() const
{
	FPFCharacterSharedStateSnapshot Snapshot;
	Snapshot.ControlMode = CurrentControlMode;
	Snapshot.bViewpointFixed = ViewpointFixed;
	Snapshot.FinalDirection = FinalDir;

	// 카메라 상태 저장
	Snapshot.SpringArmLength = SpringArm->TargetArmLength;
	Snapshot.SpringArmRelativeLocation = SpringArm->GetRelativeLocation();
	Snapshot.SpringArmRelativeRotation = SpringArm->GetRelativeRotation();

	Snapshot.DesiredSpringArmLength = ArmLengthTo;
	Snapshot.DesiredSpringArmRotation = ArmRotationTo;

	Snapshot.CameraFieldOfView = Camera->FieldOfView;

	return Snapshot;
}

// 교체된 캐릭터의 카메라 상태 반영
void APFCharacter::ApplyViewState(const FPFCharacterSharedStateSnapshot& Snapshot)
{
	if (!IsPlayerCharacter())
	{
		return;
	}

	CurrentControlMode = Snapshot.ControlMode;
	FinalDir = Snapshot.FinalDirection;

	// 이동 입력, 속도 초기화
	MovementInputDirection = IDLE;
	GetCharacterMovement()->StopMovementImmediately();

	// 제어 설정 반영
	ViewpointFixed = true;
	OnRep_CurrentControlMode();
	ViewpointFixed = Snapshot.bViewpointFixed;
	OnRep_ViewpointFixed();
	OnRep_FinalDir();


	// 카메라 상태 복원
	ArmLengthTo = Snapshot.DesiredSpringArmLength;
	ArmRotationTo = Snapshot.DesiredSpringArmRotation;

	SpringArm->TargetArmLength = Snapshot.SpringArmLength;
	SpringArm->SetRelativeLocation(Snapshot.SpringArmRelativeLocation);
	SpringArm->SetRelativeRotation(Snapshot.SpringArmRelativeRotation);

	Camera->SetFieldOfView(Snapshot.CameraFieldOfView);
}
