#include "System/Navigation/PFNavigationSetup.h"

#include "Character/PFCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "System/Navigation/PFNavLinkProxy.h"
#include "System/Navigation/PFNavigationLink.h"
#include "System/Navigation/PFNavigationLinkBuilder.h"
#include "UObject/UnrealType.h"

APFNavigationSetup::APFNavigationSetup()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	BotClass = APFCharacter::StaticClass();
}

// 기본 NavMesh 빌드, 보행 낙하 우선 이동 링크 생성
void APFNavigationSetup::PrepareStaticNavigation()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	const APFCharacter* Bot = BotClass ? BotClass.GetDefaultObject() : nullptr;
	if (!World || World->IsGameWorld() || !Bot)
	{
		return;
	}
	const UCharacterMovementComponent* Movement = Bot->GetCharacterMovement();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation || Navigation->GetNumRunningBuildTasks() > 0
		|| Navigation->IsNavigationBuildingLocked(static_cast<uint8>(~ENavigationBuildLock::NoUpdateInEditor)))
	{
		PreparationResult = TEXT("Wait until navigation is unlocked and the current build has finished.");
		return;
	}
	const float Radius = Bot->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = Bot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Gravity = FMath::Abs(World->GetWorldSettings()->GetGravityZ() * Movement->GravityScale);
	const float WalkSpeed = Movement->MaxWalkSpeed;
	const float JumpSpeed = Movement->JumpZVelocity;
	if (Gravity <= UE_SMALL_NUMBER || WalkSpeed <= 0.f || JumpSpeed <= 0.f)
	{
		PreparationResult = TEXT("Invalid bot movement settings.");
		return;
	}

	FBox NavigationBounds(ForceInit);
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		NavigationBounds += It->GetComponentsBoundingBox(true);
	}
	TArray<ARecastNavMesh*> NavMeshes;
	FNavAgentProperties AgentProperties = Movement->GetNavAgentPropertiesRef();
	AgentProperties.AgentRadius = Radius;
	AgentProperties.AgentHeight = HalfHeight * 2.f;
	if (ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(Navigation->GetNavDataForProps(AgentProperties)))
	{
		NavMeshes.Add(NavMesh);
	}
	if (!NavigationBounds.IsValid || NavMeshes.IsEmpty())
	{
		PreparationResult = TEXT("Place NavMeshBoundsVolume and create RecastNavMesh first.");
		return;
	}
	FArrayProperty* JumpConfigsProperty = FindFProperty<FArrayProperty>(ARecastNavMesh::StaticClass(), TEXT("NavLinkJumpConfigs"));
	FEnumProperty* RuntimeProperty = FindFProperty<FEnumProperty>(ANavigationData::StaticClass(), TEXT("RuntimeGeneration"));
	if (!JumpConfigsProperty || !RuntimeProperty)
	{
		PreparationResult = TEXT("Navigation properties are unavailable.");
		return;
	}

	Modify();
	int32 ObstacleCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (Cast<ACharacter>(*It))
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!Mesh->BodyInstance.bSimulatePhysics)
			{
				continue;
			}
			It->Modify();
			Mesh->Modify();
			Mesh->SetCanEverAffectNavigation(false);
			It->MarkPackageDirty();
			++ObstacleCount;
		}
	}

	for (ARecastNavMesh* NavMesh : NavMeshes)
	{
		NavMesh->Modify();
		RuntimeProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			RuntimeProperty->ContainerPtrToValuePtr<void>(NavMesh), static_cast<int64>(ERuntimeGenerationType::Dynamic));
		auto& JumpConfigs = *JumpConfigsProperty->ContainerPtrToValuePtr<TArray<FNavLinkGenerationJumpConfig>>(NavMesh);
		for (FNavLinkGenerationJumpConfig& Previous : JumpConfigs)
		{
			if (Navigation && Previous.LinkProxy && Previous.bLinkProxyRegistered)
			{
				Navigation->UnregisterCustomLink(*Previous.LinkProxy);
			}
		}
		FNavDataConfig AgentConfig = NavMesh->GetConfig();
		AgentConfig.AgentRadius = Radius;
		AgentConfig.AgentHeight = HalfHeight * 2.f;
		NavMesh->SetConfig(AgentConfig);
		NavMesh->bGenerateNavLinks = false;
		NavMesh->bAllowNavLinkAsPathEnd = false;
		JumpConfigs.Reset();
		NavMesh->MarkPackageDirty();
	}

	Navigation->Build();
	PreparationResult.Reset();
	for (ARecastNavMesh* NavMesh : NavMeshes)
	{
		FString Result;
		if (!PFNavigationLinkBuilder::Build(*World, *NavMesh, *Bot, GetLevel(), GetActorGuid(), Result))
		{
			PreparationResult = Result;
			UE_LOG(LogTemp, Warning, TEXT("%s"), *PreparationResult);
			return;
		}
		PreparationResult += Result;
	}
	Navigation->Build();
	PreparationResult += FString::Printf(TEXT(" Excluded %d physics meshes."), ObstacleCount);
	MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("%s"), *PreparationResult);
#endif
}

// 현재 월드의 프로젝트 이동 링크 삭제
void APFNavigationSetup::DeleteAllTraversalLinks()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}
	TArray<APFNavigationLink*> Links;
	for (TActorIterator<APFNavigationLink> It(World); It; ++It)
	{
		Links.Add(*It);
	}
	Modify();
	int32 DeletedCount = 0;
	for (APFNavigationLink* Link : Links)
	{
		Link->Modify();
		if (World->EditorDestroyActor(Link, true))
		{
			++DeletedCount;
		}
	}
	PreparationResult = FString::Printf(TEXT("Deleted %d project traversal links. Run Build Paths and save the map."), DeletedCount);
	if (DeletedCount < Links.Num())
	{
		PreparationResult += FString::Printf(TEXT(" %d links could not be deleted."), Links.Num() - DeletedCount);
	}
	MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("%s"), *PreparationResult);
#endif
}