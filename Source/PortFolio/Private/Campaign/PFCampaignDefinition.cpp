#include "Campaign/PFCampaignDefinition.h"

// 챕터 정의 에셋 경로
FString UPFCampaignDefinition::GetAssetPath(int32 Chapter)
{
	return FString::Printf(TEXT("/Game/GameData/Campaign/DA_Campaign_Map%d.DA_Campaign_Map%d"), Chapter, Chapter);
}

// 확정된 캠페인 구성 적용
void UPFCampaignDefinition::SetChapterDefaults()
{
	Steps.Reset();
	Encounters.Reset();
	Supplies.Reset();
	IntroShot = NAME_None;
	IntroRadio = FText::GetEmpty();
	const auto Step = [this](EPFCampaignObjective Type, const TCHAR* Text, const TCHAR* Target,
		const TCHAR* Encounter = TEXT(""), const TCHAR* Checkpoint = TEXT(""), const TCHAR* Shot = TEXT(""), const TCHAR* Radio = TEXT(""))
	{
		FPFCampaignStep& Value = Steps.AddDefaulted_GetRef();
		Value.Type = Type;
		Value.Objective = FText::FromString(Text);
		Value.Target = FName(Target);
		if (*Encounter) Value.RequiredEncounters.Add(FName(Encounter));
		Value.bCheckpoint = *Checkpoint != 0;
		Value.Checkpoint = FName(Checkpoint);
		Value.Shot = FName(Shot);
		Value.Radio = FText::FromString(Radio);
	};
	const auto Encounter = [this](const TCHAR* Id, int32 First, int32 Last, int32 Kwang, int32 Twinblast, bool Commander = false, bool Boss = false)
	{
		FPFCampaignEncounter& Value = Encounters.AddDefaulted_GetRef();
		Value.Id = FName(Id);
		Value.Trigger = FName(FString(Id) + TEXT("_Trigger"));
		Value.FirstStep = First;
		Value.LastStep = Last;
		for (int32 Index = 0; Index < Kwang + Twinblast; ++Index)
		{
			FPFCampaignEnemy& Enemy = Value.Enemies.AddDefaulted_GetRef();
			Enemy.Anchor = FName(FString::Printf(TEXT("%s_Enemy_%02d"), Id, Index + 1));
			Enemy.bKwang = Index < Kwang;
			Enemy.bCommander = Commander && Index == 0;
			Enemy.bFinalBoss = Boss && Index == 0;
		}
		if (Commander)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FPFCampaignEnemy& Enemy = Value.Reinforcements.AddDefaulted_GetRef();
				Enemy.Anchor = FName(FString::Printf(TEXT("%s_Reinforcement_%02d"), Id, Index + 1));
				Enemy.bKwang = Boss || Index == 0;
			}
		}
	};
	const auto Supply = [this](const TCHAR* Id, const TCHAR* Record)
	{
		FPFCampaignSupply& Value = Supplies.AddDefaulted_GetRef();
		Value.Anchor = FName(Id);
		Value.Record = FText::FromString(Record);
	};
	const auto Patrol = [this](const TCHAR* Id)
	{
		for (FPFCampaignEncounter& Group : Encounters)
			if (Group.Id == FName(Id))
				for (FPFCampaignEnemy& Enemy : Group.Enemies)
					for (int32 Index = 1; Index <= 2; ++Index)
						Enemy.Patrol.Add(FName(FString::Printf(TEXT("%s_Patrol_%02d"), Id, Index)));
	};
	using O = EPFCampaignObjective;
	switch (Chapter)
	{
	case 3:
		ChapterTitle = FText::FromString(TEXT("끊어진 연락"));
		IntroShot = TEXT("Intro");
		IntroRadio = FText::FromString(TEXT("선발대와 연락이 끊겼다. 신전에서 반출 경로를 확인해."));
		Step(O::Reach, TEXT("중앙 전투장으로 이동"), TEXT("Central_Trigger"));
		Step(O::Defeat, TEXT("중앙 경비 제압"), TEXT("Central_Trigger"), TEXT("Central"));
		Step(O::Rally, TEXT("중앙 집결 지점에서 G"), TEXT("Central_Checkpoint"), TEXT(""), TEXT("Central"));
		Step(O::Defeat, TEXT("신전 경비 제압"), TEXT("Temple_Trigger"), TEXT("Temple"));
		Step(O::Interact, TEXT("운송 기록 조사"), TEXT("Temple_Control"), TEXT(""), TEXT("Temple"), TEXT(""), TEXT("동력핵 반출 승인. 상부 관측소를 통해 중심 구역으로 이동."));
		Step(O::Rally, TEXT("귀환 장치에 집결하여 관측소로 이동"), TEXT("Exit"));
		Encounter(TEXT("Patrol"), 0, 2, 1, 1);
		Encounter(TEXT("Central"), 0, 2, 2, 1);
		Encounter(TEXT("Temple"), 3, 4, 1, 2);
		Patrol(TEXT("Patrol"));
		Supply(TEXT("West_Supply"), TEXT("선발대 기록: 반출용 장비가 상부 신전으로 이동했다."));
		Supply(TEXT("Lower_Supply"), TEXT("선발대 기록: 유적의 진동이 핵의 빛과 함께 강해지고 있다."));
		break;
	case 4:
		ChapterTitle = FText::FromString(TEXT("회수 명령"));
		Step(O::Defeat, TEXT("중간 광장 확보"), TEXT("Middle_Trigger"), TEXT("Middle"));
		Step(O::Rally, TEXT("중간 광장에 집결"), TEXT("Middle_Checkpoint"), TEXT(""), TEXT("Middle"));
		Step(O::Defeat, TEXT("관측소 경비 제압"), TEXT("Summit_Trigger"), TEXT("Summit"));
		Step(O::Interact, TEXT("관측소 제어 기록 조사"), TEXT("Summit_Control"), TEXT(""), TEXT("Summit"), TEXT("Revelation"), TEXT("중심 동력 분리 시 부유 지반 유지 불가.\n회수 명령을 취소한다. 핵은 그 자리에 둬. 반출 장치부터 정지시켜."));
		Step(O::Rally, TEXT("협곡 진입 지점에 집결"), TEXT("Exit"));
		Encounter(TEXT("Lower"), 0, 1, 0, 2);
		Encounter(TEXT("Middle"), 0, 1, 2, 1);
		Encounter(TEXT("Summit"), 2, 3, 2, 2);
		break;
	case 6:
		ChapterTitle = FText::FromString(TEXT("세 개의 다리"));
		IntroShot = TEXT("Bridges");
		IntroRadio = FText::FromString(TEXT("다리를 골라 건너편 집결지로 이동해. 지휘관에게 제어 권한이 있다."));
		Step(O::Rally, TEXT("다리를 건너 동쪽 집결지에서 G"), TEXT("East_Checkpoint"), TEXT(""), TEXT("East"));
		Step(O::Defeat, TEXT("지휘 거점 제압, 제어 권한 확보"), TEXT("Command_Trigger"), TEXT("Command"));
		Step(O::Rally, TEXT("중심 신전으로 출발"), TEXT("Exit"), TEXT(""), TEXT("Command"), TEXT(""), TEXT("제어 권한 확보. 중심 신전의 반출 장치를 정지시킬 수 있다."));
		Encounter(TEXT("North"), 0, 0, 1, 2);
		Encounter(TEXT("Center"), 0, 0, 1, 2);
		Encounter(TEXT("South"), 0, 0, 2, 0);
		Encounter(TEXT("Command"), 1, 2, 0, 4, true);
		Encounters.Last().Enemies[2].bKwang = true;
		Encounters.Last().Enemies[3].bKwang = true;
		Patrol(TEXT("South"));
		break;
	case 8:
		ChapterTitle = FText::FromString(TEXT("부유성의 심장"));
		Step(O::Defeat, TEXT("서쪽 제어기 경비 제압"), TEXT("West_Trigger"), TEXT("West"));
		Step(O::Interact, TEXT("외곽 제어기 작동"), TEXT("West_Control"), TEXT(""), TEXT("West"));
		Step(O::Rally, TEXT("유도등을 따라 중앙 광장에 집결"), TEXT("Core_Checkpoint"), TEXT(""), TEXT("Core"), TEXT("BossIntro"));
		Step(O::Defeat, TEXT("지휘관과 호위 제압"), TEXT("Boss_Trigger"), TEXT("Boss"));
		Step(O::Interact, TEXT("반출 장치 정지, 동력핵 안정화"), TEXT("Core_Control"), TEXT(""), TEXT("Stable"), TEXT("Stabilize"), TEXT("동력 유지 확인. 유적은 안정됐다. 철수해."));
		Step(O::Rally, TEXT("동쪽 귀환 장치에서 철수"), TEXT("Exit"), TEXT(""), TEXT(""), TEXT("Extraction"));
		Encounter(TEXT("West"), 0, 1, 1, 1);
		Encounter(TEXT("MazeA"), 2, 2, 1, 0);
		Encounter(TEXT("MazeB"), 2, 2, 1, 0);
		Encounter(TEXT("Boss"), 3, 4, 1, 2, true, true);
		Patrol(TEXT("MazeA"));
		Patrol(TEXT("MazeB"));
		Supply(TEXT("Maze_Supply_A"), TEXT(""));
		Supply(TEXT("Maze_Supply_B"), TEXT(""));
		break;
	default:
		ChapterTitle = FText::GetEmpty();
		break;
	}
}
