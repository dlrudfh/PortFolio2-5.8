
#include "System/Framework/PFGameMode.h"

#include "Projectile/Bullet.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Subsystems/PFWorldSubsystem.h"
#include "System/Framework/PFPlayerController.h"
#include "Character/PFCharacter.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"

APFGameMode::APFGameMode()
{
	DefaultPawnClass = APFTwinBlast::StaticClass();
	PlayerControllerClass = APFPlayerController::StaticClass();
	PlayerStateClass = APFPlayerState::StaticClass();
}

void APFGameMode::BeginPlay()
{
	Super::BeginPlay();

	auto* Pool = GetWorld()->GetSubsystem<UPFWorldSubsystem>();
	Pool->PreparePool(ABullet::StaticClass(), 10);
}

void APFGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!UsesInitialSpawnFlow(NewPlayer))
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	// 로그인 준비와 캐릭터 선택이 모두 끝난 뒤 최초 생성
	APFPlayerController* PlayerController = Cast<APFPlayerController>(NewPlayer);
	PlayerController->bInitialSpawnReady = true;
	TryStartInitialPlayer(PlayerController);
}

void APFGameMode::RestartPlayer(AController* NewPlayer)
{
	if (UsesInitialSpawnFlow(NewPlayer))
	{
		APFPlayerController* PlayerController = Cast<APFPlayerController>(NewPlayer);
		if (!PlayerController->bInitialSpawnComplete)
		{
			TryStartInitialPlayer(PlayerController);
			return;
		}
	}

	Super::RestartPlayer(NewPlayer);
}

// 최초 생성 대상 확인
bool APFGameMode::UsesInitialSpawnFlow(AController* Controller) const
{
	const UWorld* World = GetWorld();
	return IsValid(Cast<APFPlayerController>(Controller)) && World
		&& !FPackageName::GetShortName(World->GetMapName()).Contains(TEXT("Title"));
}

// 최초 선택 캐릭터 접수
void APFGameMode::RequestInitialSpawn(APFPlayerController* NewPlayer, ECHARACTER SelectedCharacter)
{
	if (!HasAuthority() || !UsesInitialSpawnFlow(NewPlayer)
		|| NewPlayer->bInitialSpawnComplete || NewPlayer->bInitialSpawnInProgress)
	{
		return;
	}
	if (SelectedCharacter != CHARACTER_TWINBLAST && SelectedCharacter != CHARACTER_KWANG)
	{
		return;
	}

	if (!NewPlayer->bInitialSpawnRequested)
	{
		NewPlayer->InitialSpawnCharacter = SelectedCharacter;
		NewPlayer->bInitialSpawnRequested = true;
	}
	TryStartInitialPlayer(NewPlayer);
}

// 선택 캐릭터 최초 생성
void APFGameMode::TryStartInitialPlayer(APFPlayerController* NewPlayer)
{
	if (!HasAuthority() || !IsValid(NewPlayer)
		|| NewPlayer->bInitialSpawnComplete || NewPlayer->bInitialSpawnInProgress)
	{
		return;
	}
	if (IsValid(NewPlayer->GetPawn()))
	{
		NewPlayer->bInitialSpawnComplete = true;
		return;
	}
	if (!NewPlayer->bInitialSpawnReady || !NewPlayer->bInitialSpawnRequested)
	{
		return;
	}

	APFPlayerState* PlayerState = NewPlayer->GetPlayerState<APFPlayerState>();
	if (!IsValid(PlayerState) || bStartPlayersAsSpectators
		|| MustSpectate(NewPlayer) || !PlayerCanRestart(NewPlayer))
	{
		return;
	}

	NewPlayer->bInitialSpawnInProgress = true;
	PlayerState->SetCharacter(NewPlayer->InitialSpawnCharacter);

	// 무작위 위치에 생성하고 실패하면 기본 시작 위치 사용
	FTransform SpawnTransform;
	if (TryFindInitialPlayerSpawnTransform(NewPlayer, SpawnTransform))
	{
		RestartPlayerAtTransform(NewPlayer, SpawnTransform);
	}
	if (!IsValid(NewPlayer->GetPawn()))
	{
		Super::RestartPlayer(NewPlayer);
	}

	NewPlayer->bInitialSpawnComplete = IsValid(NewPlayer->GetPawn());
	NewPlayer->bInitialSpawnInProgress = false;
	if (!NewPlayer->bInitialSpawnComplete)
	{
		PFLOG(Warning, TEXT("Initial player spawn failed: %s"), *GetNameSafe(NewPlayer));
	}
}

