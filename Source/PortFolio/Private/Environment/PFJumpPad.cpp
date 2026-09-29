#include "Environment/PFJumpPad.h"

#include "Character/PFCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "UObject/ConstructorHelpers.h"

APFJumpPad::APFJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	SetRootComponent(Platform);
	Platform->SetMobility(EComponentMobility::Static);
	Platform->SetCollisionProfileName(TEXT("BlockAll"));
	Platform->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(
		TEXT("/Game/GameData/Meshes/Gameplay/SM_Ruins_JumpPad_3m.SM_Ruins_JumpPad_3m"));
	if (Mesh.Succeeded()) Platform->SetStaticMesh(Mesh.Object);
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Platform);
	Trigger->SetMobility(EComponentMobility::Static);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	Trigger->SetBoxExtent(FVector(130.f, 130.f, 80.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);
}

void APFJumpPad::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_Client)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &APFJumpPad::HandleBeginOverlap);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &APFJumpPad::HandleEndOverlap);
		SetActorTickEnabled(true);
	}
}

void APFJumpPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GetNetMode() == NM_Client) return;
	TArray<AActor*> OverlappingActors;
	Trigger->GetOverlappingActors(OverlappingActors, APFCharacter::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryLaunch(Cast<APFCharacter>(OverlappingActor));
	}
	SetActorTickEnabled(!OverlappingActors.IsEmpty());
}

// 착지점과 중력에 맞춘 발사 속도 계산
void APFJumpPad::TryLaunch(APFCharacter* OtherCharacter)
{
	if (!IsValid(OtherCharacter) || GetNetMode() == NM_Client || !OtherCharacter->HasAuthority()
		|| !OtherCharacter->IsPlayerCharacter() || OtherCharacter->IsDeadCharacter()
		|| OtherCharacter->IsLevelStartActive() || OtherCharacter->IsJumpPadFlightActive()
		|| LaunchedCharacters.Contains(OtherCharacter)) return;
	UCharacterMovementComponent* Movement = OtherCharacter->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround()) return;
	const float Gravity = -Movement->GetGravityZ();
	const APhysicsVolume* Volume = Movement->GetPhysicsVolume();
	const float TerminalSpeed = Volume ? Volume->TerminalVelocity : 4000.f;
	if (Gravity <= KINDA_SMALL_NUMBER || TerminalSpeed <= KINDA_SMALL_NUMBER) return;
	const float Duration = FMath::Min(FMath::Clamp(FlightDuration, 0.8f, 6.f), 1.6f * TerminalSpeed / Gravity);
	if (Duration < 0.5f) return;
	const FVector LandingCenter = GetActorTransform().TransformPosition(LandingOffset)
		+ FVector(0.f, 0.f, OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	FVector LaunchVelocity = (LandingCenter - OtherCharacter->GetActorLocation()) / Duration;
	LaunchVelocity.Z += 0.5f * Gravity * Duration;
	if (LaunchVelocity.Z <= 0.f || Gravity * Duration - LaunchVelocity.Z >= TerminalSpeed * 0.98f) return;
	if (OtherCharacter->BeginJumpPadFlight(LaunchVelocity, Duration))
	{
		LaunchedCharacters.Add(OtherCharacter);
	}
}

// 진입, 착지 후 발사 검사
void APFJumpPad::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APFCharacter* OtherCharacter = Cast<APFCharacter>(OtherActor);
	if (OtherCharacter && OtherComponent == OtherCharacter->GetCapsuleComponent())
	{
		SetActorTickEnabled(true);
		TryLaunch(OtherCharacter);
	}
}

// 점프대를 벗어나면 재사용 허용
void APFJumpPad::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	APFCharacter* OtherCharacter = Cast<APFCharacter>(OtherActor);
	if (OtherCharacter && OtherComponent == OtherCharacter->GetCapsuleComponent())
	{
		LaunchedCharacters.Remove(OtherCharacter);
	}
}
