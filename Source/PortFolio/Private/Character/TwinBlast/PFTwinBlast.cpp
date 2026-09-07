

#include "Character/TwinBlast/PFTwinBlast.h"
#include "Animation/PFAnimInst_TwinBlast.h"
#include "AbilitySystemComponent.h"
#include "GAS/PFGameplayTags.h"
#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast.h"
#include "GAS/Abilities/Attack/PFGA_Attack_TwinBlast_Ultimate.h"
#include "GAS/Abilities/Ultimate/PFGA_Ultimate_TwinBlast.h"
#include "UI/HUD/PFCrosshairWidget.h"
#include "Character/TwinBlast/UltGun.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/PlayerController.h"

using enum UPFAnimInst_TwinBlast::MTGIDX_TB;

APFTwinBlast::APFTwinBlast() : ShootLeft(true), UltGun(nullptr), UltShoulderEffect(nullptr)
{
	AttackAbilityClass = UPFGA_Attack_TwinBlast::StaticClass();
	UltimateAttackAbilityClass = UPFGA_Attack_TwinBlast_Ultimate::StaticClass();
	UltimateAbilityClass = UPFGA_Ultimate_TwinBlast::StaticClass();

	SetMesh();
	SetParticle();
	SetSound();
}

void APFTwinBlast::InitAbilityActorInfo()
{
	Super::InitAbilityActorInfo();
	GiveUltimateAttackAbility();
}

// 궁극기 공격 어빌리티 부여
void APFTwinBlast::GiveUltimateAttackAbility()
{
	if (!HasAuthority() || !ASC || !UltimateAttackAbilityClass)
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
	PFCHECK(nullptr != PFAnim);
	Super::PostInitializeComponents();

	GetMesh()->SetCollisionProfileName(TEXT("PFCharacter"));

	// 궁극기 총 생성, 부착
	UltGun = GetWorld()->SpawnActor<AUltGun>(AUltGun::StaticClass(), GetMesh()->GetSocketLocation(FName("UltGunAttach")), GetActorRotation());
	
	if (UltGun)
	{
		UltGun->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, FName("UltGunAttach"));
		PFLOG(Warning, TEXT("UltGun Spawn Succeed"));
	}
	else
	{
		PFLOG(Warning, TEXT("UltGun Spawn Failed"));
	}
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

	// 공격 중 조준점 갱신
	if (IsLocallyControlled() && IsAttackCommandActive())
	{
		Server_UpdateAimPoint(CalculateAimPoint());
	}

	// 궁극기 이동 속도 적용
	if (IsUltimateActive())
	{
		GetCharacterMovement()->MaxWalkSpeed = UltSpeed;
	}
}

void APFTwinBlast::SetDir()
{
	Super::SetDir();
	if (UltGun)
	{
		UltGun->SetCurrentDir(FinalDir);
	}
}

