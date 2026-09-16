#pragma once

#include "PortFolio/PortFolio.h"

#include "Components/BoxComponent.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"

#include "PFGameplayEffectTriggerComponent.generated.h"

class UGameplayEffect;
class UPrimitiveComponent;

// 영역 효과 적용 컴포넌트
UCLASS(ClassGroup = (GAS), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "Gameplay Effect Collision"))
class PORTFOLIO_API UPFGameplayEffectTriggerComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UPFGameplayEffectTriggerComponent();
	bool GrantsJumpBlock() const;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

	virtual void OnAttachmentChanged() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	bool CanProcessEffects() const;

	void ApplyEffectToActor(AActor* TargetActor);

	void RemoveEffectFromActor(AActor* TargetActor);

	UFUNCTION()
	void HandleTargetEndPlay(AActor* TargetActor, EEndPlayReason::Type EndPlayReason);
	void RetryPendingEffects();

	UPrimitiveComponent* FindAutoFitPrimitive() const;

	void AutoFitToOwnerCollision();

private:
	// 영역 진입 시 적용할 효과
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Effect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Effect", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float EffectLevel = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Effect", meta = (AllowPrivateAccess = "true"))
	bool bRemoveEffectOnEndOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	bool bAutoFitOwnerCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AutoFitPadding = 2.f;

	// 대상별 겹친 컴포넌트 수
	TMap<TWeakObjectPtr<AActor>, int32> OverlapCounts;

	// 대상별 적용 효과 핸들
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> AppliedEffectHandles;
	// ASC 준비를 기다리는 대상
	TSet<TWeakObjectPtr<AActor>> PendingEffectActors;
	// 효과 적용 재시도 타이머
	FTimerHandle PendingEffectRetryTimer;
};
