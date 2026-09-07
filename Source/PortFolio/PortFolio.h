#pragma once

#include "EngineMinimal.h"
#include "Engine/CollisionProfile.h"
#include "Net/UnrealNetwork.h"

// 프로젝트 로그
DECLARE_LOG_CATEGORY_EXTERN(PortFolio, Log, All);
#define PFLOG_CALLINFO (FString(__FUNCTION__) + TEXT("(") + FString::FromInt(__LINE__) + TEXT(")"))
#define PFLOG_S(val) UE_LOG(PortFolio, val, TEXT("%s"), *PFLOG_CALLINFO)
#define PFLOG_W UE_LOG(PortFolio, Warning, TEXT("%s"), *PFLOG_CALLINFO)
#define PFLOG(val, Format, ...) UE_LOG(PortFolio, val, TEXT("%s %s"), *PFLOG_CALLINFO, *FString::Printf(Format, ##__VA_ARGS__))
#define PFCHECK(Expr, ...) { if(!(Expr)) { PFLOG(Warning, TEXT("ASSERTION : %s"), TEXT("'"#Expr"'")); return __VA_ARGS__; }}

// 프로젝트 충돌 채널
namespace PFCollisionChannelNames
{         
	inline const FName PFCharacter(TEXT("PFCharacter"));
	inline const FName Wall(TEXT("Wall"));
	inline const FName Item(TEXT("Item"));
	inline const FName AimTrace(TEXT("AimTrace"));
	inline const FName AStarTrace(TEXT("AStarTrace"));
}

inline bool GetCollisionChannel(const FName ChannelName, ECollisionChannel& OutCollisionChannel)
{
	OutCollisionChannel = ECC_MAX;
	const UCollisionProfile* CollisionProfile = UCollisionProfile::Get();
	if (!CollisionProfile)
	{
		PFLOG(Error, TEXT("Collision Failed : %s"), *ChannelName.ToString());
		return false;
	}

	for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ECC_MAX); ++ChannelIndex)
	{
		if (CollisionProfile->ReturnChannelNameFromContainerIndex(ChannelIndex) == ChannelName)
		{
			OutCollisionChannel = static_cast<ECollisionChannel>(ChannelIndex);
			return true;
		}
	}

	PFLOG(Error, TEXT("Collision Failed : %s"), *ChannelName.ToString());
	return false;
}

const int RETURN_ERROR = INT_MAX;

template <typename E>
constexpr auto etoi(E e) noexcept {
    return static_cast<int>(e);
}

template <typename E>
constexpr auto eAdd(E e1, E e2) noexcept {
    return static_cast<E>(static_cast<int>(e1) + static_cast<int>(e2));
}

UENUM()
enum class ECHARACTER
{
    CHARACTER_TWINBLAST = 0,
    CHARACTER_KWANG = 1,
    CHARACTER_END
};
using enum ECHARACTER;

UENUM()
enum class EUIZORDER
{
    PLAYERSTAT = 0,
    TITLE
};
using enum EUIZORDER;

UENUM(BlueprintType)
enum class EPFDirection : uint8
{
    IDLE UMETA(DisplayName = "Idle"),
    LEFT UMETA(DisplayName = "Left"),
    RIGHT UMETA(DisplayName = "Right"),
    FWD UMETA(DisplayName = "Fwd"),
    FWDLEFT UMETA(DisplayName = "Fwd Left"),
    FWDRIGHT UMETA(DisplayName = "Fwd Right"),
    BWD UMETA(DisplayName = "Bwd"),
    BWDLEFT UMETA(DisplayName = "Bwd Left"),
    BWDRIGHT UMETA(DisplayName = "Bwd Right"),
    EDIRECTION_END UMETA(DisplayName = "End")
};
using enum EPFDirection;

UENUM()
enum class ECONTROLMODE
{
	TOPVIEW,
    TPS,
    FPS,
    ECONTROLMODE_END
};
using enum ECONTROLMODE;

inline constexpr ECONTROLMODE DefaultControlMode = TPS;

// 화면 비율 계산 기준
static int32 CURRENTSCREENX = 1920;
static int32 CURRENTSCREENY = 1080;
static const int32 DEFAULTSCREENX = 1920;
static const int32 DEFAULTSCREENY = 1080;

#define SCREENRATIO FVector2D((float)CURRENTSCREENX / DEFAULTSCREENX, (float)CURRENTSCREENY / DEFAULTSCREENY)

UENUM()
enum class ENIAGARAID
{
    NIAGARA_ITEM_HPPOTION = 0,
    NIAGARA_ITEM_MPPOTION,
    NIAGARA_ITEM_SHIELD,
    NIAGARA_ITEM_COIN,
    NIAGARA_END
};
using enum ENIAGARAID;

UENUM()
enum class EMESHID
{
    MESH_CHESTCLOSED = 0,
    MESH_CHESTOPENED,
    MESH_CHESTEMPTY,
    MESH_BULLET,
    MESH_END
};
using enum EMESHID;

// 공용 에셋 조회
#define GETMESH(Key) GetGameInstance()->GetSubsystem<UPFGameInstanceSubsystem>()->GetStaticMesh(Key)
#define GETNIAGARA(Key) GetGameInstance()->GetSubsystem<UPFGameInstanceSubsystem>()->GetStaticNiagara(Key)
