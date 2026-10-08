

#include "Character/TwinBlast/PFTwinBlast.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Animation/PFAnimInst_TwinBlast.h"
#include "AbilitySystemComponent.h"
#include "GAS/PFGameplayTags.h"
#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast.h"
#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast_Ultimate.h"
#include "GAS/Abilities/Ultimate/PFGA_Ultimate_TwinBlast.h"
#include "Character/TwinBlast/UltGun.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/PlayerController.h"

APFTwinBlast::APFTwinBlast() : ShootLeft(true), UltGun(nullptr), UltShoulderEffect(nullptr)
{
	bHasCrosshair = true;
	AISettings.DesiredCombatDistance = 1000.f;
	AISettings.DistanceTolerance = 0.f;
	AISettings.AttackRange = 2000.f;
	AISettings.bRetreatWhenTooClose = true;
	AISettings.bRequiresLineOfSight = true;
	AttackAbilityClass = UPFGA_Attack_TwinBlast::StaticClass();
	UltimateAttackAbilityClass = UPFGA_Attack_TwinBlast_Ultimate::StaticClass();
	UltimateAbilityClass = UPFGA_Ultimate_TwinBlast::StaticClass();

	SetMesh();
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ARMS(TEXT("/Game/GameData/Characters/FirstPerson/SK_Twinblast_Arms.SK_Twinblast_Arms"));
	FirstPersonMeshAsset = ARMS.Object;
	FirstPersonNeckOffset = FVector(-20.f, 0.f, -16.f);
	SetParticle();
	SetSound();
}

void APFTwinBlast::InitAbilityActorInfo()
{
	EnsureUltimatePresentation();
	Super::InitAbilityActorInfo();
	GiveUltimateAttackAbility();
}

// 궁극기 공격 어빌리티 부여
void APFTwinBlast::GiveUltimateAttackAbility()
{
	if (!HasAuthority() || !IsPlayerCharacter() || !ASC || !UltimateAttackAbilityClass)
	{
		return;
	}

	const FGameplayAbilitySpecHandle UltimateAttackAbilityHandle = GetOrGiveAbility(UltimateAttackAbilityClass, 1);
	if (!UltimateAttackAbilityHandle.IsValid())
	{
		PFLOG(Warning, TEXT("Failed to give TwinBlast ultimate attack ability"));
	}
}

void APFTwinBlast::PostInitializeComponents()
{
	PFAnim = Cast<UPFAnimInst_TwinBlast>(GetMesh()->GetAnimInstance());
	checkf(PFAnim, TEXT("TwinBlast AnimInstance is required"));
	Super::PostInitializeComponents();

	GetMesh()->SetCollisionProfileName(TEXT("PFCharacter"));

}

void APFTwinBlast::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopUltShoulderEffect();
	UnbindStateTagEvents();
	// 캐릭터 제거 시 궁극기 상태 해제
	if (HasAuthority() && ASC && ASC->GetAvatarActor() == this)
	{
		SetReplicatedStateTag(PFGameplayTags::Character_State_Ultimate, false);
	}

	// 궁극기 총 제거
	if (IsValid(UltGun))
	{
		UltGun->Destroy();
		UltGun = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void APFTwinBlast::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (UltGun) UltGun->SetFirstPersonPresentation(IsFirstPersonPresentationActive() ? GetPresentationMesh() : nullptr);
	if (UltShoulderEffect) UltShoulderEffect->SetOwnerNoSee(IsFirstPersonPresentationActive());

	// 궁극기 이동 속도 적용
	if (IsUltimateActive())
	{
		GetCharacterMovement()->MaxWalkSpeed = UltSpeed;
	}
}

void APFTwinBlast::OnRep_FinalDir()
{
	Super::OnRep_FinalDir();
	if (UltGun)
	{
		UltGun->SetCurrentDir(FinalDir);
	}
}

void APFTwinBlast::SetMesh()
{
	UPFGameInstanceSubsystem::ApplyCharacterMesh(GetMesh(), CHARACTER_TWINBLAST, true);
	GetMesh()->SetIsReplicated(false);
}

