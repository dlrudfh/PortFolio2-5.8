#include "GAS/PFGameplayTags.h"

namespace PFGameplayTags
{
	// 공격 어빌리티 태그
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Ability_Attack, "Character.Ability.Attack", "모든 기본공격 GameplayAbility");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Ability_Attack_Kwang, "Character.Ability.Attack.Kwang", "Kwang 기본공격 GameplayAbility");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Ability_Attack_Twinblast_NormalAttack, "Character.Ability.Attack.Twinblast.NormalAttack", "TwinBlast 일반 기본공격 GameplayAbility");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Ability_Attack_Twinblast_UltAttack, "Character.Ability.Attack.Twinblast.UltAttack", "TwinBlast 궁극기 사격 GameplayAbility");

	// 캐릭터 상태 태그
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Attacking, "Character.State.Attacking", "활성 기본공격 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Invulnerable, "Character.State.Invulnerable", "LevelStart 재생 중 무적 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Dead, "Character.State.Dead", "사망 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Ultimate, "Character.State.Ultimate", "궁극기 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Jumping, "Character.State.Jumping", "상승 중 공중 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Falling, "Character.State.Falling", "하강 중 공중 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Sprinting, "Character.State.Sprinting", "질주 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Attack_ComboWindow, "Character.State.Attack.ComboWindow", "Kwang 콤보 입력 가능 상태");
	// 아이템 쿨타임 태그
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Cooldown_HP, "Item.Cooldown.HP", "HP 포션 쿨타임 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Cooldown_MP, "Item.Cooldown.MP", "MP 포션 쿨타임 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Cooldown_Shield, "Item.Cooldown.Shield", "실드 아이템 쿨타임 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Cooldown_Coin, "Item.Cooldown.Coin", "Coin 아이템 쿨타임 상태");
	// 공격 차단 태그
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Block_Attack, "Character.Block.Attack", "기본공격 차단 상태");

	// 공격 타이밍 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Event_Attack_ComboWindow, "Character.Event.Attack.ComboWindow", "공격 연계 가능 시점 이벤트");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Event_Attack_ComboReset, "Character.Event.Attack.ComboReset", "공격 콤보 종료 이벤트");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Event_Attack_Shoot, "Character.Event.Attack.Shoot", "TwinBlast 발사 이벤트");

	// 아이템 연출 Cue 태그
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Use_HP, "GameplayCue.Item.Use.HP", "HP 포션 사용 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Use_MP, "GameplayCue.Item.Use.MP", "MP 포션 사용 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Use_Shield, "GameplayCue.Item.Use.Shield", "실드 사용 지속 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Use_Coin, "GameplayCue.Item.Use.Coin", "Coin 획득 연출");
}
