#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "PFShrubStealthComponent.generated.h"

class APFCharacter;
class UPrimitiveComponent;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// 은신 중 사용할 캐릭터 머티리얼 변형
UCLASS()
class PORTFOLIO_API UPFShrubMaterialLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Stealth")
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInterface>> MaterialVariants;
};

USTRUCT()
struct FPFConcealedMaterialSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> Mesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Original;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Faded;
	int32 Index = INDEX_NONE;
};

// 수풀 진입, 공격 해제, 시점별 은신 표시
UCLASS(ClassGroup = Character)
class PORTFOLIO_API UPFShrubStealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPFShrubStealthComponent();
	bool IsConcealed() const { return bConcealed; }
	void BreakForAttack();
	void RefreshState();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Concealed();
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	bool IsInsideShrub() const;
	void SetConcealed(bool bNewConcealed);
	void RefreshPresentation();
	void RestorePresentation();
	void ApplyLocalFade(UMeshComponent* Mesh);

	// 은신 판정 캐릭터
	TWeakObjectPtr<APFCharacter> Character;
	UPROPERTY(ReplicatedUsing = OnRep_Concealed)
	bool bConcealed = false;
	bool bRequiresExit = false;
	// 은신 전 컴포넌트 표시 상태
	TMap<TWeakObjectPtr<UPrimitiveComponent>, bool> HiddenStates;
	TSet<TWeakObjectPtr<UMeshComponent>> FadedMeshes;
	// 은신 종료 시 복원할 원본 머티리얼
	UPROPERTY(Transient)
	TArray<FPFConcealedMaterialSlot> MaterialSlots;
	UPROPERTY(EditDefaultsOnly, Category = "Stealth")
	TSoftObjectPtr<UPFShrubMaterialLibrary> MaterialLibrary;
};