void APFTwinBlast::SetParticle()
{
	static const TCHAR* const Paths[] =
	{
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Primary/FX/P_TwinBlast_Primary_MuzzleFlashLeft.P_TwinBlast_Primary_MuzzleFlashLeft'"),
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Primary/FX/P_TwinBlast_Primary_MuzzleFlash.P_TwinBlast_Primary_MuzzleFlash'"),
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Ultimate/FX/P_TwinBlast_Ultimate_MuzzleFlash_L.P_TwinBlast_Ultimate_MuzzleFlash_L'"),
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Ultimate/FX/P_TwinBlast_Ultimate_MuzzleFlash_L.P_TwinBlast_Ultimate_MuzzleFlash_L'"),
		TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/Abilities/Ultimate/FX/P_Activate_Ult_Reticules.P_Activate_Ult_Reticules'"),
		TEXT("ParticleSystem'/Game/MyFiles/FX/P_TwinBlast_Ult2_ShouldersLooping_UE58Fix.P_TwinBlast_Ult2_ShouldersLooping_UE58Fix'")
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(PARTICLE_END));
	UPFGameInstanceSubsystem::LoadAssets(Particles, Paths);
}

void APFTwinBlast::SetSound()
{
	static const TCHAR* const Paths[] =
	{
		TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Explosion_Medium_2-1.Explosion_Medium_2-1'"),
		TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Explosion_Large_1-1.Explosion_Large_1-1'"),
		TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Hit_Generic_2-1.Hit_Generic_2-1'")
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(SOUND_END));
	UPFGameInstanceSubsystem::LoadAssets(Sounds, Paths);
}

void APFTwinBlast::Jump()
{
	if (IsUltimateActive())
	{
		return;
	}

	Super::Jump();
}

bool APFTwinBlast::CanSprint() const
{
	return !IsUltimateActive();
}

TSubclassOf<UGameplayAbility> APFTwinBlast::GetAttackAbilityClass() const
{
	const FGameplayTag UltimateStateTag = PFGameplayTags::Character_State_Ultimate;
	return ASC && ASC->HasMatchingGameplayTag(UltimateStateTag)
		? UltimateAttackAbilityClass
		: AttackAbilityClass;
}

// 궁극기 상태 전환
void APFTwinBlast::ToggleUltimateState()
{
	if (HasAuthority() && IsPlayerCharacter())
	{
		SetReplicatedStateTag(PFGameplayTags::Character_State_Ultimate, !IsUltimateActive());
	}
}

// 궁극기 활성 여부 조회
bool APFTwinBlast::IsUltimateActive() const
{
	return HasStateTag(PFGameplayTags::Character_State_Ultimate);
}

void APFTwinBlast::UltimateTagChanged(FGameplayTag StateTag, int32 NewCount)
{
	Super::UltimateTagChanged(StateTag, NewCount);
	if (!IsActorBeingDestroyed())
	{
		ApplyUltimateState();
	}
}

// 궁극기 전환 상태 반영
void APFTwinBlast::ApplyUltimateState()
{
	if (IsDeadCharacter())
	{
		return;
	}
	// 전환 중 행동 차단, 이동 초기화
	SetBlockTags(true);
	if (HasAuthority() && IsUltimateActive() && IsSprinting())
	{
		SetReplicatedStateTag(PFGameplayTags::Character_State_Sprinting, false);
	}

	GetCharacterMovement()->StopMovementImmediately();
	FinalDir = IDLE;
	MovementInputDirection = IDLE;
	PFAnim->SetCurrentDir(IDLE);
	if (UltGun) UltGun->SetCurrentDir(IDLE);

	// 캐릭터, 총 전환 연출
	if(IsUltimateActive())
	{
		PFAnim->PlayMontage(etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTSTART));
		if (UltGun) UltGun->PlayMontage(etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTSTART));
	}
	else
	{
		StopUltShoulderEffect();
		PFAnim->PlayMontage(etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTEND));
		if (UltGun) UltGun->PlayMontage(etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTEND));
	}

}

// 일반 공격 몽타주 재생
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Normal_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed)
	{
		return;
	}

	const bool bGCShootLeft = Parameters.RawMagnitude > 0.5f;
	PFAnim->PlayMontage(bGCShootLeft ? etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::LEFTATTACK) : etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::RIGHTATTACK));
}

// 궁극기 공격 몽타주 재생
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Ultimate_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	(void)Parameters;
	UPFAnimInst_TwinBlast* TwinBlastAnim = static_cast<UPFAnimInst_TwinBlast*>(PFAnim);
	if (EventType == EGameplayCueEvent::Executed && !TwinBlastAnim->IsUltimateAttackMontagePlaying())
	{
		TwinBlastAnim->PlayMontage(etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTATTACK));
	}
}

