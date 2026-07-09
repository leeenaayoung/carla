// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Game/CarlaStatics.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrossWindZone.generated.h"

class UBoxComponent;
class ACarlaWheeledVehicle;

UCLASS()
class ACrossWindZone : public AActor
{
    GENERATED_BODY()

public:
    ACrossWindZone();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// parameters for wind effect calculation
    UPROPERTY(EditAnywhere, Category = "CrossWind")
        float MaxLateralAccel = 60.0f;

    UPROPERTY(EditAnywhere, Category = "CrossWind")
        float IdleLateralAccel = 23.0f;

    UPROPERTY(EditAnywhere, Category = "CrossWind")
        float IdleSpeedThreshold = 250.0f;

    UPROPERTY(EditAnywhere, Category = "CrossWind")
        float FullWindSpeed = 1800.0f;

	// crosswind sway frequency (how fast the wind gusts change over time)
    UPROPERTY(EditAnywhere, Category = "CrossWind")
        float SwayFrequency = 0.2f;

	// crosswind effect radius (for debug visualization, not used in force calculation since we use trigger box)
    UPROPERTY(EditAnywhere, Category = "CrossWind")
    float Radius = 5000.0f;

    UPROPERTY()
    ACarlaWheeledVehicle* EgoVehicle = nullptr;

private:
    UPROPERTY(VisibleAnywhere)
        UBoxComponent* TriggerBox;

    TSet<ACarlaWheeledVehicle*> VehiclesInZone;

	// add camera shake when entering crosswind zone
    UPROPERTY(EditAnywhere, Category = "CrossWind")
    TSubclassOf<UMatineeCameraShake> WindCameraShake;

    UPROPERTY(EditAnywhere, Category = "CrossWind")
    float ShakeScale = 1.0f;

    UFUNCTION()
        void OnBeginOverlap(
            UPrimitiveComponent* OverlappedComp,
            AActor* OtherActor,
            UPrimitiveComponent* OtherComp,
            int32 OtherBodyIndex,
            bool bFromSweep,
            const FHitResult& SweepResult);

    UFUNCTION()
        void OnEndOverlap(
            UPrimitiveComponent* OverlappedComp,
            AActor* OtherActor,
            UPrimitiveComponent* OtherComp,
            int32 OtherBodyIndex);
};