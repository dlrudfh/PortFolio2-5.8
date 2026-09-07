#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Engine/AssetManager.h"

void UPFGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadStaticData();
}

// 공용 에셋 로드
void UPFGameInstanceSubsystem::LoadStaticData()
{
    LoadStaticMeshes();
    LoadStaticNiagaras();
}

// 공용 메시 로드
void UPFGameInstanceSubsystem::LoadStaticMeshes()
{
    StaticMeshes.SetNum(etoi(MESH_END));
    StaticMeshes[etoi(MESH_CHESTCLOSED)] = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Assets/AncientTreasures/Meshes/SM_Chest_01a.SM_Chest_01a"));
    StaticMeshes[etoi(MESH_CHESTOPENED)] = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Assets/AncientTreasures/Meshes/SM_Chest_01b.SM_Chest_01b"));
    StaticMeshes[etoi(MESH_CHESTEMPTY)] = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Assets/AncientTreasures/Meshes/SM_Chest_01c.SM_Chest_01c"));
    StaticMeshes[etoi(MESH_BULLET)] = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ParagonTwinblast/FX/Meshes/Shells/SM_Twinblast_SimpleUltBullet.SM_Twinblast_SimpleUltBullet"));
}

// 아이템 표시 이펙트 로드
void UPFGameInstanceSubsystem::LoadStaticNiagaras()
{
    StaticNiagaras.SetNum(etoi(NIAGARA_END));

    StaticNiagaras[etoi(NIAGARA_ITEM_HPPOTION)] = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_3.NS_Pickup_3"));
    StaticNiagaras[etoi(NIAGARA_ITEM_MPPOTION)] = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_2.NS_Pickup_2"));
    StaticNiagaras[etoi(NIAGARA_ITEM_SHIELD)] = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_4.NS_Pickup_4"));
    StaticNiagaras[etoi(NIAGARA_ITEM_COIN)] = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_1.NS_Pickup_1"));
}

// 메시 번호로 에셋 조회
UStaticMesh* UPFGameInstanceSubsystem::GetStaticMesh(EMESHID MeshID)
{
    int Index = etoi(MeshID);

    if (!StaticMeshes.IsValidIndex(Index))
    {
        PFLOG(Warning, TEXT("Invalid Index : %d"), Index);
        return nullptr;
    }

    UStaticMesh* Mesh = StaticMeshes[Index];

    if (!IsValid(Mesh))
    {
        PFLOG(Warning, TEXT("Mesh doesn't exist : %d"), Index);
        return nullptr;
    }

    return Mesh;
}

// 이펙트 번호로 에셋 조회
UNiagaraSystem* UPFGameInstanceSubsystem::GetStaticNiagara(ENIAGARAID NiagaraID)
{
    int Index = etoi(NiagaraID);

    if (!StaticNiagaras.IsValidIndex(Index))
    {
        PFLOG(Warning, TEXT("Invalid Index : %d"), Index);
        return nullptr;
    }

    UNiagaraSystem* Niagara = StaticNiagaras[Index];

    if (!IsValid(Niagara))
    {
        PFLOG(Warning, TEXT("Niagara doesn't exist : %d"), Index);
        return nullptr;
    }

    return Niagara;
}