// 일반 공격 발사 연출
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Normal_Shoot(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed
		|| (Parameters.NormalizedMagnitude > 0.5f && IsLocallyControlled() && !HasAuthority()))
	{
		return;
	}
	PlayShotEffects(Parameters.RawMagnitude > 0.5f);
}

// 총구 효과, 발사음 재생
void APFTwinBlast::PlayShotEffects(bool bShootLeft, bool bUltimate)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const FName SocketName = bUltimate
		? (bShootLeft ? FName("Muzzle_04") : FName("Muzzle_03"))
		: (bShootLeft ? FName("Muzzle_02") : FName("Muzzle_01"));
	const PARTICLE MuzzleParticle = bUltimate
		? (bShootLeft ? ULTMUZZLELEFT : ULTMUZZLERIGHT) : (bShootLeft ? MUZZLELEFT : MUZZLERIGHT);
	if (UParticleSystemComponent* Muzzle = UGameplayStatics::SpawnEmitterAttached(Particles[etoi(MuzzleParticle)], GetPresentationMesh(), SocketName, FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.f), EAttachLocation::SnapToTarget, true))
	{
		Muzzle->SetOnlyOwnerSee(IsFirstPersonPresentationActive());
		Muzzle->SetFirstPersonPrimitiveType(IsFirstPersonPresentationActive() ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
	}
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(bUltimate ? ULTSHOOT : SHOOT)]);
}

// 궁극기 발사 연출
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Ultimate_Shoot(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		PlayShotEffects(Parameters.RawMagnitude > 0.5f, true);
	}
}

// 궁극기 어깨 이펙트 재생
void APFTwinBlast::StartUltShoulderEffect()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IsValid(UltShoulderEffect))
	{
		UltShoulderEffect->ActivateSystem(true);
		return;
	}

	UltShoulderEffect = UGameplayStatics::SpawnEmitterAttached(
		Particles[etoi(ULTSHOULDER)],
		GetMesh(),
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		FVector(1.f),
		EAttachLocation::KeepRelativeOffset,
		false,
		EPSCPoolMethod::None,
		true);

	if (!UltShoulderEffect)
	{
		PFLOG(Warning, TEXT("Ult shoulder effect spawn failed"));
	}
}

// 궁극기 어깨 이펙트 중단
void APFTwinBlast::StopUltShoulderEffect()
{
	if (IsValid(UltShoulderEffect))
	{
		UltShoulderEffect->DeactivateSystem();
	}
}

void APFTwinBlast::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnMontageEnd(Montage, bInterrupted);

	int MontageIdx = PFAnim->MontageEndTask(Montage);
	if (MontageIdx == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::MONTAGE_END))
	{
		return;
	}

	// 궁극기 전환 종료 후 행동, 이펙트 반영
	switch (MontageIdx)
	{
	case etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTSTART):
	{
		SetBlockTags(false);

		if (IsUltimateActive())
		{
			StartUltShoulderEffect();
		}
	}
		break;
	case etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTEND):
		StopUltShoulderEffect();
		SetBlockTags(false);
		break;
	default:
		break;
	}

	if (!IsAttackCommandActive() && (MontageIdx == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::LEFTATTACK) || MontageIdx == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::RIGHTATTACK)))
	{
		PFAnim->ResetAttackCombo();
	}

	if (!IsAttackCommandActive())
	{
		ShootLeft = true;
	}
}

void APFTwinBlast::PostHitProcessing()
{
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(HIT)]);
}

void APFTwinBlast::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APFTwinBlast, ShootLeft);
}

// 플레이어 궁극기 총 준비
void APFTwinBlast::EnsureUltimatePresentation()
{
	if (!IsPlayerCharacter() || IsValid(UltGun) || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// 궁극기 총 생성, 부착
	UltGun = GetWorld()->SpawnActor<AUltGun>(AUltGun::StaticClass(), GetMesh()->GetSocketLocation(FName("UltGunAttach")), GetActorRotation());

	if (UltGun)
	{
		UltGun->SetOwner(this);
		UltGun->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, FName("UltGunAttach"));
		PFLOG(Warning, TEXT("UltGun Spawn Succeed"));
	}
	else
	{
		PFLOG(Warning, TEXT("UltGun Spawn Failed"));
	}

}
