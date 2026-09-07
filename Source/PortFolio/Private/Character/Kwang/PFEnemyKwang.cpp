#include "Character/Kwang/PFEnemyKwang.h"

#include "Animation/PFAnimInst_Kwang.h"
#include "GAS/Abilities/Attack/PFGA_Attack_Kwang.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

using enum UPFAnimInst_Kwang::MTGIDX_K;

APFEnemyKwang::APFEnemyKwang()
{
	AttackAbilityClass = UPFGA_Attack_Kwang::StaticClass();

	DesiredCombatDistance = 150.f;
	DistanceTolerance = 0.f;
	AttackRange = 200.f;
	bRetreatWhenTooClose = false;
	SetMesh();
	SetParticle();
	SetSound();
}

void APFEnemyKwang::PostInitializeComponents()
{
	PFAnim = Cast<UPFAnimInst_Kwang>(GetMesh()->GetAnimInstance());
	PFCHECK(nullptr != PFAnim);
	Super::PostInitializeComponents();

	UPFAnimInst_Kwang* KwangAnim = Cast<UPFAnimInst_Kwang>(PFAnim);
	// 검 판정 이벤트 연결
	KwangAnim->AttackStart.AddUObject(this, &APFEnemyKwang::SwordAttackStart);
	KwangAnim->AttackEnd.AddUObject(this, &APFEnemyKwang::SwordAttackEnd);

	GetMesh()->SetCollisionProfileName(TEXT("PFCharacter"));
}

void APFEnemyKwang::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && bSwordHitDetectionActive)
	{
		ProcessSwordHits();
	}
}

