#pragma once

#include "CoreMinimal.h"

class ACharacter;
class ARecastNavMesh;
class ULevel;
class UWorld;

namespace PFNavigationLinkBuilder
{
	bool Build(UWorld& World, ARecastNavMesh& NavMesh, const ACharacter& Bot, ULevel* Level,
		const FGuid& GeneratorId, FString& OutResult);
}
