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
#include "Navigation/CrowdManager.h"
#include "NavigationSystem.h"
#include "System/Navigation/PFNavLinkProxy.h"
#include "System/Navigation/PFPhysicsNavObstacleComponent.h"
#include "UObject/UnrealType.h"

APFNavigationSetup::APFNavigationSetup()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	BotClass = APFCharacter::StaticClass();
}

// 물리 장애물 제외, 봇 탄도에서 자동 링크 생성 설정 산출
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
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		NavMeshes.Add(*It);
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
	float LargestObstacleRadius = Radius;
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
			TInlineComponentArray<UPFPhysicsNavObstacleComponent*> Obstacles(*It);
			UPFPhysicsNavObstacleComponent* Obstacle = nullptr;
			for (UPFPhysicsNavObstacleComponent* Existing : Obstacles)
			{
				if (Existing->GetObstacleMesh() == Mesh)
				{
					Obstacle = Existing;
					break;
				}
			}
			if (!Obstacle)
			{
				Obstacle = NewObject<UPFPhysicsNavObstacleComponent>(*It, NAME_None, RF_Transactional);
				It->AddInstanceComponent(Obstacle);
				Obstacle->SetObstacleMesh(Mesh);
				Obstacle->RegisterComponent();
			}
			// 회전 후에도 메시 전체를 담을 수 있는 반경
			LargestObstacleRadius = FMath::Max(LargestObstacleRadius, static_cast<float>(Mesh->Bounds.SphereRadius));
			It->MarkPackageDirty();
			++ObstacleCount;
		}
	}

	const float Apex = FMath::Square(JumpSpeed) / (2.f * Gravity);
	const float MaxDrop = NavigationBounds.GetSize().Z;
	const float LengthStride = FMath::Max(20.f, Radius);
	FVector MaxDropVelocity;
	float MaxFlightTime;
	UPFNavLinkProxy::CalculateJump(FVector::ZeroVector, FVector(0, 0, -MaxDrop),
		WalkSpeed, JumpSpeed, Gravity, MaxDropVelocity, MaxFlightTime);
	const float MaxLength = WalkSpeed * MaxFlightTime;
	const int32 LengthBands = FMath::Max(1, FMath::CeilToInt(MaxLength / LengthStride));
	TArray<FNavLinkGenerationJumpConfig> Configs;
	Configs.Reserve(LengthBands);
	for (int32 LengthBand = 1; LengthBand <= LengthBands; ++LengthBand)
	{
		FNavLinkGenerationJumpConfig& Config = Configs.AddDefaulted_GetRef();
		Config.Name = FName(*FString::Printf(TEXT("PF_Jump_%d"), LengthBand));
		Config.JumpLength = MaxLength * LengthBand / LengthBands;
		Config.JumpDistanceFromEdge = 0.f;
		const float MinimumFlightTime = Config.JumpLength / WalkSpeed;
		const float ReferenceEndHeight = JumpSpeed * MinimumFlightTime
			- 0.5f * Gravity * FMath::Square(MinimumFlightTime);
		Config.JumpMaxDepth = FMath::Max(-ReferenceEndHeight, -Apex + 2.f);
		Config.JumpHeight = Apex;
		Config.JumpEndsHeightTolerance = FMath::Max(Apex - ReferenceEndHeight,
			MaxDrop + ReferenceEndHeight) + Movement->MaxStepHeight;
		Config.SamplingSeparationFactor = 1.f;
		Config.FilterDistanceThreshold = Radius;
		Config.LinkBuilderFlags = static_cast<uint16>(ENavLinkBuilderFlags::CreateCenterPointLink);
		Config.DownDirectionAreaClass = UPFNavArea_Jump::StaticClass();
		Config.UpDirectionAreaClass = nullptr;
		Config.LinkProxyClass = UPFNavLinkProxy::StaticClass();
	}

	for (ARecastNavMesh* NavMesh : NavMeshes)
	{
		NavMesh->Modify();
		RuntimeProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			RuntimeProperty->ContainerPtrToValuePtr<void>(NavMesh), static_cast<int64>(ERuntimeGenerationType::Static));
		auto& JumpConfigs = *JumpConfigsProperty->ContainerPtrToValuePtr<TArray<FNavLinkGenerationJumpConfig>>(NavMesh);
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
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
		NavMesh->bGenerateNavLinks = true;
		NavMesh->bAllowNavLinkAsPathEnd = false;
		JumpConfigs = Configs;
		NavMesh->MarkPackageDirty();
	}

	UCrowdManager* CrowdDefaults = GetMutableDefault<UCrowdManager>();
	bool bCrowdConfigSaved = false;
	if (FFloatProperty* RadiusProperty = FindFProperty<FFloatProperty>(UCrowdManager::StaticClass(), TEXT("MaxAgentRadius")))
	{
		const float RequiredRadius = FMath::Max(RadiusProperty->GetPropertyValue_InContainer(CrowdDefaults), LargestObstacleRadius + 10.f);
		RadiusProperty->SetPropertyValue_InContainer(CrowdDefaults, RequiredRadius);
		bCrowdConfigSaved = CrowdDefaults->TryUpdateDefaultConfigFile();
	}
	PreparationResult = FString::Printf(TEXT("Ready: %d physics meshes, %d jump profiles. Run Build Paths and save the map."),
		ObstacleCount, Configs.Num());
	if (!bCrowdConfigSaved)
	{
		PreparationResult += TEXT(" Warning: Crowd config could not be saved; check DefaultEngine.ini write access.");
	}
	MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("%s"), *PreparationResult);
#endif
}