// 플레이어 초기 생성 위치 탐색
bool APFGameMode::TryFindInitialPlayerSpawnTransform(APlayerController* NewPlayer, FTransform& OutSpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World || !NewPlayer)
	{
		return false;
	}

	UClass* PlayerPawnClass = GetDefaultPawnClassForController(NewPlayer);
	const APFCharacter* PlayerDefaultObject = PlayerPawnClass ? Cast<APFCharacter>(PlayerPawnClass->GetDefaultObject()) : nullptr;
	const UCapsuleComponent* PlayerCapsule = PlayerDefaultObject ? PlayerDefaultObject->GetCapsuleComponent() : nullptr;
	if (!PlayerCapsule)
	{
		return false;
	}

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InitialPlayerSpawn), false, this);
	int32 RemainingAttempts = 30;
	return FindSpawnTransform(World, PlayerCapsule, QueryParams, RemainingAttempts, true, OutSpawnTransform);
}

// 지면, 충돌을 고려한 생성 위치 탐색
bool APFGameMode::FindSpawnTransform(UWorld* World, const UCapsuleComponent* Capsule,
	const FCollisionQueryParams& QueryParams, int32& RemainingAttempts,
	bool bCheckBlockingCollision, FTransform& OutSpawnTransform)
{
	if (!World || RemainingAttempts <= 0 || (bCheckBlockingCollision && !Capsule))
	{
		return false;
	}

	constexpr float BoundsEdgePadding = 10.f;
	constexpr float VerticalTracePadding = 1000.f;
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;

	// 생성 영역 액터 탐색
	TArray<AActor*> StaticMeshActors;
	UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), StaticMeshActors);
	AStaticMeshActor* SpawnAreaActor = nullptr;
	for (AActor* CandidateActor : StaticMeshActors)
	{
		if (CandidateActor && CandidateActor->GetActorNameOrLabel().Equals(TEXT("SM_Cube"), ESearchCase::CaseSensitive))
		{
			SpawnAreaActor = Cast<AStaticMeshActor>(CandidateActor);
			break;
		}
	}
	if (!SpawnAreaActor)
	{
		return false;
	}

	// 캡슐 크기를 고려한 생성 범위 계산
	const FBox SpawnAreaBounds = SpawnAreaActor->GetComponentsBoundingBox(true);
	const float MinX = SpawnAreaBounds.Min.X + CapsuleRadius + BoundsEdgePadding;
	const float MaxX = SpawnAreaBounds.Max.X - CapsuleRadius - BoundsEdgePadding;
	const float MinY = SpawnAreaBounds.Min.Y + CapsuleRadius + BoundsEdgePadding;
	const float MaxY = SpawnAreaBounds.Max.Y - CapsuleRadius - BoundsEdgePadding;
	if (!SpawnAreaBounds.IsValid || MinX >= MaxX || MinY >= MaxY)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ECollisionChannel WallCollisionChannel;
	if (!GetCollisionChannel(PFCollisionChannelNames::Wall, WallCollisionChannel))
	{
		return false;
	}
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(WallCollisionChannel);

	// 무작위 위치의 지면 탐색
	while (RemainingAttempts > 0)
	{
		--RemainingAttempts;
		const float RandomX = FMath::FRandRange(MinX, MaxX);
		const float RandomY = FMath::FRandRange(MinY, MaxY);
		const FVector TraceStart(RandomX, RandomY, SpawnAreaBounds.Max.Z + VerticalTracePadding);
		const FVector TraceEnd(RandomX, RandomY, SpawnAreaBounds.Min.Z - VerticalTracePadding);

		FHitResult GroundHit;
		if (!World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectQueryParams, QueryParams)
			|| GroundHit.ImpactNormal.Z < 0.5f)
		{
			continue;
		}

		const FVector SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
		// 생성 위치의 캡슐 충돌 확인
		if (bCheckBlockingCollision)
		{
			const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
			const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());
			if (World->OverlapBlockingTestByChannel(SpawnLocation, FQuat::Identity, Capsule->GetCollisionObjectType(),
				CapsuleShape, QueryParams, ResponseParams))
			{
				continue;
			}
		}

		OutSpawnTransform = FTransform(FRotator::ZeroRotator, SpawnLocation);
		return true;
	}

	return false;
}

