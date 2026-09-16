#include "GAS/Components/PFGameplayEffectTriggerComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"
#include "System/Subsystems/PFWorldSubsystem.h"

UPFGameplayEffectTriggerComponent::UPFGameplayEffectTriggerComponent()
{
	static ConstructorHelpers::FClassFinder<UGameplayEffect> JumpBlockEffectClass(
		TEXT("/Game/GameData/Abilities/GE_JumpBlock.GE_JumpBlock_C"));
	if (JumpBlockEffectClass.Succeeded())
	{
		EffectClass = JumpBlockEffectClass.Class;
	}

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	InitBoxExtent(FVector(50.f));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ECollisionChannel PFCharacterCollisionChannel;
	if (GetCollisionChannel(PFCollisionChannelNames::PFCharacter, PFCharacterCollisionChannel))
	{
		SetCollisionResponseToChannel(PFCharacterCollisionChannel, ECR_Overlap);
	}
	SetGenerateOverlapEvents(true);

	OnComponentBeginOverlap.AddUniqueDynamic(this, &UPFGameplayEffectTriggerComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddUniqueDynamic(this, &UPFGameplayEffectTriggerComponent::HandleEndOverlap);
}

void UPFGameplayEffectTriggerComponent::OnRegister()
{
	Super::OnRegister();
	// 캐릭터 겹침 응답 설정
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ECollisionChannel PFCharacterCollisionChannel;
	if (GetCollisionChannel(PFCollisionChannelNames::PFCharacter, PFCharacterCollisionChannel))
	{
		SetCollisionResponseToChannel(PFCharacterCollisionChannel, ECR_Overlap);
	}
	AutoFitToOwnerCollision();
	if (HasBegunPlay() && GetWorld())
	{
		if (UPFWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UPFWorldSubsystem>())
		{
			WorldSubsystem->RegisterJumpBlockRegion(this);
		}
	}
}

void UPFGameplayEffectTriggerComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();
	AutoFitToOwnerCollision();
}

void UPFGameplayEffectTriggerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPFWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UPFWorldSubsystem>())
	{
		WorldSubsystem->RegisterJumpBlockRegion(this);
	}
}

void UPFGameplayEffectTriggerComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UPFWorldSubsystem* WorldSubsystem = World->GetSubsystem<UPFWorldSubsystem>())
		{
			WorldSubsystem->UnregisterJumpBlockRegion(this);
		}
	}
	Super::OnUnregister();
}

// 영역 효과의 점프 차단 태그 확인
bool UPFGameplayEffectTriggerComponent::GrantsJumpBlock() const
{
	const UGameplayEffect* Effect = EffectClass ? EffectClass.GetDefaultObject() : nullptr;
	return Effect && Effect->GetGrantedTags().HasTag(FGameplayTag::RequestGameplayTag(TEXT("Character.Block.Jump")));
}

// 영역 크기 기준 컴포넌트 탐색
UPrimitiveComponent* UPFGameplayEffectTriggerComponent::FindAutoFitPrimitive() const
{
	if (UPrimitiveComponent* AttachedPrimitive = Cast<UPrimitiveComponent>(GetAttachParent()))
	{
		if (AttachedPrimitive != this && AttachedPrimitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return AttachedPrimitive;
		}
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	// 소유 액터에서 가장 큰 충돌 컴포넌트 선택
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents(PrimitiveComponents);

	UPrimitiveComponent* LargestPrimitive = nullptr;
	double LargestBoundsVolume = 0.0;
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent) || PrimitiveComponent == this ||
			PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		const FVector WorldExtent = PrimitiveComponent->Bounds.BoxExtent.GetAbs();
		const double BoundsVolume = static_cast<double>(WorldExtent.X) * WorldExtent.Y * WorldExtent.Z;
		if (BoundsVolume > LargestBoundsVolume)
		{
			LargestBoundsVolume = BoundsVolume;
			LargestPrimitive = PrimitiveComponent;
		}
	}

	return LargestPrimitive;
}

