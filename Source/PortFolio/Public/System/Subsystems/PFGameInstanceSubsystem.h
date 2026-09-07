#pragma once

#include "PortFolio/PortFolio.h"

#include "Engine/Engine.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/AssetManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "PFGameInstanceSubsystem.generated.h"

// 공용 에셋 관리 클래스
UCLASS(meta=(PrioritizeCategories="Assets"))
class PORTFOLIO_API UPFGameInstanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UStaticMesh* GetStaticMesh(EMESHID MeshID);
    UNiagaraSystem* GetStaticNiagara(ENIAGARAID NiagaraID);

private:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    void LoadStaticData();
    void LoadStaticMeshes();
    void LoadStaticNiagaras();

private:
    // 공용 메시 목록
    UPROPERTY(EditDefaultsOnly, Category = "Assets")
    TArray<UStaticMesh*> StaticMeshes;

    // 아이템 표시 이펙트 목록
    UPROPERTY(EditDefaultsOnly, Category = "Assets")
    TArray<UNiagaraSystem*> StaticNiagaras;
};
    