// 사망 플레이어 부활
void APFGameMode::RespawnPlayer(TWeakObjectPtr<APlayerController> PlayerController)
{
	APlayerController* Controller = PlayerController.Get();
	if (!HasAuthority() || !Controller)
	{
		return;
	}
	APFCharacter* DeadPlayer = Cast<APFCharacter>(Controller->GetPawn());
	APFPlayerState* PlayerState = Controller->GetPlayerState<APFPlayerState>();
	UAbilitySystemComponent* PlayerASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	UPFAttributeSet* PlayerAttributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	if (!DeadPlayer || !DeadPlayer->IsDeadCharacter() || !PlayerASC || !PlayerAttributes)
	{
		return;
	}

	FTransform SpawnTransform;
	const bool bHasSpawnTransform = TryFindInitialPlayerSpawnTransform(Controller, SpawnTransform);
	if (!DeadPlayer->Destroy())
	{
		return;
	}

	// 사망, 질주 해제와 체력, 마나 회복
	PlayerASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Dead, 0, EGameplayTagReplicationState::TagOnly);
	PlayerASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Sprinting, 0, EGameplayTagReplicationState::TagOnly);

	const float MissingHealth = PlayerAttributes->GetMaxHealth() - PlayerAttributes->GetHealth();
	if (MissingHealth > 0.f)
	{
		FPFGE_StatGameplayEffects::ApplyHeal(PlayerASC, MissingHealth);
	}
	const float MissingMana = PlayerAttributes->GetMaxMana() - PlayerAttributes->GetMana();
	if (MissingMana > 0.f)
	{
		FPFGE_StatGameplayEffects::ApplyManaRestore(PlayerASC, MissingMana);
	}

	// 새 플레이어 생성
	if (bHasSpawnTransform)
	{
		RestartPlayerAtTransform(Controller, SpawnTransform);
	}
	if (!Controller->GetPawn())
	{
		RestartPlayer(Controller);
	}

	PlayerState->ForceNetUpdate();
}

// 선택 캐릭터 교체, 상태 복원
void APFGameMode::ChangeCharacter(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	APFPlayerState* PS = Controller->GetPlayerState<APFPlayerState>();
	if (!PS)
	{
		return;
	}

	// 이전 위치, 제어 상태 보관
	FTransform RestartTransform;
	bool bHasRestartTransform = false;
	APFPlayerController* PFController = Cast<APFPlayerController>(Controller);
	if (PFController)
	{
		PFController->CaptureCharacterState();
	}
	const FRotator SavedControlRotation = Controller->GetControlRotation();

	if (APawn* OldPawn = Controller->GetPawn())
	{
		RestartTransform = OldPawn->GetActorTransform();
		bHasRestartTransform = true;

	}

	// 다음 캐릭터 선택
	ECHARACTER CharacterType = PS->GetCharacter();

	switch (CharacterType)
	{
	case CHARACTER_TWINBLAST:
		PS->SetCharacter(CHARACTER_KWANG);
		break;
	case CHARACTER_KWANG:
		PS->SetCharacter(CHARACTER_TWINBLAST);
		break;
	default:
		break;
	}

	if (APawn* OldPawn = Controller->GetPawn())
	{
		OldPawn->Destroy();
	}

	// 선택 캐릭터 생성, 상태 복원
	if (bHasRestartTransform)
	{
		RestartPlayerAtTransform(Controller, RestartTransform);
	}
	else
	{
		RestartPlayer(Controller);
	}

	if (PFController)
	{
		PFController->RestoreCharacterState();
	}

	// 서버, 클라이언트 시선 복원
	Controller->SetControlRotation(SavedControlRotation);
	if (APFPlayerController* PlayerController = Cast<APFPlayerController>(Controller))
	{
		PlayerController->ClientSetRotation(SavedControlRotation, false);
	}
}

UClass* APFGameMode::GetDefaultPawnClassForController_Implementation(AController* Controller)
{
	APFPlayerState* PS = IsValid(Controller) ? Controller->GetPlayerState<APFPlayerState>() : nullptr;
	UClass* PawnClass = nullptr;

	if (IsValid(PS))
	{
		// 선택 캐릭터의 Pawn 클래스 조회
		switch (PS->GetCharacter())
		{
		case CHARACTER_TWINBLAST:
			PawnClass = LoadClass<APawn>(nullptr, TEXT("/Game/GameData/Character/Twinblast.Twinblast_C"));
			break;
		case CHARACTER_KWANG:
			PawnClass = LoadClass<APawn>(nullptr, TEXT("/Game/GameData/Character/Kwang.Kwang_C"));
			break;
		default:
			break;
		}
	}

	return PawnClass ? PawnClass : Super::GetDefaultPawnClassForController_Implementation(Controller);
}
