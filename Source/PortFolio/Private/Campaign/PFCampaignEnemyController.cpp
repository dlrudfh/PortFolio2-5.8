#include "Campaign/PFCampaignEnemyController.h"
#include "Campaign/PFCampaignDirector.h"
#include "Campaign/PFGA_CampaignShockwave.h"
#include "Character/PFCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Campaign_Returning, "Character.State.Invulnerable.Returning");
}

APFCampaignEnemyController::APFCampaignEnemyController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TargetSearchRadius = 5000.f;
}

// 전투 배치와 지휘관 어빌리티 연결
void APFCampaignEnemyController::Configure(const FVector& Origin, const TArray<FVector>& Patrol,
	APFCampaignDirector* InDirector, bool bCommander, bool bBoss)
{
	GuardOrigin = Origin;
	if (const APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn()))
		GuardOrigin = CampaignPawn->GetCharacterMovement()->GetActorFeetLocation();
	PatrolPoints = Patrol;
	Director = InDirector;
	bConfigured = true;
	bFinalBoss = bCommander && bBoss;
	NextShockwave = GetWorld()->GetTimeSeconds() + 5.;
	if (APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn()); bFinalBoss && CampaignPawn)
		ShockwaveHandle = CampaignPawn->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UPFGA_CampaignShockwave::StaticClass(), 1));
}

// 피격 시 공격자를 추적 대상으로 인식
void APFCampaignEnemyController::NotifyDamageFrom(APFCharacter* Attacker)
{
	const APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn());
	if (!HasAuthority() || !CampaignPawn || CampaignPawn->IsDeadCharacter() || !IsPlayerTargetValid(Attacker)) return;
	RememberShrubAttacker(Attacker);
	if (TargetCharacter.Get() != Attacker)
	{
		TargetCharacter = Attacker;
		RequestNavigationRefresh();
	}
	bWasInCombat = true;
}

void APFCampaignEnemyController::OnUnPossess()
{
	FinishReturn();
	bWasInCombat = false;
	Super::OnUnPossess();
}

void APFCampaignEnemyController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FinishReturn();
	Super::EndPlay(EndPlayReason);
}

// 전투 중단, 체력 회복, 무적 복귀 시작
void APFCampaignEnemyController::BeginReturn()
{
	APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn());
	if (bReturning || !HasAuthority() || !CampaignPawn || CampaignPawn->IsDeadCharacter()) return;
	UAbilitySystemComponent* ASC = CampaignPawn->GetAbilitySystemComponent();
	const UPFAttributeSet* Stats = CampaignPawn->GetAttributeSet();
	if (!ASC || !Stats) return;

	bReturning = true;
	bWasInCombat = false;
	TargetCharacter.Reset();
	RevealedAttackers.Reset();
	CampaignPawn->ClearControlCommands();
	if (ShockwaveHandle.IsValid()) ASC->CancelAbilityHandle(ShockwaveHandle);
	ReturnASC = ASC;
	ASC->AddLooseGameplayTag(TAG_Campaign_Returning, 1, EGameplayTagReplicationState::TagOnly);
	ASC->AddLooseGameplayTag(PFGameplayTags::Character_Block_Attack, 1, EGameplayTagReplicationState::TagOnly);
	FPFGE_StatGameplayEffects::ApplyHeal(ASC, Stats->GetMaxHealth() - Stats->GetHealth());
	SavedWalkSpeed = CampaignPawn->GetCharacterMovement()->MaxWalkSpeed;
	CampaignPawn->GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed * 2.f;
	ClearEnemyIntent();
	RequestNavigationRefresh();
}

// 복귀 무적, 이동 속도 복원
void APFCampaignEnemyController::FinishReturn()
{
	if (!bReturning) return;
	if (UAbilitySystemComponent* ASC = ReturnASC.Get())
	{
		ASC->RemoveLooseGameplayTag(TAG_Campaign_Returning, 1, EGameplayTagReplicationState::TagOnly);
		ASC->RemoveLooseGameplayTag(PFGameplayTags::Character_Block_Attack, 1, EGameplayTagReplicationState::TagOnly);
	}
	if (APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn()))
		CampaignPawn->GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed;
	ReturnASC.Reset();
	bReturning = false;
	NextShockwave = GetWorld()->GetTimeSeconds() + 5.;
}

// 지휘관 패턴 활성 상태
bool APFCampaignEnemyController::IsBossPatternActive() const
{
	const APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn());
	const UAbilitySystemComponent* ASC = CampaignPawn ? CampaignPawn->GetAbilitySystemComponent() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(ShockwaveHandle) : nullptr;
	return Spec && Spec->IsActive();
}

