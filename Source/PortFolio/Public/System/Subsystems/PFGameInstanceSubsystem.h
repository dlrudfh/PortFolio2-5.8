#pragma once

#include "PortFolio/PortFolio.h"

#include "Engine/Engine.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "PFGameInstanceSubsystem.generated.h"

struct FStreamableHandle;

// 캐릭터 에셋, 표시 정보
struct FPFCharacterDefinition
{
    const TCHAR* Name;
    const TCHAR* MeshPath;
    const TCHAR* AnimationPath;
    const TCHAR* PawnPath;
};

// 공용 에셋 관리 클래스
UCLASS(meta=(PrioritizeCategories="Assets"))
class PORTFOLIO_API UPFGameInstanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static const FPFCharacterDefinition* GetCharacterDefinition(ECHARACTER Character);
    static void ApplyCharacterMesh(class USkeletalMeshComponent* Mesh, ECHARACTER Character, bool bRequired);
    template <typename AssetType>
    static void LoadAssets(TArray<AssetType*>& Assets, TConstArrayView<const TCHAR*> Paths)
    {
        Assets.SetNum(Paths.Num());
        for (int32 Index = 0; Index < Paths.Num(); ++Index)
        {
            Assets[Index] = LoadObject<AssetType>(nullptr, Paths[Index]);
            if (!Assets[Index])
            {
                PFLOG(Warning, TEXT("Asset load failed: %s"), Paths[Index]);
            }
        }
    }

    UStaticMesh* GetStaticMesh(EMESHID MeshID);
    UNiagaraSystem* GetStaticNiagara(ENIAGARAID NiagaraID);

private:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void LoadStaticMeshes();
    void LoadStaticNiagaras();

private:
    // 은신 머티리얼 사전 로드
    TSharedPtr<FStreamableHandle> ShrubMaterialLoad;

    // 공용 메시 목록
    UPROPERTY(EditDefaultsOnly, Category = "Assets")
    TArray<UStaticMesh*> StaticMeshes;

    // 아이템 표시 이펙트 목록
    UPROPERTY(EditDefaultsOnly, Category = "Assets")
    TArray<UNiagaraSystem*> StaticNiagaras;
};
    
