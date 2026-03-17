// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "WindAmbienceActor.h"

#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

AWindAmbienceActor::AWindAmbienceActor()
{
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    WindDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("WindDirectionArrow"));
    WindDirectionArrow->SetupAttachment(RootComponent);
    WindDirectionArrow->ArrowSize = 2.0f;
    WindDirectionArrow->ArrowLength = 200.0f;

    WindAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("WindAudio"));
    WindAudio->SetupAttachment(RootComponent);
    WindAudio->bAutoActivate = false;
}

void AWindAmbienceActor::BeginPlay()
{
    Super::BeginPlay();

    if (WindLoopSound)
    {
        WindAudio->SetSound(WindLoopSound);
        WindAudio->SetVolumeMultiplier(WindVolume);

        if (bAutoPlay)
        {
            WindAudio->Play();
        }
    }
}