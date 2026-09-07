#include "Character/PFCharacter.h"

#include "UI/HUD/PFCharacterWidget.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFGameMode.h"
#include "Engine/World.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

APFCharacter::APFCharacter()
	: IsAttacking(false)
	, PFAnim(nullptr)
	, FinalDir(IDLE)
{
	PrimaryActorTick.bCanEverTick = true;

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
	GetMesh()->SetIsReplicated(true);

}

UAbilitySystemComponent* APFCharacter::GetAbilitySystemComponent() const
{
	// 플레이어는 PlayerState ASC 사용
	if (const APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>())
	{
		return PFPlayerState->GetAbilitySystemComponent();
	}

	return ASC;
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

void APFCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 이벤트 구독 해제
	UnbindStateTagEvents();
	if (AttributeSet)
	{
		AttributeSet->OnHealthIsZero.RemoveAll(this);
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
	if (!ASC || BoundStateASC.Get() == ASC)
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
	SetActorEnableCollision(NewCount == 0);
	if (NewCount > 0 && PFAnim)
	{
		PFAnim->Dead();
	}
}

// 궁극기 상태 처리 (파생 클래스 구현)
void APFCharacter::UltimateTagChanged(FGameplayTag StateTag, int32 NewCount)
{
}

void APFCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetHPBar();
}

// ASC 연결 및 전투 초기화
void APFCharacter::InitAbilityActorInfo()
{
	if (APFPlayerState* PFPlayerState = GetPlayerState<APFPlayerState>())
	{
		// PlayerState의 ASC, 스탯 참조
		ASC = PFPlayerState->GetAbilitySystemComponent();
		AttributeSet = PFPlayerState->GetAttributeSet();
		if (!ASC || !AttributeSet)
		{
			return;
		}

		// 플레이어 ASC 연결 및 스탯 초기화
		ASC->InitAbilityActorInfo(PFPlayerState, this);
		PFPlayerState->InitializeGASStats();
	}
	else if (ASC && AttributeSet)
	{
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
		const UAnimMontage* ActiveMontage = PFAnim ? PFAnim->GetCurrentActiveMontage() : nullptr;
		const bool bPlayingLevelStart = ActiveMontage && PFAnim->IsLevelStartMontage(ActiveMontage)
			&& PFAnim->Montage_IsPlaying(ActiveMontage);
		ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Invulnerable, bPlayingLevelStart ? 1 : 0);
	}

	// 전투 상태, 이벤트, UI 준비
	GiveAttackAbility();
	SetBlockTags(true);
	BindAttributeDelegates();
	SetHPBar();
	BindStateTagEvents();
}

// 기본 공격 어빌리티 부여
void APFCharacter::GiveAttackAbility()
{
	if (!HasAuthority() || !ASC || !AttackAbilityClass)
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
	if (!HasAuthority() || !ASC || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

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
	if (!HasAuthority() || !ASC || !AttributeSet)
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
		bOwnedGASStatsInitialized = FPFGE_StatGameplayEffects::InitializeStats(
			ASC,
			static_cast<float>(InitialData->Level),
			static_cast<float>(InitialData->CurExp),
			InitialData->MaxHP,
			InitialData->MaxHP,
			InitialData->MaxMP,
			InitialData->MaxMP,
			InitialData->Damage,
			0.f,
			10.f);
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
	if (!AttributeSet || bAttributeDelegatesBound)
	{
		return;
	}

	AttributeSet->OnHealthIsZero.AddUObject(this, &APFCharacter::Dead);
	bAttributeDelegatesBound = true;
}

// Loose 태그 카운트 증가
void APFCharacter::AddTag(FName TagName, int Value)
{
	if (ASC)
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TagName), Value);
	}
}

// Loose 태그 활성화, 해제
void APFCharacter::SetTag(FName TagName, bool Value)
{
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TagName), Value);
	}
}

// 행동 차단 태그 설정
void APFCharacter::SetBlockTags(bool bBlocked)
{
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(PFGameplayTags::Character_Block_Attack, bBlocked);
	}
	SetTag(FName("Character.Block.Jump"), bBlocked);
	SetTag(FName("Character.Block.Move"), bBlocked);
	SetTag(FName("Character.Block.Ultimate"), bBlocked);
}

