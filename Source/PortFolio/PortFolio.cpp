// Copyright Epic Games, Inc. All Rights Reserved.

#include "PortFolio.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "UObject/ICookInfo.h"
#endif

DEFINE_LOG_CATEGORY(PortFolio);

// 게임 모듈, 런타임 에셋 패키징
class FPortFolioModule : public FDefaultGameModuleImpl
{
#if WITH_EDITOR
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		ModifyCookHandle = UE::Cook::FDelegates::ModifyCook.AddStatic(&AddRuntimeCookPackages);
	}

	virtual void ShutdownModule() override
	{
		UE::Cook::FDelegates::ModifyCook.Remove(ModifyCookHandle);
		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
	// 문자열 경로로 로드하는 에셋의 패키징 포함
	static void AddRuntimeCookPackages(UE::Cook::ICookInfo&, TArray<UE::Cook::FPackageCookRule>& CookRules)
	{
		static const TCHAR* const RuntimePackages[] =
		{
			TEXT("/Game/Assets/AncientTreasures/Meshes/SM_Chest_01a"),
			TEXT("/Game/Assets/AncientTreasures/Meshes/SM_Chest_01b"),
			TEXT("/Game/Assets/AncientTreasures/Meshes/SM_Chest_01c"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_CoinBurst"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Energy_2"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Healing_1"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_1"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_2"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_3"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Pickup_4"),
			TEXT("/Game/Assets/sA_PickupSet_1/Fx/NiagaraSystems/NS_Shield_2"),
			TEXT("/Game/GameData/Abilities/CharacterJump"),
			TEXT("/Game/GameData/Abilities/GE_JumpBlock"),
			TEXT("/Game/GameData/Character/Kwang"),
			TEXT("/Game/GameData/Character/Twinblast"),
			TEXT("/Game/GameData/Images/Items/Coin"),
			TEXT("/Game/GameData/Images/Items/Hp"),
			TEXT("/Game/GameData/Images/Items/Mp"),
			TEXT("/Game/GameData/Images/Items/Shield"),
			TEXT("/Game/GameData/Materials/ShrubStealth/DA_ShrubMaterials"),
			TEXT("/Game/GameData/Meshes/Gameplay/SM_Ruins_JumpPad_3m"),
			TEXT("/Game/GameData/PFCharacterData"),
			TEXT("/Game/GameData/PFItemData"),
			TEXT("/Game/GameData/UI/Crosshair"),
			TEXT("/Game/GameData/UI/Inventory"),
			TEXT("/Game/GameData/UI/Menu"),
			TEXT("/Game/GameData/UI/Minimap/DA_MinimapAtlas"),
			TEXT("/Game/GameData/UI/OtherUI"),
			TEXT("/Game/GameData/UI/Respawn"),
			TEXT("/Game/GameData/UI/SelfUI"),
			TEXT("/Game/GameData/UI/Stats"),
			TEXT("/Game/GameData/UI/Title"),
			TEXT("/Game/ParagonTwinblast/FX/Meshes/Shells/SM_Twinblast_SimpleUltBullet")
		};

		for (const TCHAR* PackageName : RuntimePackages)
		{
			UE::Cook::FPackageCookRule& Rule = CookRules.AddDefaulted_GetRef();
			Rule.PackageName = FName(PackageName);
			Rule.InstigatorName = TEXT("PortFolio.RuntimeAssets");
			Rule.CookRule = UE::Cook::EPackageCookRule::AddToCook;
		}
	}

	// 쿠킹 에셋 등록 핸들
	FDelegateHandle ModifyCookHandle;
#endif
};

// 게임 모듈 등록
IMPLEMENT_PRIMARY_GAME_MODULE( FPortFolioModule, PortFolio, "PortFolio" );
