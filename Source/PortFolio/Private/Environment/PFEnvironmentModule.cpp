#include "Environment/PFEnvironmentModule.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Environment/PFEnvironmentInstancesComponent.h"

APFEnvironmentModule::APFEnvironmentModule()
{
	PrimaryActorTick.bCanEverTick = false;
	Instances = CreateDefaultSubobject<UPFEnvironmentInstancesComponent>(TEXT("Instances"));
	SetRootComponent(Instances);
	Instances->SetMobility(EComponentMobility::Static);
	Instances->SetCollisionProfileName(TEXT("BlockAll"));
	Instances->SetCanEverAffectNavigation(true);
}