// 이동 차단 여부 조회
bool APFCharacter::IsMovementBlocked() const
{
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	return AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(
			FGameplayTag::RequestGameplayTag(FName("Character.Block.Move")));
}

// 공중 상태 태그 조회
bool APFCharacter::HasAirborneTag() const
{
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	return AbilitySystemComponent
		&& (AbilitySystemComponent->HasMatchingGameplayTag(
			FGameplayTag::RequestGameplayTag(FName("Character.State.Jumping")))
			|| AbilitySystemComponent->HasMatchingGameplayTag(
				FGameplayTag::RequestGameplayTag(FName("Character.State.Falling"))));
}

// 점프, 낙하 태그 갱신
void APFCharacter::UpdateAirborneTag()
{
	// 상승, 하강 구분
	const UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	const bool bIsAirborne = CharacterMovementComponent && CharacterMovementComponent->IsFalling();
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

	// 캐릭터 충돌 설정
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("PFCharacter"));
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
}

// 체력바 연결 및 표시 설정
void APFCharacter::SetHPBar()
{
	if (!AttributeSet || !OtherHPBar)
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
	if (!World)
	{
		return;
	}

	// 타이틀, 로컬 캐릭터 체력바 숨김
	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")) || IsLocallyControlled())
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

	// 공중 상태, 체력바 갱신
	UpdateAirborneTag();

	if (!IsLocallyControlled())
	{
		TickHPBar();
	}
}

float APFCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
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
			PostHitProcessing();
		}
	}
	return FinalDamage;
}

// GAS 피해 적용
bool APFCharacter::ApplyAttackDamageTo(APFCharacter* Target, float Damage, AActor* Attacker,
	const UGameplayAbility* AttackAbility, const FHitResult* HitResult)
{
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
		Target->PostHitProcessing();
	}
	return bApplied;
}

// 피격 후처리 (파생 클래스 구현)
void APFCharacter::PostHitProcessing()
{
}

// 조준 피치 반환
float APFCharacter::GetAimPitch() const
{
	return GetBaseAimRotation().GetNormalized().Pitch;
}

// 공격력 반환
float APFCharacter::GetDamage() const
{
	return AttributeSet ? AttributeSet->GetAttackPower() : 0.f;
}

// 이동 방향을 애니메이션에 반영
void APFCharacter::OnRep_FinalDir()
{
	if (PFAnim)
	{
		PFAnim->SetCurrentDir(FinalDir);
	}
}

// 체력바 크기, 방향 갱신
void APFCharacter::TickHPBar()
{
	if (!OtherHPBar || IsLocallyControlled())
	{
		return;
	}

	// 화면 비율에 맞춰 크기 보정
	OtherHPBar->SetRelativeScale3D(FVector(1.f, SCREENRATIO.X * 0.25f, SCREENRATIO.Y * 0.25f));

	// 카메라 방향으로 회전
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
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
	return HasAuthority()
		? IsAttacking
		: ASC && ASC->HasMatchingGameplayTag(PFGameplayTags::Character_State_Attacking);
}

// 사망 처리 및 리스폰 예약
void APFCharacter::Dead()
{
	if (!HasAuthority() || !IsValid(AttributeSet) || AttributeSet->GetHealth() > 0.f || IsDeadCharacter())
	{
		return;
	}

	SetReplicatedStateTag(PFGameplayTags::Character_State_Dead, true);

	// 플레이어 리스폰 예약
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (APFGameMode* PFGameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
		{
			GetWorldTimerManager().SetTimer(PlayerRespawnTimerHandle,
				FTimerDelegate::CreateUObject(PFGameMode, &APFGameMode::RespawnPlayer,
					TWeakObjectPtr<APlayerController>(PlayerController)), 10.f, false);
		}
	}
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

// 등장 종료 처리
void APFCharacter::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	if (PFAnim->IsLevelStartMontage(Montage))
	{
		// 등장 무적 해제
		if (HasAuthority() && !PFAnim->Montage_IsPlaying(Montage))
		{
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
	DOREPLIFETIME(APFCharacter, FinalDir);
}