void APFEnemyKwang::SetMesh()
{
	// 광 메시 설정
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> KwangMesh(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_GDC.Kwang_GDC"));
	if (KwangMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(KwangMesh.Object);
	}
	else
	{
		PFLOG(Warning, TEXT("Enemy Kwang mesh failed"));
	}

	// 광 애니메이션 연결
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	static ConstructorHelpers::FClassFinder<UPFAnimInst_Kwang> KwangAnimBlueprint(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Kwang_Blueprint.Kwang_Blueprint_C"));
	if (KwangAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(KwangAnimBlueprint.Class);
	}
	else
	{
		PFLOG(Warning, TEXT("Enemy Kwang animation blueprint failed"));
	}
}

void APFEnemyKwang::SetParticle()
{
	// 검 궤적 이펙트 로드
	Particles.SetNum(etoi(PARTICLE_END));

	Particles[etoi(SWORDTRAIL)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonKwang/FX/Particles/Abilities/Primary/FX/P_Kwang_Primary_Trail.P_Kwang_Primary_Trail'"));
	if (!Particles[etoi(SWORDTRAIL)])
	{
		PFLOG(Warning, TEXT("Enemy Kwang sword trail failed"));
	}
}

void APFEnemyKwang::SetSound()
{
	// 검 공격, 피격 사운드 로드
	Sounds.SetNum(etoi(SOUND_END));

	Sounds[etoi(SLASH)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Whoosh_1-1.Whoosh_1-1'"));
	if (!Sounds[etoi(SLASH)])
	{
		PFLOG(Warning, TEXT("Enemy Kwang slash sound failed"));
	}

	Sounds[etoi(HIT)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Hit_Generic_2-1.Hit_Generic_2-1'"));
	if (!Sounds[etoi(HIT)])
	{
		PFLOG(Warning, TEXT("Enemy Kwang hit sound failed"));
	}
}

// 공격 Cue의 콤보 몽타주 재생
void APFEnemyKwang::GameplayCue_Character_Attack_Kwang_Normal_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed || !PFAnim)
	{
		return;
	}

	const int32 MontageIndex = FMath::RoundToInt(Parameters.RawMagnitude);
	if (MontageIndex < etoi(ATTACKA) || MontageIndex > etoi(ATTACKD))
	{
		return;
	}

	PFAnim->PlayMontage(MontageIndex);
}

// 검 판정, 공격 연출 시작
void APFEnemyKwang::SwordAttackStart()
{
	// 피격 기록, 검 위치 초기화
	bSwordHitDetectionActive = true;
	HitActorsDuringAttack.Empty();
	PreviousSwordBaseLocation = GetMesh()->GetSocketLocation(FName("FX_weapon_base"));
	PreviousSwordTipLocation = GetMesh()->GetSocketLocation(FName("FX_weapon_tip"));

	// 검 궤적 준비, 재생
	if (IsValid(SwordTrail))
	{
		SwordTrail->EndTrails();
	}
	else if (Particles.IsValidIndex(etoi(SWORDTRAIL)) && Particles[etoi(SWORDTRAIL)] && GetWorld())
	{
		SwordTrail = NewObject<UParticleSystemComponent>(GetMesh());
		SwordTrail->bAutoDestroy = false;
		SwordTrail->bAllowRecycling = true;
		SwordTrail->SecondsBeforeInactive = 0.f;
		SwordTrail->bAutoActivate = false;
		SwordTrail->bOverrideLODMethod = false;
		SwordTrail->bAutoManageAttachment = true;
		SwordTrail->SetAutoAttachParams(GetMesh(), NAME_None);
		SwordTrail->SetTemplate(Particles[etoi(SWORDTRAIL)]);
		SwordTrail->RegisterComponentWithWorld(GetWorld());
		SwordTrail->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (IsValid(SwordTrail))
	{
		SwordTrail->CustomTimeDilation = 0.7f;
		SwordTrail->BeginTrails(FName("FX_weapon_base"), FName("FX_weapon_tip"), ETrailWidthMode_FromCentre, 2.f);
	}
	else
	{
		PFLOG(Warning, TEXT("Enemy Kwang sword trail spawn failed"));
	}

	UGameplayStatics::PlaySound2D(this, Sounds[etoi(SLASH)]);
}

// 검 판정, 궤적 종료
void APFEnemyKwang::SwordAttackEnd()
{
	bSwordHitDetectionActive = false;
	HitActorsDuringAttack.Empty();

	if (IsValid(SwordTrail))
	{
		SwordTrail->EndTrails();
	}
}

// 검 궤적의 피격 대상 처리
void APFEnemyKwang::ProcessSwordHits()
{
	UWorld* World = GetWorld();
	if (!World || !GetMesh())
	{
		return;
	}

	// 이전, 현재 검 위치 계산
	const FVector CurrentSwordBaseLocation = GetMesh()->GetSocketLocation(FName("FX_weapon_base"));
	const FVector CurrentSwordTipLocation = GetMesh()->GetSocketLocation(FName("FX_weapon_tip"));
	const FVector PreviousSwordMiddleLocation = (PreviousSwordBaseLocation + PreviousSwordTipLocation) * 0.5f;
	const FVector CurrentSwordMiddleLocation = (CurrentSwordBaseLocation + CurrentSwordTipLocation) * 0.5f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyKwangSwordAttack), false, this);
	QueryParams.AddIgnoredActor(this);

	ECollisionChannel PFCharacterCollisionChannel;
	if (!GetCollisionChannel(PFCollisionChannelNames::PFCharacter, PFCharacterCollisionChannel))
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(PFCharacterCollisionChannel);

	// 검의 이동 구간, 길이를 스윕 검사
	TArray<FHitResult> HitResults;
	const FCollisionShape SwordShape = FCollisionShape::MakeSphere(SwordTraceRadius);

	auto AppendSweepHits = [&](const FVector& Start, const FVector& End)
	{
		TArray<FHitResult> SweepHits;
		World->SweepMultiByObjectType(SweepHits, Start, End, FQuat::Identity, ObjectQueryParams, SwordShape, QueryParams);
		HitResults.Append(SweepHits);
	};

	AppendSweepHits(PreviousSwordBaseLocation, CurrentSwordBaseLocation);
	AppendSweepHits(PreviousSwordMiddleLocation, CurrentSwordMiddleLocation);
	AppendSweepHits(PreviousSwordTipLocation, CurrentSwordTipLocation);
	AppendSweepHits(CurrentSwordBaseLocation, CurrentSwordTipLocation);

	// 대상별 한 번씩 피해 적용
	for (const FHitResult& HitResult : HitResults)
	{
		APFCharacter* HitCharacter = Cast<APFCharacter>(HitResult.GetActor());
		if (!IsPlayerTargetValid(HitCharacter) || HitActorsDuringAttack.Contains(HitCharacter))
		{
			continue;
		}

		HitActorsDuringAttack.Add(HitCharacter);
		ApplyAttackDamageTo(HitCharacter, GetDamage(), this, nullptr, &HitResult);
	}

	PreviousSwordBaseLocation = CurrentSwordBaseLocation;
	PreviousSwordTipLocation = CurrentSwordTipLocation;
}

void APFEnemyKwang::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnMontageEnd(Montage, bInterrupted);

	const int MontageIndex = PFAnim->MontageEndTask(Montage);

	// 공격 몽타주 종료 시 검 판정 해제
	switch (MontageIndex)
	{
	case etoi(ATTACKA):
	case etoi(ATTACKB):
	case etoi(ATTACKC):
	case etoi(ATTACKD):
		SwordAttackEnd();
		break;
	default:
		break;
	}

}

void APFEnemyKwang::PostHitProcessing()
{
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(HIT)]);
}
