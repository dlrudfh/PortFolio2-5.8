#include "Character/PFShrubStealthComponent.h"

#include "Character/PFCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GAS/PFGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "System/Framework/PFEnemyAIController.h"

UPFShrubStealthComponent::UPFShrubStealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
	MaterialLibrary = TSoftObjectPtr<UPFShrubMaterialLibrary>(FSoftObjectPath(TEXT("/Game/GameData/Materials/ShrubStealth/DA_ShrubMaterials.DA_ShrubMaterials")));
}

void UPFShrubStealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<APFCharacter>(GetOwner());
	if (!Character.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &UPFShrubStealthComponent::OnBeginOverlap);
	Capsule->OnComponentEndOverlap.AddDynamic(this, &UPFShrubStealthComponent::OnEndOverlap);
	RefreshState();
}

void UPFShrubStealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Character.IsValid())
	{
		Character->GetCapsuleComponent()->OnComponentBeginOverlap.RemoveDynamic(this, &UPFShrubStealthComponent::OnBeginOverlap);
		Character->GetCapsuleComponent()->OnComponentEndOverlap.RemoveDynamic(this, &UPFShrubStealthComponent::OnEndOverlap);
	}
	RestorePresentation();
	Super::EndPlay(EndPlayReason);
}

void UPFShrubStealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshState();
}

void UPFShrubStealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPFShrubStealthComponent, bConcealed);
}

// 겹친 수풀 컴포넌트 확인
bool UPFShrubStealthComponent::IsInsideShrub() const
{
	if (!Character.IsValid()) return false;
	TArray<UPrimitiveComponent*> Overlapping;
	Character->GetCapsuleComponent()->GetOverlappingComponents(Overlapping);
	for (const UPrimitiveComponent* Component : Overlapping)
	{
		if (IsValid(Component) && Component->ComponentHasTag(TEXT("PFShrubConcealment"))
			&& Component->IsQueryCollisionEnabled())
		{
			return true;
		}
	}
	return false;
}

// 은신 중 공격을 주변 봇에 알리고 재은신 차단
void UPFShrubStealthComponent::BreakForAttack()
{
	if (!Character.IsValid() || !Character->HasAuthority()) return;
	if (bConcealed)
	{
		if (UWorld* World = GetWorld())
		{
			for (FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator)
			{
				if (APFEnemyAIController* EnemyController = Cast<APFEnemyAIController>(Iterator->Get()))
				{
					EnemyController->RememberShrubAttacker(Character.Get());
				}
			}
		}
	}
	bRequiresExit = IsInsideShrub();
	SetConcealed(false);
}

// 서버 판정, 소유자 변경에 따른 표시 갱신
void UPFShrubStealthComponent::RefreshState()
{
	if (!Character.IsValid()) return;
	if (Character->HasAuthority())
	{
		const bool bEligible = Character->IsPlayerCharacter() && Character->GetController() && !Character->IsDeadCharacter();
		const bool bInside = bEligible && IsInsideShrub();
		if (!bInside)
		{
			bRequiresExit = false;
		}
		else if (!bRequiresExit && Character->HasStateTag(PFGameplayTags::Character_State_Attacking))
		{
			BreakForAttack();
		}
		SetConcealed(bInside && !bRequiresExit);
	}
	RefreshPresentation();
}

// 은신 상태 복제 예약
void UPFShrubStealthComponent::SetConcealed(bool bNewConcealed)
{
	if (bConcealed == bNewConcealed) return;
	bConcealed = bNewConcealed;
	Character->ForceNetUpdate();
	RefreshPresentation();
}

// 복제된 은신 표시 반영
void UPFShrubStealthComponent::OnRep_Concealed()
{
	RefreshPresentation();
}

// 수풀 진입 판정
void UPFShrubStealthComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherComponent && OtherComponent->ComponentHasTag(TEXT("PFShrubConcealment"))) RefreshState();
}

