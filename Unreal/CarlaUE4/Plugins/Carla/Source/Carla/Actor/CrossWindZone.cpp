// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "CrossWindZone.h"
#include "Carla/Game/CarlaStatics.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Actor/CarlaActor.h"

ACrossWindZone::ACrossWindZone()
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(2000.0f, 400.0f, 150.0f));     // trigger box setting
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
}

void ACrossWindZone::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACrossWindZone::OnBeginOverlap);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACrossWindZone::OnEndOverlap);
}

void ACrossWindZone::OnBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    ACarlaWheeledVehicle* Vehicle = Cast<ACarlaWheeledVehicle>(OtherActor);
    if (!Vehicle) return;
    if (VehiclesInZone.Contains(Vehicle)) return;

    VehiclesInZone.Add(Vehicle);

    FString ActorId = TEXT("unknown");
    UCarlaEpisode* Episode = UCarlaStatics::GetCurrentEpisode(GetWorld());
    if (Episode)
    {
        FCarlaActor* CarlaActor = Episode->FindCarlaActor(Vehicle);
        if (CarlaActor)
        {
            ActorId = CarlaActor->GetActorInfo()->Description.Id;
        }
    }

    if (ActorId == TEXT("vehicle.tesla.model3"))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CrossWind] ENTERED Tesla Model 3 - %s / id=%s"),
            *OtherActor->GetName(),
            *ActorId);
    }
}

void ACrossWindZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector ZoneCenter = GetActorLocation();
    const FVector BoxExtent = TriggerBox->GetScaledBoxExtent();

    const float Time = World->GetTimeSeconds();
    const float SwayWave = FMath::Sin(Time * SwayFrequency);
    
    for (auto It = VehiclesInZone.CreateIterator(); It; ++It)
    //for (TActorIterator<ACarlaWheeledVehicle> It(World); It; ++It)
    {
        ACarlaWheeledVehicle* Vehicle = *It;
  //      if (!Vehicle) continue;

		//// check if vehicle is within the box area
  //      FVector VehicleLoc = Vehicle->GetActorLocation();
  //      FVector Diff = VehicleLoc - ZoneCenter;

  //      if (FMath::Abs(Diff.X) > BoxExtent.X ||
  //          FMath::Abs(Diff.Y) > BoxExtent.Y ||
  //          FMath::Abs(Diff.Z) > BoxExtent.Z)
  //          continue;
        if (!IsValid(Vehicle))
        {
            It.RemoveCurrent();
            continue;
        }

        USkeletalMeshComponent* Mesh = Vehicle->GetMesh();
        if (!Mesh || !Mesh->IsSimulatingPhysics()) continue;

        const float Speed = Vehicle->GetVelocity().Size();
        const FVector RightVector = Vehicle->GetActorRightVector();

        // Reduce wind effect while cornering
        const FVector AngularVel = Mesh->GetPhysicsAngularVelocityInDegrees();
        const float YawRate = FMath::Abs(AngularVel.Z);
        const float CornerAttenuation = FMath::Clamp(
            1.0f - (YawRate / 5.0f), 0.05f, 1.0f);

        // Reduce wind effect while steering
        const float SteerInput = FMath::Abs(Vehicle->GetVehicleControl().Steer);
        const float SpeedNorm = FMath::Clamp(Speed / 1500.0f, 0.0f, 1.0f);
        const float EffectiveSteer = SteerInput * (1.0f + SpeedNorm * 3.0f);
        const float SteerAttenuation = FMath::Clamp(
            1.0f - EffectiveSteer, 0.05f, 1.0f);

        // Calculate lateral acceleration based on speed
        float LateralAccelMag = 0.0f;
        float SpeedAlpha = 0.0f;

        if (Speed < IdleSpeedThreshold)
        {
            LateralAccelMag = IdleLateralAccel * SwayWave
                * CornerAttenuation * SteerAttenuation;
        }
        else
        {
            SpeedAlpha = FMath::Sqrt(FMath::Clamp(
                (Speed - IdleSpeedThreshold) / (FullWindSpeed - IdleSpeedThreshold),
                0.0f, 1.0f));

            const float StraightBonus = (YawRate < 1.0f && SteerInput < 0.05f)
                ? 1.3f   // 직진 시 30% 증폭
                : 1.0f;

            LateralAccelMag = MaxLateralAccel * SpeedAlpha * SwayWave
                * CornerAttenuation * SteerAttenuation * StraightBonus;
        }

        // Apply lateral physical force to simulate crosswind
        const float RawForce = LateralAccelMag * Mesh->GetMass() * 1.2f;
        const float MaxForce = Mesh->GetMass() * 150.0f;   // 질량 대비 상한
        const float ForceMag = FMath::Clamp(RawForce, -MaxForce, MaxForce);
        Mesh->AddForce(RightVector * ForceMag);

        // Debug: show current wind force and speed on screen
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1, 0.1f, FColor::Yellow,
                FString::Printf(TEXT("[CrossWind] Force=%.1f Speed=%.1f Sway=%.2f"),
                    ForceMag, Speed, SwayWave));
        }
    }
}

void ACrossWindZone::OnEndOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (!OtherActor) return;

    ACarlaWheeledVehicle* Vehicle = Cast<ACarlaWheeledVehicle>(OtherActor);
    if (!Vehicle) return;
    if (!VehiclesInZone.Contains(Vehicle)) return;

    VehiclesInZone.Remove(Vehicle);

    FString ActorId = TEXT("unknown");
    UCarlaEpisode* Episode = UCarlaStatics::GetCurrentEpisode(GetWorld());
    if (Episode)
    {
        FCarlaActor* CarlaActor = Episode->FindCarlaActor(Vehicle);
        if (CarlaActor)
        {
            ActorId = CarlaActor->GetActorInfo()->Description.Id;
        }
    }

    if (ActorId == TEXT("vehicle.tesla.model3"))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CrossWind] EXITED Tesla Model 3 - %s / id=%s"),
            *OtherActor->GetName(),
            *ActorId);
    }
}

void ACrossWindZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    VehiclesInZone.Empty();
    Super::EndPlay(EndPlayReason);
}