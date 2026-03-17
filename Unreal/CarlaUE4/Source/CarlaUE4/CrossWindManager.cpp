// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "CrossWindManager.h"
#include "EngineUtils.h"
#include "WheeledVehicle.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

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
    if (!World)
    {
        return;
    }

    //const float BaseWindForce = 850000.0f;
    //const float GustAmplitude = 0.55f;
    //const float GustFrequency = 0.9f;

    //const float MinWindScale = 0.20f;
    //const float YawTorqueStrength = 1000000.0f;
    const float BaseWindForce = 350000.0f;
    const float GustAmplitude = 0.9f;
    const float GustFrequency = 0.18f;

    const float MinWindScale = 0.15f;
    const float YawTorqueStrength = 80000.0f;

    const float Time = World->GetTimeSeconds();
    const float Gust = FMath::Max(0.0f, FMath::Sin(Time * GustFrequency));

    for (TActorIterator<AWheeledVehicle> It(World); It; ++It)
    {
        AWheeledVehicle* Vehicle = *It;
        if (!Vehicle)
        {
            continue;
        }

        // ego 차량만 적용
        if (!Vehicle->GetName().Contains(TEXT("BP_TeslaM3")))
        {
            continue;
        }

        USkeletalMeshComponent* Mesh = Vehicle->GetMesh();
        if (!Mesh || !Mesh->IsSimulatingPhysics())
        {
            continue;
        }

        const float Speed = Vehicle->GetVelocity().Size(); // cm/s
        const float SpeedFactor = FMath::Clamp(Speed / 700.0f, MinWindScale, 2.5f);

        const FVector RightVector = Vehicle->GetActorRightVector();
        const FVector ForwardVector = Vehicle->GetActorForwardVector();
        const FVector UpVector = Vehicle->GetActorUpVector();

        const float ForceMag = BaseWindForce * (0.50f + GustAmplitude * Gust) * SpeedFactor;
        const FVector WindForce = RightVector * ForceMag;

        const FVector ForceLocation =
            Vehicle->GetActorLocation()
            + ForwardVector * 100.0f
            + UpVector * 80.0f;

        Mesh->AddForceAtLocation(WindForce, ForceLocation);
        Mesh->AddTorqueInRadians(FVector(0.0f, 0.0f, YawTorqueStrength * Gust * SpeedFactor));

        UE_LOG(LogTemp, Warning, TEXT("[CrossWind][EGO] %s Speed=%.2f Force=%.2f Gust=%.2f Scale=%.2f"),
            *Vehicle->GetName(), Speed / 100.0f, ForceMag, Gust, SpeedFactor);
    }
}