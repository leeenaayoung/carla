#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindAmbienceActor.generated.h"

class USceneComponent;
class UAudioComponent;
class UArrowComponent;
class USoundBase;

UCLASS()
class CARLAUE4_API AWindAmbienceActor : public AActor
{
    GENERATED_BODY()

public:
    AWindAmbienceActor();

protected:
    virtual void BeginPlay() override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind")
        USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind")
        UArrowComponent* WindDirectionArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind")
        UAudioComponent* WindAudio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
        USoundBase* WindLoopSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
        float WindVolume = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
        bool bAutoPlay = true;
};