// 소유 액터 충돌에 영역 맞춤
void UPFGameplayEffectTriggerComponent::AutoFitToOwnerCollision()
{
	if (!bAutoFitOwnerCollision)
	{
		return;
	}

	UPrimitiveComponent* SourcePrimitive = FindAutoFitPrimitive();
	if (!IsValid(SourcePrimitive))
	{
		return;
	}

	// 기준 경계를 부착 부모 좌표로 변환
	const FBoxSphereBounds SourceLocalBounds = SourcePrimitive->GetLocalBounds();
	const FVector SourceExtent = SourceLocalBounds.BoxExtent.GetAbs();
	const FBox SourceBox(SourceLocalBounds.Origin - SourceExtent, SourceLocalBounds.Origin + SourceExtent);

	FBox FittedBox;
	if (const USceneComponent* ParentComponent = GetAttachParent())
	{
		const FTransform SourceToParent = SourcePrimitive->GetComponentTransform().GetRelativeTransform(
			ParentComponent->GetComponentTransform());
		FittedBox = SourceBox.TransformBy(SourceToParent);
		SetRelativeTransform(FTransform(FRotator::ZeroRotator, FittedBox.GetCenter(), FVector::OneVector));
	}
	else
	{
		FittedBox = SourceBox.TransformBy(SourcePrimitive->GetComponentTransform());
		SetWorldTransform(FTransform(FRotator::ZeroRotator, FittedBox.GetCenter(), FVector::OneVector));
	}

	// 최소 크기, 여백 적용
	FVector FittedExtent = FittedBox.GetExtent().GetAbs();
	FittedExtent.X = FMath::Max(FittedExtent.X, 1.f);
	FittedExtent.Y = FMath::Max(FittedExtent.Y, 1.f);
	FittedExtent.Z = FMath::Max(FittedExtent.Z, 1.f);
	FittedExtent += FVector(FMath::Max(AutoFitPadding, 0.f));
	SetBoxExtent(FittedExtent, true);
}

void UPFGameplayEffectTriggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 재시도, 대상 종료 구독 해제
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingEffectRetryTimer);
	}

	for (const TPair<TWeakObjectPtr<AActor>, int32>& Pair : OverlapCounts)
	{
		if (AActor* TargetActor = Pair.Key.Get())
		{
			TargetActor->OnEndPlay.RemoveDynamic(this, &UPFGameplayEffectTriggerComponent::HandleTargetEndPlay);
		}
	}
	OverlapCounts.Empty();
	PendingEffectActors.Empty();

	// 영역이 부여한 지속 효과 정리
	const TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> EffectHandlesToRemove = MoveTemp(AppliedEffectHandles);
	if (CanProcessEffects() && bRemoveEffectOnEndOverlap)
	{
		for (const TPair<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : EffectHandlesToRemove)
		{
			if (UAbilitySystemComponent* AbilitySystem = Pair.Value.GetOwningAbilitySystemComponent())
			{
				AbilitySystem->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

// 첫 진입 시 효과 적용
void UPFGameplayEffectTriggerComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!CanProcessEffects() || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	const TWeakObjectPtr<AActor> TargetKey(OtherActor);
	int32& OverlapCount = OverlapCounts.FindOrAdd(TargetKey);
	++OverlapCount;

	if (OverlapCount == 1)
	{
		OtherActor->OnEndPlay.AddUniqueDynamic(this, &UPFGameplayEffectTriggerComponent::HandleTargetEndPlay);
		ApplyEffectToActor(OtherActor);
	}
}

// 완전 이탈 시 효과 정리
void UPFGameplayEffectTriggerComponent::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (!CanProcessEffects() || !OtherActor || OtherActor == GetOwner())
	{
		return;
	}

	const TWeakObjectPtr<AActor> TargetKey(OtherActor);
	int32* OverlapCount = OverlapCounts.Find(TargetKey);
	if (!OverlapCount)
	{
		return;
	}

	--(*OverlapCount);
	if (*OverlapCount > 0)
	{
		return;
	}

	OverlapCounts.Remove(TargetKey);
	RemoveEffectFromActor(OtherActor);
}

// 서버 효과 처리 여부 확인
bool UPFGameplayEffectTriggerComponent::CanProcessEffects() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

// 대상에게 영역 효과 적용
void UPFGameplayEffectTriggerComponent::ApplyEffectToActor(AActor* TargetActor)
{
	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	if (!IsValid(TargetActor) || !OverlapCounts.Contains(TargetKey) || !EffectClass
		|| AppliedEffectHandles.Contains(TargetKey))
	{
		PendingEffectActors.Remove(TargetKey);
		return;
	}

	UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	const UGameplayEffect* GameplayEffect = EffectClass.GetDefaultObject();
	if (!GameplayEffect)
	{
		PendingEffectActors.Remove(TargetKey);
		return;
	}
	// ASC가 준비될 때까지 적용 대기
	if (!IsValid(AbilitySystem))
	{
		PendingEffectActors.Add(TargetKey);
		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(PendingEffectRetryTimer))
			{
				World->GetTimerManager().SetTimer(PendingEffectRetryTimer, this,
					&UPFGameplayEffectTriggerComponent::RetryPendingEffects, 0.1f, true);
			}
		}
		return;
	}

	PendingEffectActors.Remove(TargetKey);
	// 영역을 출처로 효과 적용
	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(GetOwner());

	const FActiveGameplayEffectHandle EffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(
		GameplayEffect, EffectLevel, EffectContext);
	// 적용 중 이탈 여부 확인, 효과 핸들 보관
	if (EffectHandle.IsValid())
	{
		if (IsValid(TargetActor) && OverlapCounts.Contains(TargetKey) && IsOverlappingActor(TargetActor))
		{
			AppliedEffectHandles.Add(TargetKey, EffectHandle);
		}
		else if (bRemoveEffectOnEndOverlap)
		{
			if (UAbilitySystemComponent* AppliedAbilitySystem = EffectHandle.GetOwningAbilitySystemComponent())
			{
				AppliedAbilitySystem->RemoveActiveGameplayEffect(EffectHandle);
			}
		}
	}
}

