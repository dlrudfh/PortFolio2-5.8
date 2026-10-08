#include "Campaign/PFGA_CampaignShockwave.h"
#include "Campaign/PFCampaignEnemyController.h"
#include "Character/PFCharacter.h"
#include "AbilitySystemComponent.h"
#include "Animation/PFAnimInst_Kwang.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/PFGameplayTags.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

APFCampaignShockwaveMarker::APFCampaignShockwaveMarker()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Telegraph"));
	RootComponent = Decal;
	Decal->DecalSize = FVector(40.f, 350.f, 350.f);
	FSoftObjectPath Path(TEXT("/Game/GameData/Campaign/Materials/M_Campaign_Telegraph.M_Campaign_Telegraph"));
	Path.PostLoadPath(nullptr);
	TelegraphMaterial = TSoftObjectPtr<UMaterialInterface>(Path);
}

void APFCampaignShockwaveMarker::BeginPlay()
{
	Super::BeginPlay();
	Decal->SetDecalMaterial(TelegraphMaterial.LoadSynchronous());
	if (HasAuthority())
	{
		StartedAt = GetWorld()->GetTimeSeconds();
		SetLifeSpan(2.5f);
	}
}

void APFCampaignShockwaveMarker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const AGameStateBase* State = GetWorld()->GetGameState();
	const double Age = (State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - StartedAt;
	Decal->SetVisibility(Age < 1.8);
	if (!bPlayedStrike && Age >= 1.0 && Age < 1.8 && IsValid(Caster) && !Caster->IsDeadCharacter())
		if (UPFAnimInst_Kwang* Anim = Cast<UPFAnimInst_Kwang>(Caster->GetMesh()->GetAnimInstance()))
		{
			bPlayedStrike = true;
			Anim->PlayMontage(static_cast<int32>(UPFAnimInst_Kwang::MTGIDX_K::ATTACKD));
		}
	Decal->SetRelativeScale3D(FVector(1.f, Age >= 1.5 ? 1.15f : 1.f, Age >= 1.5 ? 1.15f : 1.f));
}

void APFCampaignShockwaveMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPlayedStrike && IsValid(Caster) && !Caster->IsDeadCharacter())
		if (UPFAnimInst_Kwang* Anim = Cast<UPFAnimInst_Kwang>(Caster->GetMesh()->GetAnimInstance()))
		{
			Anim->Montage_Stop(.1f);
			Anim->AttackEnd.Broadcast();
			Anim->ResetAttackCombo();
		}
	Super::EndPlay(EndPlayReason);
}

void APFCampaignShockwaveMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APFCampaignShockwaveMarker, StartedAt);
	DOREPLIFETIME(APFCampaignShockwaveMarker, Caster);
}

UPFGA_CampaignShockwave::UPFGA_CampaignShockwave()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(PFGameplayTags::Character_Block_Attack);
	ActivationBlockedTags.AddTag(PFGameplayTags::Character_State_Dead);
	ActivationBlockedTags.AddTag(PFGameplayTags::Character_State_Attacking);
}

void UPFGA_CampaignShockwave::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	APFCharacter* Character = Cast<APFCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !Character->IsEnemyCharacter()
		|| !Cast<APFCampaignEnemyController>(Character->GetController()) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Caster = Character;
	Character->ClearControlCommands();
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->OnCharacterDied.AddUObject(this, &UPFGA_CampaignShockwave::HandleDeath);
	Center = Character->GetCharacterMovement()->GetActorFeetLocation();
	const FTransform Transform(FRotator(-90.f, 0.f, 0.f), Center + FVector(0.f, 0.f, 5.f));
	Marker = GetWorld()->SpawnActorDeferred<APFCampaignShockwaveMarker>(APFCampaignShockwaveMarker::StaticClass(),
		Transform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Marker.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	Marker->Caster = Character;
	UGameplayStatics::FinishSpawningActor(Marker.Get(), Transform);
	GetWorld()->GetTimerManager().SetTimer(DamageTimer, this, &UPFGA_CampaignShockwave::Detonate, 1.5f);
}

// 동일 높이의 가시 대상에 한 번만 GAS 피해 적용
void UPFGA_CampaignShockwave::Detonate()
{
	APFCharacter* Character = Caster.Get();
	if (!Character || Character->IsDeadCharacter()) { FinishRecovery(); return; }
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APFCharacter* Target = It->Get() ? Cast<APFCharacter>(It->Get()->GetPawn()) : nullptr;
		if (!Target || Target->IsDeadCharacter()) continue;
		const FVector Feet = Target->GetCharacterMovement()->GetActorFeetLocation();
		if (FMath::Abs(Feet.Z - Center.Z) > 100.f || FVector::DistSquared2D(Feet, Center) > FMath::Square(350.f)) continue;
		FHitResult Hit;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(CampaignShockwave), true, Character);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Center + FVector(0.f, 0.f, 60.f), Target->GetActorLocation(), ECC_Visibility, Query)
			|| Hit.GetActor() == Target) Character->ApplyAttackDamageTo(Target, Character->GetDamage() * 2.f, Character, this);
	}
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &UPFGA_CampaignShockwave::FinishRecovery, 1.f);
}

// 범위 공격 회복 종료
void UPFGA_CampaignShockwave::FinishRecovery()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// 지휘관 사망 시 예고 공격 취소
void UPFGA_CampaignShockwave::HandleDeath(APFCharacter* Character)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPFGA_CampaignShockwave::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimer);
		World->GetTimerManager().ClearTimer(RecoveryTimer);
	}
	if (Caster.IsValid()) Caster->OnCharacterDied.RemoveAll(this);
	if (Marker.IsValid()) Marker->Destroy();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
