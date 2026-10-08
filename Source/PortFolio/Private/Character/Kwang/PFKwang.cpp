

#include "Character/Kwang/PFKwang.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Campaign/PFCampaignEnemyController.h"
#include "Animation/PFAnimInst_Kwang.h"
#include "GAS/Abilities/Attack/PFGA_Attack_Kwang.h"
#include "GAS/PFGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

APFKwang::APFKwang() : SwordTrail(nullptr), bSwordHitDetectionActive(false)
{
	AttackAbilityClass = UPFGA_Attack_Kwang::StaticClass();
	AttackDamageMultiplier = 3.5f;
	IncomingDamageMultiplier = 0.5f;
	WalkSpeed = 500.f;
	SprintSpeed = 1000.f;
	AISettings.DesiredCombatDistance = 150.f;
	AISettings.DistanceTolerance = 0.f;
	AISettings.AttackRange = 200.f;

	SetMesh();
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ARMS(TEXT("/Game/GameData/Characters/FirstPerson/SK_Kwang_Arms.SK_Kwang_Arms"));
	FirstPersonMeshAsset = ARMS.Object;
	FirstPersonNeckOffset = FVector(-8.f, -8.f, -20.f);
	SetParticle();
	SetSound();
}

void APFKwang::PostInitializeComponents()
{
	PFAnim = Cast<UPFAnimInst_Kwang>(GetMesh()->GetAnimInstance());
	checkf(PFAnim, TEXT("Kwang AnimInstance is required"));
	Super::PostInitializeComponents();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	// 검 판정 이벤트 연결
	Cast<UPFAnimInst_Kwang>(PFAnim)->AttackStart.AddUObject(this, &APFKwang::AttackStart);
	Cast<UPFAnimInst_Kwang>(PFAnim)->AttackEnd.AddUObject(this, &APFKwang::AttackEnd);

	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
}

void APFKwang::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSwordTrailAttachment();

	if (HasAuthority() && bSwordHitDetectionActive && UpdateSwordCollision())
	{
		ProcessSwordHits();
	}
}

void APFKwang::SetMesh()
{
	UPFGameInstanceSubsystem::ApplyCharacterMesh(GetMesh(), CHARACTER_KWANG, false);
}

void APFKwang::SetParticle()
{
	static const TCHAR* const Paths[] =
	{
		TEXT("ParticleSystem'/Game/ParagonKwang/FX/Particles/Abilities/Primary/FX/P_Kwang_Primary_Trail.P_Kwang_Primary_Trail'")
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(PARTICLE_END));
	UPFGameInstanceSubsystem::LoadAssets(Particles, Paths);
}

void APFKwang::SetSound()
{
	static const TCHAR* const Paths[] =
	{
		TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Whoosh_1-1.Whoosh_1-1'"),
		TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Hit_Generic_2-1.Hit_Generic_2-1'")
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(SOUND_END));
	UPFGameInstanceSubsystem::LoadAssets(Sounds, Paths);
}

// 공격 Cue의 콤보 몽타주 재생
void APFKwang::GameplayCue_Character_Attack_Kwang_Normal_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed)
	{
		return;
	}

	const int32 MontageIndex = FMath::RoundToInt(Parameters.RawMagnitude);
	if (MontageIndex < etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA) || MontageIndex > etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKD))
	{
		return;
	}

	PFAnim->PlayMontage(MontageIndex);
}

// 검 판정, 공격 연출 시작
void APFKwang::AttackStart()
{
	bSwordHitDetectionActive = true;
	HitActorsDuringAttack.Reset();
	if (GetNetMode() == NM_DedicatedServer) return;

	// 검 궤적 준비, 재생
	if (IsValid(SwordTrail))
	{
		SwordTrail->EndTrails();
	}
	else if (Particles[etoi(SWORDTRAIL)])
	{
		SwordTrail = NewObject<UParticleSystemComponent>(GetMesh());
		SwordTrail->bAutoDestroy = false;
		SwordTrail->bAllowRecycling = true;
		SwordTrail->SecondsBeforeInactive = 0.f;
		SwordTrail->bAutoActivate = false;
		SwordTrail->bOverrideLODMethod = false;
		SwordTrail->bAutoManageAttachment = false;
		SwordTrail->SetTemplate(Particles[etoi(SWORDTRAIL)]);
		SwordTrail->RegisterComponentWithWorld(GetWorld());
	}

	if (IsValid(SwordTrail))
	{
		SwordTrail->CustomTimeDilation = 0.7f;
		if (SwordTrail->GetAttachParent() != GetPresentationMesh())
		{
			UpdateSwordTrailAttachment();
		}
		else
		{
			SwordTrail->BeginTrails(FName("FX_weapon_base"), FName("FX_weapon_tip"), ETrailWidthMode_FromCentre, 2.f);
		}
	}
	else
	{
		PFLOG(Warning, TEXT("SwordTrail Spawn Failed"));
	}

	UGameplayStatics::PlaySound2D(this, Sounds[etoi(SLASH)]);
}

