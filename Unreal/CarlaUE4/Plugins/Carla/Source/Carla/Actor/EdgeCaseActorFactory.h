// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "Carla/Actor/CarlaActorFactory.h"
#include "EdgeCaseActorFactory.generated.h"

UCLASS()
class AEdgeCaseActorFactory : public ACarlaActorFactory
{
    GENERATED_BODY()

public:
    AEdgeCaseActorFactory(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer) {
    }

    TArray<FActorDefinition> GetDefinitions() override;

    FActorSpawnResult SpawnActor(
        const FTransform& SpawnAtTransform,
        const FActorDescription& ActorDescription) override;
};
