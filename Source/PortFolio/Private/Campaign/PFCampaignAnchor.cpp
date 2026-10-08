#include "Campaign/PFCampaignAnchor.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Engine/StaticMesh.h"

APFCampaignAnchor::APFCampaignAnchor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreRing"));
	Ring->SetupAttachment(RootComponent);
	Ring->SetMobility(EComponentMobility::Movable);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCanEverAffectNavigation(false);
	RingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RingRotation"));
	RingMovement->RotationRate = FRotator(0.f, 25.f, 0.f);
	Status = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StateLabel"));
	Status->SetupAttachment(RootComponent);
	Status->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	Status->SetWorldSize(14.f);
	Status->SetHorizontalAlignment(EHTA_Center);
	UArrowComponent* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Facing"));
	Arrow->SetupAttachment(RootComponent);
}

void APFCampaignAnchor::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
	RingMovement->SetUpdatedComponent(Ring);
	RingMovement->SetComponentTickEnabled(false);
}

void APFCampaignAnchor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UStaticMesh* Mesh = MeshOverride;
	if (!Mesh && Prop != EPFCampaignProp::None)
	{
		const TCHAR* Name = Prop == EPFCampaignProp::Core ? TEXT("SM_Campaign_Core")
			: Prop == EPFCampaignProp::Beacon ? TEXT("SM_Campaign_ReturnBeacon")
			: Prop == EPFCampaignProp::Guide ? TEXT("SM_Campaign_Guide") : TEXT("SM_Campaign_ControlPedestal");
		Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/GameData/Campaign/Meshes/%s.%s"), Name, Name));
	}
	Visual->SetStaticMesh(Mesh);
	Ring->SetStaticMesh(Prop == EPFCampaignProp::Core
		? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/GameData/Campaign/Meshes/SM_Campaign_CoreRing.SM_Campaign_CoreRing")) : nullptr);
	Status->SetVisibility(Prop == EPFCampaignProp::Control || Prop == EPFCampaignProp::Beacon);
}

// 장치 활성 상태 표시
void APFCampaignAnchor::SetActiveVisual(bool bActive)
{
	SetActorHiddenInGame(false);
	if (bVisualInitialized && bActiveVisual == bActive) return;
	bVisualInitialized = true;
	bActiveVisual = bActive;
	RingMovement->SetComponentTickEnabled(Prop == EPFCampaignProp::Core);
	Visual->SetCustomPrimitiveDataFloat(0, bActive ? 1.f : 0.f);
	Status->SetText(FText::FromString(bActive ? TEXT("[G] READY") : TEXT("[-] LOCKED")));
}

APFCampaignShot::APFCampaignShot()
{
	Path = CreateDefaultSubobject<USplineComponent>(TEXT("CameraPath"));
	RootComponent = Path;
}

// 고정 스플라인의 카메라 위치 계산
void APFCampaignShot::Evaluate(float Alpha, FVector& Location, FRotator& Rotation) const
{
	const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f), 2.f);
	const float Distance = Path->GetSplineLength() * Smooth;
	Location = Path->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	const FVector Target = LookAt ? FMath::Lerp(LookAt->GetActorLocation(), EndLookAt ? EndLookAt->GetActorLocation() : LookAt->GetActorLocation(), Smooth) : FVector::ZeroVector;
	Rotation = LookAt ? (Target - Location).Rotation()
		: Path->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}
