// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "CrossWindManager.h"
#include "EngineUtils.h"
#include "WheeledVehicle.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"

ACrossWindManager::ACrossWindManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACrossWindManager::BeginPlay()
{
    Super::BeginPlay();
}

void ACrossWindManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UWorld* World = GetWorld();
    if (!World) return;

    const float Time = World->GetTimeSeconds();

    const float MaxLateralAccel = 90.0f;
    const float IdleLateralAccel = 30.0f;
    const float IdleSpeedThreshold = 250.0f;
    const float FullWindSpeed = 1500.0f;

    for (TActorIterator<AWheeledVehicle> It(World); It; ++It)
    {
        AWheeledVehicle* Vehicle = *It;
        if (!Vehicle) continue;

        USkeletalMeshComponent* Mesh = Vehicle->GetMesh();
        if (!Mesh || !Mesh->IsSimulatingPhysics()) continue;

        const float Speed = Vehicle->GetVelocity().Size();
        const FVector RightVector = Vehicle->GetActorRightVector();

        // 코너 감쇠
        const FVector AngularVel = Mesh->GetPhysicsAngularVelocityInDegrees();
        const float YawRate = FMath::Abs(AngularVel.Z);
        const float CornerAttenuation = FMath::Clamp(
            1.0f - (YawRate / 10.0f),
            0.15f,
            1.0f
        );

        // 스티어 감쇠
        float SteerAttenuation = 1.0f;
        ACarlaWheeledVehicle* CarlaVehicle = Cast<ACarlaWheeledVehicle>(Vehicle);
        if (CarlaVehicle)
        {
            const float SteerInput = FMath::Abs(CarlaVehicle->GetVehicleControl().Steer);
            SteerAttenuation = FMath::Clamp(1.0f - SteerInput * 0.7f, 0.3f, 1.0f);
        }

        const float SwayWave = FMath::Sin(Time * 0.15f);

        float LateralAccelMag = 0.0f;

        if (Speed < IdleSpeedThreshold)
        {
            LateralAccelMag = IdleLateralAccel * SwayWave;
        }
        else
        {
            const float SpeedAlpha = FMath::Clamp(
                (Speed - IdleSpeedThreshold) / (FullWindSpeed - IdleSpeedThreshold),
                0.0f, 1.0f
            );
            LateralAccelMag = MaxLateralAccel * SpeedAlpha * SwayWave
                * CornerAttenuation * SteerAttenuation;
        }

        const float OffsetAmount = LateralAccelMag * DeltaTime * DeltaTime * 0.5f;
        Vehicle->SetActorLocation(
            Vehicle->GetActorLocation() + RightVector * OffsetAmount,
            true, nullptr, ETeleportType::TeleportPhysics
        );
    }
}