void APFCampaignEnemyController::Tick(float DeltaTime)
{
	APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn());
	if (!HasAuthority() || !CampaignPawn || CampaignPawn->IsDeadCharacter())
	{
		FinishReturn();
		Super::Tick(DeltaTime);
		return;
	}
	if (bConfigured && (bWasInCombat || TargetCharacter.IsValid()) && !CampaignPawn->IsLevelStartActive()
		&& FVector::DistSquared2D(CampaignPawn->GetActorLocation(), GuardOrigin) > FMath::Square(GuardRadius))
		BeginReturn();
	if (bConfigured && Director.IsValid()
		&& (Director->IsEncounterPaused() || IsBossPatternActive()))
	{
		CampaignPawn->ClearControlCommands();
		StopMovement();
		return;
	}
	Super::Tick(DeltaTime);
	if (TargetCharacter.IsValid()) bWasInCombat = true;
	if (!bConfigured || !bFinalBoss || CampaignPawn->IsDeadCharacter() || CampaignPawn->IsLevelStartActive()
		|| !TargetCharacter.IsValid() || !CampaignPawn->GetCharacterMovement()->IsMovingOnGround()) return;
	if (GetWorld()->GetTimeSeconds() >= NextShockwave
		&& FVector::DistSquared2D(CampaignPawn->GetActorLocation(), TargetCharacter->GetActorLocation()) <= FMath::Square(600.f))
	{
		UAbilitySystemComponent* ASC = CampaignPawn->GetAbilitySystemComponent();
		if (ASC->TryActivateAbility(ShockwaveHandle)) NextShockwave = GetWorld()->GetTimeSeconds() + 10.;
	}
}

bool APFCampaignEnemyController::IsPlayerTargetValid(const APFCharacter* Candidate) const
{
	return !bReturning && Super::IsPlayerTargetValid(Candidate) && (!bConfigured
		|| (GetPawn() && FVector::DistSquared2D(GetPawn()->GetActorLocation(), GuardOrigin) <= FMath::Square(GuardRadius)));
}

bool APFCampaignEnemyController::CanAcquireTarget(const APFCharacter* Candidate, const APFCharacter* Previous) const
{
	if (Candidate == Previous) return true;
	const APawn* ControlledPawn = GetPawn();
	return FVector::DotProduct(ControlledPawn->GetActorForwardVector(),
		(Candidate->GetActorLocation() - ControlledPawn->GetActorLocation()).GetSafeNormal2D()) >= 0.f && HasClearSightToTarget(Candidate);
}

bool APFCampaignEnemyController::ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const
{
	if (bFinalBoss && GetWorld()->GetTimeSeconds() >= NextShockwave
		&& FVector::DistSquared2D(GetPawn()->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(600.f)) return false;
	return Super::ShouldAttackTarget(Target, SurfaceDistance);
}

bool APFCampaignEnemyController::HandleIdleMovement(float DeltaTime)
{
	APFCharacter* CampaignPawn = Cast<APFCharacter>(GetPawn());
	if (!bConfigured || CampaignPawn->IsLevelStartActive()) return false;
	if (bWasInCombat && !bReturning) BeginReturn();
	CampaignPawn->SetAIAttackCommand(false, false);
	UCharacterMovementComponent* Movement = CampaignPawn->GetCharacterMovement();
	if (bReturning)
	{
		const FVector Feet = Movement->GetActorFeetLocation();
		if (Movement->IsMovingOnGround() && FVector::DistSquared2D(Feet, GuardOrigin) <= FMath::Square(100.f)
			&& FMath::Abs(Feet.Z - GuardOrigin.Z) <= Movement->MaxStepHeight)
		{
			ClearEnemyIntent();
			FinishReturn();
			return true;
		}
		UpdateIdleNavigation(GuardOrigin, DeltaTime);
		return true;
	}
	FVector Goal = PatrolPoints.IsValidIndex(PatrolIndex) ? PatrolPoints[PatrolIndex] : GuardOrigin;
	if (FVector::DistSquared2D(CampaignPawn->GetActorLocation(), Goal) < FMath::Square(100.f)
		&& FMath::Abs(Movement->GetActorFeetLocation().Z - Goal.Z) <= Movement->MaxStepHeight && !PatrolPoints.IsEmpty())
	{
		PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
		Goal = PatrolPoints[PatrolIndex];
	}
	UpdateIdleNavigation(Goal, DeltaTime);
	return true;
}
