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

    const float BaseWindForce = 50000.0f;
    const float DrivingYawTorqueStrength = 0.0f;

    const float IdleSwayForce = 1300.0f;
    const float IdleYawTorqueStrength = 0.0f;

    //// 속도 구간 기준 (cm/s)
    const float IdleSpeedThreshold = 200.0f;     // 약 1.2 m/s 이하: 정지/극저속
    const float LowSpeedThreshold = 600.0f;     // 약 9 m/s 이하: 저속
    const float FullWindSpeed = 1500.0f;    // 이 이상은 충분히 강한 횡풍 영향

    // 주기 설정
    const float IdleSwayFreq = 0.95f;            // 정지 시 좌우 잔흔들림
    const float IdleYawFreq = 0.60f;            // 정지 시 yaw 잔흔들림
    
    const float GustFreq1 = 0.24f;
    const float GustFreq2 = 0.57f;

    // 속도에 따른 최소 바람 스케일
    const float MinDrivingWindScale = 0.08f;
    const float Time = World->GetTimeSeconds();

    for (TActorIterator<AWheeledVehicle> It(World); It; ++It)
    {
        AWheeledVehicle* Vehicle = *It;
        if (!Vehicle)
        {
            continue;
        }

        USkeletalMeshComponent* Mesh = Vehicle->GetMesh();

        if (!Mesh || !Mesh->IsSimulatingPhysics())
        {
            continue;
        }

        const float Speed = Vehicle->GetVelocity().Size(); // cm/s

        const FVector RightVector = Vehicle->GetActorRightVector();
        const FVector ForwardVector = Vehicle->GetActorForwardVector();
        const FVector UpVector = Vehicle->GetActorUpVector();

        // 1) 정지 / 극저속 구간
        if (Speed < IdleSpeedThreshold)
        {
            // 평균 0인 왕복형 흔들림
            const float IdleSway = FMath::Sin(Time * IdleSwayFreq);
            const float IdleYaw = FMath::Sin(Time * IdleYawFreq + 1.1f);

            // 정지 상태에서는 힘 가하는 위치를 중심에 더 가깝게 둠
            const FVector IdleForceLocation =
                Vehicle->GetActorLocation()
                + UpVector * 25.0f;

            const FVector SwayForce = RightVector * IdleSwayForce * IdleSway;

            Mesh->AddForceAtLocation(SwayForce, IdleForceLocation);
            /*Mesh->AddTorqueInRadians(FVector(0.0f, 0.0f, IdleYawTorqueStrength * IdleYaw));*/
            const FVector IdleRollTorque = ForwardVector * 200000.0f * IdleSway;
            Mesh->AddTorqueInRadians(IdleRollTorque, NAME_None, true);

            continue;
        }

        // 2) 주행 구간
        // 속도 증가에 따라 바람 영향 증가
        const float SpeedAlpha = FMath::Clamp(
            (Speed - IdleSpeedThreshold) / (FullWindSpeed - IdleSpeedThreshold),
            0.0f,
            1.0f
        );

        const float SpeedFactor = FMath::Lerp(MinDrivingWindScale, 1.0f, SpeedAlpha);
        const float Gust =
            0.60f
            + 0.38f * FMath::Sin(Time * 0.24f)
            + 0.18f * FMath::Sin(Time * 0.57f + 1.1f);

        const float ClampedGust = FMath::Clamp(Gust, 0.2f, 1.4f);

        float LowSpeedAttenuation = 1.0f;
        if (Speed < LowSpeedThreshold)
        {
            const float LowSpeedAlpha = FMath::Clamp(
                (Speed - IdleSpeedThreshold) / (LowSpeedThreshold - IdleSpeedThreshold),
                0.0f,
                1.0f
            );

            LowSpeedAttenuation = FMath::Lerp(0.35f, 1.0f, LowSpeedAlpha);
        }

        const float ForceMag = BaseWindForce * SpeedFactor * ClampedGust * LowSpeedAttenuation;
        const float DirectionBias = 0.9f + 0.1f * FMath::Sin(Time * 0.12f);
        const FVector WindForce = RightVector * ForceMag * DirectionBias;

        /*const FVector DrivingForceLocation =
            Vehicle->GetActorLocation()
            + UpVector * 100.0f;*/
            // 선형 힘: CoM에 직접 줘서 토크 없이 옆으로만 밀기
        Mesh->AddForce(WindForce, NAME_None, true);

        // Roll 토크: 별도로 따로 제어 (ForwardVector = X축)
        const float RollTorqueMag = 0.0f;
        const FVector RollTorque = ForwardVector * RollTorqueMag * ClampedGust * SpeedFactor;
        Mesh->AddTorqueInRadians(RollTorque, NAME_None, true);

        /*Mesh->AddForceAtLocation(WindForce, DrivingForceLocation);*/

        const float YawScale = FMath::Lerp(0.35f, 1.0f, SpeedAlpha);
        const float YawTorque = DrivingYawTorqueStrength * ClampedGust * YawScale * LowSpeedAttenuation;
    }
}