// 표시 메시 변경 시 검 궤적 재연결
void APFKwang::UpdateSwordTrailAttachment()
{
	if (!IsValid(SwordTrail)) return;
	USkeletalMeshComponent* PresentationMesh = GetPresentationMesh();
	SwordTrail->SetVisibility(PresentationMesh->IsVisible());
	if (SwordTrail->GetAttachParent() == PresentationMesh) return;
	SwordTrail->EndTrails();
	SwordTrail->DeactivateImmediate();
	SwordTrail->AttachToComponent(PresentationMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
	SwordTrail->SetOnlyOwnerSee(IsFirstPersonPresentationActive());
	SwordTrail->SetFirstPersonPrimitiveType(IsFirstPersonPresentationActive() ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
	if (bSwordHitDetectionActive)
	{
		SwordTrail->BeginTrails(TEXT("FX_weapon_base"), TEXT("FX_weapon_tip"), ETrailWidthMode_FromCentre, 2.f);
	}
}

// 검 판정, 궤적 종료
void APFKwang::AttackEnd()
{
	bSwordHitDetectionActive = false;
	HitActorsDuringAttack.Reset();

	if (IsValid(SwordTrail))
	{
		SwordTrail->EndTrails();
	}
}

// 칼자루, 칼끝을 포함한 검 OBB 갱신
bool APFKwang::UpdateSwordCollision()
{
	const FTransform BaseTransform = GetMesh()->GetSocketTransform(TEXT("FX_weapon_base"));
	const FVector SwordBase = BaseTransform.GetLocation();
	const FVector SwordSegment = GetMesh()->GetSocketLocation(TEXT("FX_weapon_tip")) - SwordBase;
	const float BladeLength = SwordSegment.Size();
	if (BladeLength <= KINDA_SMALL_NUMBER) return false;

	const FVector SwordDirection = SwordSegment / BladeLength;
	const float SwordScale = BaseTransform.GetScale3D().GetAbsMax();
	const float HiltPadding = 60.f * SwordScale;
	const float TipPadding = 20.f * SwordScale;
	SwordCollisionCenter = SwordBase + SwordDirection * ((BladeLength + TipPadding - HiltPadding) * 0.5f);
	SwordCollisionExtent = FVector((BladeLength + HiltPadding + TipPadding) * 0.5f,
		25.f * SwordScale, 20.f * SwordScale);
	SwordCollisionRotation = FRotationMatrix::MakeFromXZ(SwordDirection, BaseTransform.GetUnitAxis(EAxis::Z)).ToQuat();
	return true;
}

// 검 OBB의 피격 대상 처리
void APFKwang::ProcessSwordHits()
{
	if (const APFCampaignEnemyController* CampaignAI = Cast<APFCampaignEnemyController>(GetController());
		CampaignAI && CampaignAI->IsBossPatternActive()) return;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KwangSwordAttack), false, this);
	static const ECollisionChannel PFCharacterCollisionChannel = GetCollisionChannel(PFCollisionChannelNames::PFCharacter);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(PFCharacterCollisionChannel);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, SwordCollisionCenter, SwordCollisionRotation,
		ObjectQueryParams, FCollisionShape::MakeBox(SwordCollisionExtent), QueryParams);

	// 대상별 한 번씩 피해 적용
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APFCharacter* HitCharacter = Cast<APFCharacter>(Overlap.GetActor());
		if (!IsValid(HitCharacter) || HitCharacter == this || HitActorsDuringAttack.Contains(HitCharacter))
		{
			continue;
		}

		if (IsEnemyCharacter() && (!HitCharacter->IsPlayerCharacter() || !HitCharacter->IsPlayerControlled()
			|| HitCharacter->IsDeadCharacter() || HitCharacter->HasStateTag(PFGameplayTags::Character_State_Invulnerable)))
		{
			continue;
		}

		const FVector HitLocation = HitCharacter->GetActorLocation();
		const FHitResult HitResult(HitCharacter, Overlap.GetComponent(), HitLocation,
			(HitLocation - SwordCollisionCenter).GetSafeNormal());
		HitActorsDuringAttack.Add(HitCharacter);
		ApplyAttackDamageTo(HitCharacter, GetDamage(), this, nullptr, &HitResult);
	}
}

void APFKwang::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnMontageEnd(Montage, bInterrupted);

	int MontageIdx = PFAnim->MontageEndTask(Montage);

	// 공격 몽타주 종료 시 검 판정 해제
	switch (MontageIdx)
	{
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKA):
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKB):
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKC):
	case etoi(UPFAnimInst_Kwang::MTGIDX_K::ATTACKD):
		AttackEnd();
		break;
	default:
		break;
	}


}

void APFKwang::PostHitProcessing()
{
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(HIT)]);
}