// 대상 효과, 대기 상태 정리
void UPFGameplayEffectTriggerComponent::RemoveEffectFromActor(AActor* TargetActor)
{
	// 대기 대상, 종료 구독 해제
	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	PendingEffectActors.Remove(TargetKey);
	if (PendingEffectActors.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PendingEffectRetryTimer);
		}
	}
	if (IsValid(TargetActor))
	{
		TargetActor->OnEndPlay.RemoveDynamic(this, &UPFGameplayEffectTriggerComponent::HandleTargetEndPlay);
	}

	// 영역이 부여한 효과 제거
	FActiveGameplayEffectHandle EffectHandle;
	if (!AppliedEffectHandles.RemoveAndCopyValue(TargetKey, EffectHandle) || !bRemoveEffectOnEndOverlap)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = EffectHandle.GetOwningAbilitySystemComponent())
	{
		AbilitySystem->RemoveActiveGameplayEffect(EffectHandle);
	}
}

// 제거된 대상의 영역 기록 정리
void UPFGameplayEffectTriggerComponent::HandleTargetEndPlay(AActor* TargetActor, EEndPlayReason::Type EndPlayReason)
{
	OverlapCounts.Remove(TWeakObjectPtr<AActor>(TargetActor));
	RemoveEffectFromActor(TargetActor);
}

// ASC 준비 후 효과 적용 재시도
void UPFGameplayEffectTriggerComponent::RetryPendingEffects()
{
	if (!CanProcessEffects())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PendingEffectRetryTimer);
		}
		PendingEffectActors.Empty();
		return;
	}

	// 아직 겹친 대상만 재시도
	const TArray<TWeakObjectPtr<AActor>> PendingActors = PendingEffectActors.Array();
	for (const TWeakObjectPtr<AActor>& TargetKey : PendingActors)
	{
		AActor* TargetActor = TargetKey.Get();
		if (!IsValid(TargetActor))
		{
			PendingEffectActors.Remove(TargetKey);
			OverlapCounts.Remove(TargetKey);
			continue;
		}
		if (!OverlapCounts.Contains(TargetKey) || !IsOverlappingActor(TargetActor))
		{
			OverlapCounts.Remove(TargetKey);
			RemoveEffectFromActor(TargetActor);
			continue;
		}

		ApplyEffectToActor(TargetActor);
	}

	if (PendingEffectActors.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PendingEffectRetryTimer);
		}
	}
}