void APFTwinBlast::SetMesh()
{
	// 트윈블라스트 메시 설정
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TWINBLAST(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast.TwinBlast"));
	if (TWINBLAST.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(TWINBLAST.Object);
	}
	else
	{
		PFLOG(Fatal, TEXT("Mesh Failed"));
	}

	// 트윈블라스트 애니메이션 연결
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	static ConstructorHelpers::FClassFinder<UPFAnimInst_TwinBlast> PORTFOLIO_BLUEPRINT(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/TwinBlast_Blueprint.TwinBlast_Blueprint_C"));

	if (PORTFOLIO_BLUEPRINT.Succeeded())
	{
		PFLOG(Warning, TEXT("BluePrint Succeed"));
		GetMesh()->SetAnimInstanceClass(PORTFOLIO_BLUEPRINT.Class);
	}
	else
	{
		PFLOG(Fatal, TEXT("BluePrint Failed"));
	}
	GetMesh()->SetIsReplicated(true);
}

void APFTwinBlast::SetParticle()
{
	// 총구, 궁극기 이펙트 로드
	Particles.SetNum(etoi(PARTICLE_END));

	Particles[etoi(MUZZLELEFT)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/"
		"Abilities/Primary/FX/P_TwinBlast_Primary_MuzzleFlashLeft.P_TwinBlast_Primary_MuzzleFlashLeft'"));
	if (!Particles[etoi(MUZZLELEFT)])
	{
		PFLOG(Warning, TEXT("MuzzleLeft Failed"));
	}
	Particles[etoi(MUZZLERIGHT)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/"
		"Abilities/Primary/FX/P_TwinBlast_Primary_MuzzleFlash.P_TwinBlast_Primary_MuzzleFlash'"));
	if (!Particles[etoi(MUZZLERIGHT)])
	{
		PFLOG(Warning, TEXT("MuzzleRight Failed"));
	}
	Particles[etoi(ULTMUZZLELEFT)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/"
		"Abilities/Ultimate/FX/P_TwinBlast_Ultimate_MuzzleFlash_L.P_TwinBlast_Ultimate_MuzzleFlash_L'"));
	if (!Particles[etoi(ULTMUZZLELEFT)])
	{
		PFLOG(Warning, TEXT("UltMuzzleLeft Failed"));
	}
	Particles[etoi(ULTMUZZLERIGHT)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/"
		"Abilities/Ultimate/FX/P_TwinBlast_Ultimate_MuzzleFlash_L.P_TwinBlast_Ultimate_MuzzleFlash_L'"));
	if (!Particles[etoi(ULTMUZZLERIGHT)])
	{
		PFLOG(Warning, TEXT("UltMuzzleRight Failed"));
	}
	Particles[etoi(ULTACTIVATE)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/ParagonTwinblast/FX/Particles/"
		"Abilities/Ultimate/FX/P_Activate_Ult_Reticules.P_Activate_Ult_Reticules'"));
	if (!Particles[etoi(ULTACTIVATE)])
	{
		PFLOG(Warning, TEXT("UltActivate Failed"));
	}
	Particles[etoi(ULTSHOULDER)] = LoadObject<UParticleSystem>(nullptr, TEXT("ParticleSystem'/Game/MyFiles/FX/"
		"P_TwinBlast_Ult2_ShouldersLooping_UE58Fix.P_TwinBlast_Ult2_ShouldersLooping_UE58Fix'"));
	if (!Particles[etoi(ULTSHOULDER)])
	{
		PFLOG(Warning, TEXT("UltShoulder Failed"));
	}
}

