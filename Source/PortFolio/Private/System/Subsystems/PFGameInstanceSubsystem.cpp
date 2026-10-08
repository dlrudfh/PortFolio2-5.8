#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

namespace
{
    // 공용 에셋 인덱스, 객체 유효성 확인
    template <typename AssetType>
    AssetType* FindAsset(const TArray<AssetType*>& Assets, int32 Index)
    {
        if (!Assets.IsValidIndex(Index))
        {
            PFLOG(Warning, TEXT("Invalid asset index: %d"), Index);
            return nullptr;
        }
        if (!IsValid(Assets[Index]))
        {
            PFLOG(Warning, TEXT("Asset does not exist: %d"), Index);
            return nullptr;
        }
        return Assets[Index];
    }
}

void UPFGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!IsRunningDedicatedServer())
        ShrubMaterialLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(
            FSoftObjectPath(TEXT("/Game/GameData/Materials/ShrubStealth/DA_ShrubMaterials.DA_ShrubMaterials")));
    LoadStaticMeshes();
    LoadStaticNiagaras();
}

void UPFGameInstanceSubsystem::Deinitialize()
{
    if (ShrubMaterialLoad.IsValid()) ShrubMaterialLoad->CancelHandle();
    ShrubMaterialLoad.Reset();
    Super::Deinitialize();
}

// 캐릭터 공통 정의 조회
const FPFCharacterDefinition* UPFGameInstanceSubsystem::GetCharacterDefinition(ECHARACTER Character)
{
    static const FPFCharacterDefinition Definitions[] =
    {
        {TEXT("Twinblast"),
            TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast.TwinBlast"),
            TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/TwinBlast_Blueprint.TwinBlast_Blueprint_C"),
            TEXT("/Game/GameData/Character/Twinblast.Twinblast_C")},
        {TEXT("Kwang"),
            TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_GDC.Kwang_GDC"),
            TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Kwang_Blueprint.Kwang_Blueprint_C"),
            TEXT("/Game/GameData/Character/Kwang.Kwang_C")}
    };
    static_assert(UE_ARRAY_COUNT(Definitions) == etoi(CHARACTER_END));
    const int32 Index = etoi(Character);
    return Index >= 0 && Index < UE_ARRAY_COUNT(Definitions) ? &Definitions[Index] : nullptr;
}

// 캐릭터 메시, 애니메이션 적용
void UPFGameInstanceSubsystem::ApplyCharacterMesh(USkeletalMeshComponent* Mesh, ECHARACTER Character, bool bRequired)
{
    const FPFCharacterDefinition* Definition = GetCharacterDefinition(Character);
    if (!Definition)
    {
        return;
    }
    USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, Definition->MeshPath);
    UClass* Animation = LoadClass<UAnimInstance>(nullptr, Definition->AnimationPath);
    if (Asset)
    {
        Mesh->SetSkeletalMesh(Asset);
    }
    Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    if (Animation)
    {
        Mesh->SetAnimInstanceClass(Animation);
    }
    if (!Asset || !Animation)
    {
        if (bRequired)
        {
            PFLOG(Fatal, TEXT("Character assets missing: %s"), Definition->Name);
        }
        else
        {
            PFLOG(Warning, TEXT("Character assets missing: %s"), Definition->Name);
        }
    }
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
    return FindAsset(StaticMeshes, etoi(MeshID));
}

// 이펙트 번호로 에셋 조회
UNiagaraSystem* UPFGameInstanceSubsystem::GetStaticNiagara(ENIAGARAID NiagaraID)
{
    return FindAsset(StaticNiagaras, etoi(NiagaraID));
}
