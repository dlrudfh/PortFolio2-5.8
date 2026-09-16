#include "Props/PFChest.h"
#include "Character/PFCharacter.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"

using enum APFItem::EITEM;

APFChest::APFChest() : CurrentState(etoi(CHEST_CLOSED)), Radius(200.f), Reward(etoi(ITEM_HPPOTION))
{
	PrimaryActorTick.bCanEverTick = false;
	
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("TRIGGER"));
	Chest = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CHEST"));
	RootComponent = Trigger;
	Chest->SetupAttachment(RootComponent);

	Trigger->SetCollisionProfileName(TEXT("Chest"));
	Trigger->SetSphereRadius(Radius);

	bReplicates = true;
	Chest->SetIsReplicated(true);
}

// 상자 열기 요청
void APFChest::ChestOpen()
{
	Server_ChestOpen();
}

// 서버 상자 개방, 보상 생성
void APFChest::Server_ChestOpen_Implementation()
{
	if (CurrentState != etoi(CHEST_CLOSED))
	{
		return;
	}

	CurrentState = etoi(CHEST_OPENED);
	OnRep_CurrentState();

	// 보상 데이터 설정 후 아이템 생성
	FVector SpawnLocation = GetActorLocation() + FVector::UpVector * 10.f;
	FRotator SpawnRotation = GetActorRotation();

	APFItem* NewItem = GetWorld()->SpawnActorDeferred<APFItem>(
		APFItem::StaticClass(), FTransform(SpawnRotation, SpawnLocation));
	if (NewItem)
	{
		NewItem->SetItemData(Reward);
		NewItem->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));
	}

	// 빈 상자 전환, 제거 예약
	GetWorldTimerManager().SetTimer(EmptyStateTimerHandle, this, &APFChest::HandleChestEmpty, 3.f, false);
	GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &APFChest::HandleChestDestroy, 6.f, false);
}

// 상자 상태에 맞춰 메시, 충돌 갱신
void APFChest::OnRep_CurrentState()
{
	switch (CurrentState)
	{
	case etoi(CHEST_CLOSED):
		Chest->SetStaticMesh(GETMESH(MESH_CHESTCLOSED));
		break;
	case etoi(CHEST_OPENED):
		Chest->SetStaticMesh(GETMESH(MESH_CHESTOPENED));
		break;
	case etoi(CHEST_EMPTY):
		Chest->SetStaticMesh(GETMESH(MESH_CHESTEMPTY));
		break;
	default:
		break;
	}

	if (CurrentState == etoi(CHEST_OPENED) || CurrentState == etoi(CHEST_EMPTY))
	{
		Chest->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void APFChest::BeginPlay()
{
	Super::BeginPlay();
	OnRep_CurrentState();
}

void APFChest::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// 상호작용 범위 진입, 이탈 이벤트 연결
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APFChest::OnCharacterBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &APFChest::OnCharacterEndOverlap);

}

// 빈 상자 상태로 전환
void APFChest::HandleChestEmpty()
{
	CurrentState = etoi(CHEST_EMPTY);
	OnRep_CurrentState();
}

// 상자 제거
void APFChest::HandleChestDestroy()
{
	Destroy();
}

// 플레이어 상호작용 대상으로 등록
void APFChest::OnCharacterBeginOverlap(UPrimitiveComponent* OverlappedCom, AActor* OtherActor, UPrimitiveComponent* OtherCom, int32 OteryBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APFCharacter* Character = Cast<APFCharacter>(OtherActor);
	APFPlayerController* PlayerController = Character ? Cast<APFPlayerController>(Character->GetController()) : nullptr;
	if (Character && Character->IsPlayerCharacter() && PlayerController)
	{
		PlayerController->SetChest(this);
	}
}

// 플레이어 상호작용 대상 해제
void APFChest::OnCharacterEndOverlap(UPrimitiveComponent* OverlappedCom, AActor* OtherActor, UPrimitiveComponent* OtherCom, int32 OteryBodyIndex)
{
	APFCharacter* Character = Cast<APFCharacter>(OtherActor);
	APFPlayerController* PlayerController = Character ? Cast<APFPlayerController>(Character->GetController()) : nullptr;
	if (Character && Character->IsPlayerCharacter() && PlayerController)
	{
		PlayerController->SetChest();
	}
}

void APFChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APFChest, CurrentState);
}
