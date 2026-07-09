// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackIceZone.generated.h"

class UBoxComponent;
class ABlackIceVisualActor;
class ACarlaWheeledVehicle;

UCLASS()
class ABlackIceZone : public AActor
{
    GENERATED_BODY()

public:
    ABlackIceZone();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackIce")
    UBoxComponent* TriggerBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    ABlackIceVisualActor* VisualActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    float IceFrictionScale = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    bool bAffectOnlyEgo = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackIce")
    bool bZoneActive = false;

    UPROPERTY()
    TSet<ACarlaWheeledVehicle*> VehiclesInZone;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

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

    // black ice strength multiplier for slide force
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    float SlideForceMultiplier = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    float EntryImpulseStrength = 0.0f;
};