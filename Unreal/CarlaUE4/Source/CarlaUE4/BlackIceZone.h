// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackIceZone.generated.h"

class UBoxComponent;
class ABlackIceVisualActor;
class ACarlaWheeledVehicle;

UCLASS()
class CARLAUE4_API ABlackIceZone : public AActor
{
    GENERATED_BODY()

public:
    ABlackIceZone();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackIce")
    UBoxComponent* TriggerBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    ABlackIceVisualActor* VisualActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    float IceFrictionScale = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    bool bAffectOnlyEgo = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackIce")
    bool bZoneActive = false;

    UPROPERTY()
    ACarlaWheeledVehicle* CurrentVehicle = nullptr;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UFUNCTION()
    void OnEndOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );
};