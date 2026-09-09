#include "Character/TwinBlast/PFEnemyTwinblast.h"

#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast.h"

#include "Animation/PFAnimInst_TwinBlast.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

APFEnemyTwinblast::APFEnemyTwinblast()
{
	AttackAbilityClass = UPFGA_Attack_TwinBlast::StaticClass();

	DesiredCombatDistance = 1000.f;
	DistanceTolerance = 0.f;
	AttackRange = 2000.f;
	bRetreatWhenTooClose = true;

	SetMesh();
	SetParticle();
	SetSound();
}

void APFEnemyTwinblast::PostInitializeComponents()
{
	PFAnim = Cast<UPFAnimInst_TwinBlast>(GetMesh()->GetAnimInstance());
	PFCHECK(nullptr != PFAnim);
	Super::PostInitializeComponents();

	GetMesh()->SetCollisionProfileName(TEXT("PFCharacter"));
}

void APFEnemyTwinblast::SetMesh()
{
	// 트윈블라스트 메시 설정
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TwinblastMesh(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast.TwinBlast"));
	if (TwinblastMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(TwinblastMesh.Object);
	}
	else
	{
		PFLOG(Fatal, TEXT("Enemy Twinblast mesh failed"));
	}

	// 트윈블라스트 애니메이션 연결
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	static ConstructorHelpers::FClassFinder<UPFAnimInst_TwinBlast> TwinblastAnimBlueprint(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/TwinBlast_Blueprint.TwinBlast_Blueprint_C"));
	if (TwinblastAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(TwinblastAnimBlueprint.Class);
	}
	else
	{
		PFLOG(Fatal, TEXT("Enemy Twinblast animation blueprint failed"));
	}
	
	GetMesh()->SetIsReplicated(true);
}

void APFEnemyTwinblast::SetParticle()
{
	// 총구 이펙트 로드
	Particles.SetNum(etoi(PARTICLE_END));

	Particles[etoi(MUZZLELEFT)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Primary/FX/P_TwinBlast_Primary_MuzzleFlashLeft.P_TwinBlast_Primary_MuzzleFlashLeft'"));
	if (!Particles[etoi(MUZZLELEFT)])
	{
		PFLOG(Warning, TEXT("Enemy Twinblast left muzzle particle failed"));
	}

	Particles[etoi(MUZZLERIGHT)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Primary/FX/P_TwinBlast_Primary_MuzzleFlash.P_TwinBlast_Primary_MuzzleFlash'"));
	if (!Particles[etoi(MUZZLERIGHT)])
	{
		PFLOG(Warning, TEXT("Enemy Twinblast right muzzle particle failed"));
	}
}

void APFEnemyTwinblast::SetSound()
{
	// 발사, 피격 사운드 로드
	Sounds.SetNum(etoi(SOUND_END));

	Sounds[etoi(SHOOT)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Explosion_Medium_2-1.Explosion_Medium_2-1'"));
	if (!Sounds[etoi(SHOOT)])
	{
		PFLOG(Warning, TEXT("Enemy Twinblast shoot sound failed"));
	}

	Sounds[etoi(HIT)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Hit_Generic_2-1.Hit_Generic_2-1'"));
	if (!Sounds[etoi(HIT)])
	{
		PFLOG(Warning, TEXT("Enemy Twinblast hit sound failed"));
	}
}

bool APFEnemyTwinblast::ShouldAttackTarget(const APFCharacter* Target, float SurfaceDistance) const
{
	if (!IsPlayerTargetValid(Target) || SurfaceDistance > AttackRange)
	{
		return false;
	}

	return HasClearSightToTarget(Target);
}

bool APFEnemyTwinblast::ShouldApproachTarget(const APFCharacter* Target, float SurfaceDistance) const
{
	return IsPlayerTargetValid(Target)
		&& (Super::ShouldApproachTarget(Target, SurfaceDistance) || !HasClearSightToTarget(Target));
}

// 대상까지 시야 확인
bool APFEnemyTwinblast::HasClearSightToTarget(const APFCharacter* Target) const
{
	if (!IsPlayerTargetValid(Target))
	{
		return false;
	}

	const FVector SightStart = GetPawnViewLocation();
	const FVector SightEnd = Target->GetPawnViewLocation();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams SightQueryParams(SCENE_QUERY_STAT(EnemyTwinblastSight), false, this);
	SightQueryParams.AddIgnoredActor(this);
	FHitResult SightHit;
	const bool bHasBlockingHit = World->LineTraceSingleByChannel(
		SightHit,
		SightStart,
		SightEnd,
		ECC_Visibility,
		SightQueryParams);

	return !bHasBlockingHit || SightHit.GetActor() == Target;
}

// 일반 공격 몽타주 재생
void APFEnemyTwinblast::GameplayCue_Character_Attack_Twinblast_Normal_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed || !PFAnim)
	{
		return;
	}

	const bool bGCShootLeft = Parameters.RawMagnitude > 0.5f;
	PFAnim->PlayMontage(bGCShootLeft ? etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::LEFTATTACK) : etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::RIGHTATTACK));
}

// 일반 공격 발사 연출
void APFEnemyTwinblast::GameplayCue_Character_Attack_Twinblast_Normal_Shoot(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bGCShootLeft = Parameters.RawMagnitude > 0.5f;
	const FName SocketName = bGCShootLeft ? FName("Muzzle_02") : FName("Muzzle_01");
	const PARTICLE MuzzleParticle = bGCShootLeft ? MUZZLELEFT : MUZZLERIGHT;
	UGameplayStatics::SpawnEmitterAttached(Particles[etoi(MuzzleParticle)], GetMesh(), SocketName, FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.f), EAttachLocation::SnapToTarget, true);
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(SHOOT)]);
}

void APFEnemyTwinblast::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnMontageEnd(Montage, bInterrupted);

	const int MontageIndex = PFAnim->MontageEndTask(Montage);
	if (MontageIndex == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::MONTAGE_END))
	{
		return;
	}

	// 공격 종료 후 콤보, 발사 방향 초기화
	if (!IsAttackCommandActive())
	{
		if (MontageIndex == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::LEFTATTACK) || MontageIndex == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::RIGHTATTACK))
		{
			PFAnim->ResetAttackCombo();
		}

		bShootLeft = true;
	}
}

void APFEnemyTwinblast::PostHitProcessing()
{
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(HIT)]);
}

void APFEnemyTwinblast::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APFEnemyTwinblast, bShootLeft);
}
