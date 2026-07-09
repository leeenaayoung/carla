// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "BlackIceVisualActor.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"

ABlackIceVisualActor::ABlackIceVisualActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    IceDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("IceDecal"));
    IceDecal->SetupAttachment(RootComponent);

    DecalSize = FVector(200.0f, 1200.0f, 250.0f);
    IceDecal->DecalSize = DecalSize;

    IceDecal->SetHiddenInGame(false);
}

void ABlackIceVisualActor::BeginPlay()
{
    Super::BeginPlay();

    if (DecalMaterial)
    {
        IceDecal->SetDecalMaterial(DecalMaterial);
    }

    IceDecal->DecalSize = DecalSize;
}

void ABlackIceVisualActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ABlackIceVisualActor::SetIceVisible(bool bVisible)
{
    if (IceDecal)
    {
        IceDecal->SetHiddenInGame(!bVisible);
        IceDecal->SetVisibility(bVisible);
    }
}

void ABlackIceVisualActor::UpdateDecalSize(FVector NewSize)
{
    DecalSize = NewSize;

    if (IceDecal)
    {
        IceDecal->DecalSize = DecalSize;
    }
}