// 수풀 이탈 판정
void UPFShrubStealthComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (OtherComponent && OtherComponent->ComponentHasTag(TEXT("PFShrubConcealment"))) RefreshState();
}

// 원격 캐릭터 숨김, 소유 캐릭터 반투명 표시
void UPFShrubStealthComponent::RefreshPresentation()
{
	if (!Character.IsValid() || GetNetMode() == NM_DedicatedServer) return;
	const bool bActive = bConcealed && !Character->IsDeadCharacter();
	const bool bLocal = Character->IsLocalPlayerCharacter();
	if (!bActive)
	{
		RestorePresentation();
		return;
	}
	if ((bLocal && !HiddenStates.IsEmpty()) || (!bLocal && !FadedMeshes.IsEmpty())) RestorePresentation();
	TInlineComponentArray<UPrimitiveComponent*> Components(Character.Get());
	TArray<AActor*> AttachedActors;
	Character->GetAttachedActors(AttachedActors, true, true);
	for (AActor* Attached : AttachedActors)
	{
		TInlineComponentArray<UPrimitiveComponent*> AttachedComponents(Attached);
		Components.Append(AttachedComponents);
	}
	for (UPrimitiveComponent* Component : Components)
	{
		if (bLocal)
		{
			if (UMeshComponent* Mesh = Cast<UMeshComponent>(Component); Mesh && !FadedMeshes.Contains(Mesh))
			{
				ApplyLocalFade(Mesh);
				FadedMeshes.Add(Mesh);
			}
		}
		else if (!HiddenStates.Contains(Component))
		{
			HiddenStates.Add(Component, Component->bHiddenInGame);
			Component->SetHiddenInGame(true);
		}
	}
}

// 소유자에게만 은신용 머티리얼 적용
void UPFShrubStealthComponent::ApplyLocalFade(UMeshComponent* Mesh)
{
	UPFShrubMaterialLibrary* Library = MaterialLibrary.LoadSynchronous();
	if (!Library) return;
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* Original = Mesh->GetMaterial(Index);
		UMaterialInterface* Key = Original;
		const TObjectPtr<UMaterialInterface>* Variant = Library->MaterialVariants.Find(Key);
		while (!Variant)
		{
			UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Key);
			if (!Dynamic) break;
			Key = Dynamic->Parent;
			Variant = Library->MaterialVariants.Find(Key);
		}
		if (!Variant || !*Variant) continue;
		UMaterialInstanceDynamic* Faded = UMaterialInstanceDynamic::Create(*Variant, this);
		if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Original)) Faded->CopyParameterOverrides(Instance);
		Faded->SetScalarParameterValue(TEXT("PFShrubOpacity"), 0.5f);
		FPFConcealedMaterialSlot& Slot = MaterialSlots.AddDefaulted_GetRef();
		Slot.Mesh = Mesh;
		Slot.Index = Index;
		Slot.Original = Original;
		Slot.Faded = Faded;
		Mesh->SetMaterial(Index, Faded);
	}
}

// 은신 전 표시 상태 복원
void UPFShrubStealthComponent::RestorePresentation()
{
	for (const auto& Entry : HiddenStates)
	{
		if (UPrimitiveComponent* Component = Entry.Key.Get()) Component->SetHiddenInGame(Entry.Value);
	}
	HiddenStates.Reset();
	for (const FPFConcealedMaterialSlot& Slot : MaterialSlots)
	{
		if (UMeshComponent* Mesh = Slot.Mesh.Get(); Mesh && Mesh->GetMaterial(Slot.Index) == Slot.Faded)
		{
			if (UMaterialInstanceDynamic* Original = Cast<UMaterialInstanceDynamic>(Slot.Original))
			{
				Original->CopyParameterOverrides(Slot.Faded);
			}
			Mesh->SetMaterial(Slot.Index, Slot.Original);
		}
	}
	MaterialSlots.Reset();
	FadedMeshes.Reset();
}