void APFTwinBlast::SetSound()
{
	// 발사, 피격 사운드 로드
	Sounds.SetNum(etoi(SOUND_END));

	Sounds[etoi(SHOOT)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Explosion_Medium_2-1.Explosion_Medium_2-1'"));
	if (!Sounds[etoi(SHOOT)])
	{
		PFLOG(Warning, TEXT("ShootSound Failed"));
	}
	Sounds[etoi(ULTSHOOT)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Explosion_Large_1-1.Explosion_Large_1-1'"));
	if (!Sounds[etoi(ULTSHOOT)])
	{
		PFLOG(Warning, TEXT("UltShootSound Failed"));
	}
	Sounds[etoi(HIT)] = LoadObject<USoundBase>(nullptr, TEXT("SoundCue'/Game/Free_Sounds_Pack/wav/Hit_Generic_2-1.Hit_Generic_2-1'"));
	if (!Sounds[etoi(HIT)])
	{
		PFLOG(Warning, TEXT("HitSound Failed"));
	}
}

void APFTwinBlast::Jump()
{
	if (IsUltimateActive())
	{
		return;
	}

	Super::Jump();
}

void APFTwinBlast::Attack()
{
	if (IsDeadCharacter() || !IsLocallyControlled())
	{
		return;
	}

	Server_UpdateAimPoint(CalculateAimPoint());
	Super::Attack();
}

TSubclassOf<UGameplayAbility> APFTwinBlast::GetAttackAbilityClass() const
{
	const FGameplayTag UltimateStateTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Ultimate"));
	return ASC && ASC->HasMatchingGameplayTag(UltimateStateTag)
		? UltimateAttackAbilityClass
		: AttackAbilityClass;
}

// 궁극기 상태 전환
void APFTwinBlast::ToggleUltimateState()
{
	SetReplicatedStateTag(PFGameplayTags::Character_State_Ultimate, !IsUltimateActive());
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
	if (!PFAnim || IsDeadCharacter())
	{
		return;
	}
	// 전환 중 행동 차단, 이동 초기화
	SetBlockTags(true);

	GetCharacterMovement()->StopMovementImmediately();
	FinalDir = IDLE;
	UpDownDir = IDLE;
	LeftRightDir = IDLE;
	PFAnim->SetCurrentDir(IDLE);
	if (UltGun) UltGun->SetCurrentDir(IDLE);

	// 캐릭터, 총 전환 연출
	if(IsUltimateActive())
	{
		PFAnim->PlayMontage(etoi(ULTSTART));
		if (UltGun) UltGun->PlayMontage(etoi(ULTSTART));
	}
	else
	{
		StopUltShoulderEffect();
		PFAnim->PlayMontage(etoi(ULTEND));
		if (UltGun) UltGun->PlayMontage(etoi(ULTEND));
	}

	// 궁극기 조준점 반영
	if (IsLocallyControlled() && CrosshairWidget)
	{
		CrosshairWidget->SetUltimateCrosshair(IsUltimateActive());
	}
}

// 일반 공격 몽타주 재생
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Normal_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed || !PFAnim)
	{
		return;
	}

	const bool bGCShootLeft = Parameters.RawMagnitude > 0.5f;
	PFAnim->PlayMontage(bGCShootLeft ? etoi(LEFTATTACK) : etoi(RIGHTATTACK));
}

// 궁극기 공격 몽타주 재생
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Ultimate_Montage(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	(void)Parameters;
	UPFAnimInst_TwinBlast* TwinBlastAnim = Cast<UPFAnimInst_TwinBlast>(PFAnim);
	if (EventType == EGameplayCueEvent::Executed && TwinBlastAnim && !TwinBlastAnim->IsUltimateAttackMontagePlaying())
	{
		TwinBlastAnim->PlayMontage(etoi(ULTATTACK));
	}
}

// 일반 공격 발사 연출
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Normal_Shoot(
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

// 궁극기 발사 연출
void APFTwinBlast::GameplayCue_Character_Attack_Twinblast_Ultimate_Shoot(
	EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType != EGameplayCueEvent::Executed || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bGCShootLeft = Parameters.RawMagnitude > 0.5f;
	const FName SocketName = bGCShootLeft ? FName("Muzzle_04") : FName("Muzzle_03");
	const PARTICLE MuzzleParticle = bGCShootLeft ? ULTMUZZLELEFT : ULTMUZZLERIGHT;
	UGameplayStatics::SpawnEmitterAttached(Particles[etoi(MuzzleParticle)], GetMesh(), SocketName, FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.f), EAttachLocation::SnapToTarget, true);
	UGameplayStatics::PlaySound2D(this, Sounds[etoi(ULTSHOOT)]);
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

	// 궁극기 전환 종료 후 행동, 이펙트 반영
	switch (MontageIdx)
	{
	case etoi(ULTSTART):
	{
		SetBlockTags(false);

		if (IsUltimateActive())
		{
			StartUltShoulderEffect();
		}
	}
		break;
	case etoi(ULTEND):
		StopUltShoulderEffect();
		SetBlockTags(false);
		break;
	default:
		break;
	}

	if (!IsAttackCommandActive() && (MontageIdx == etoi(LEFTATTACK) || MontageIdx == etoi(RIGHTATTACK)))
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
