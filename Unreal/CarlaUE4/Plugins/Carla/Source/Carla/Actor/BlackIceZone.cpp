// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Actor/CarlaActor.h"
#include "BlackIceZone.h"
#include "BlackIceVisualActor.h"

#include "Components/BoxComponent.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"
#include "DrawDebugHelpers.h"

ABlackIceZone::ABlackIceZone()
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    TriggerBox->SetBoxExtent(FVector(800.0f, 400.0f, 150.0f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
}

void ABlackIceZone::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABlackIceZone::OnBeginOverlap);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ABlackIceZone::OnEndOverlap);

    UE_LOG(LogTemp, Warning, TEXT("[BlackIceZone] Name=%s ActorLoc=%s TriggerLoc=%s Extent=%s"),
        *GetName(),
        *GetActorLocation().ToString(),
        *TriggerBox->GetComponentLocation().ToString(),
        *TriggerBox->GetScaledBoxExtent().ToString());
}

void ABlackIceZone::OnBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor)
    {
        return;
    }

    ACarlaWheeledVehicle* Vehicle = Cast<ACarlaWheeledVehicle>(OtherActor);
    if (!Vehicle)
    {
        return;
    }

    if (VehiclesInZone.Contains(Vehicle))
    {
        return;
    }

    VehiclesInZone.Add(Vehicle);

    Vehicle->SetBlackIceFriction(IceFrictionScale);

    /*USkeletalMeshComponent* Mesh = Vehicle->GetMesh();
    if (Mesh && Mesh->IsSimulatingPhysics())
    {
        const FVector Right = Vehicle->GetActorRightVector();
        const float Speed = Vehicle->GetVelocity().Size();
        const float ImpulseMag = EntryImpulseStrength * FMath::Clamp(Speed / 1500.0f, 0.0f, 1.0f);
        Mesh->AddImpulse(Right * ImpulseMag, NAME_None, true);
    }*/

    if (VisualActor)
    {
        VisualActor->SetIceVisible(true);
    }

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
        UE_LOG(LogTemp, Warning, TEXT("[BlackIce] ENTERED Tesla Model 3 - %s"), *OtherActor->GetName());
    }
}

void ABlackIceZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    for (auto It = VehiclesInZone.CreateIterator(); It; ++It)
    {
        ACarlaWheeledVehicle* Vehicle = *It;
        if (!IsValid(Vehicle))
        {
            It.RemoveCurrent();
            continue;
        }

        USkeletalMeshComponent* Mesh = Vehicle->GetMesh();
        if (!Mesh || !Mesh->IsSimulatingPhysics()) continue;

        const FVector Velocity = Vehicle->GetVelocity();
        const float Speed = Velocity.Size();
        if (Speed < 200.0f) continue;

        const FVector AngularVel = Mesh->GetPhysicsAngularVelocityInDegrees();
        const float YawRate = FMath::Abs(AngularVel.Z);
        if (YawRate > 8.0f) continue;   // ← 핵심: 스핀 시 완전 중단

        const FVector Right = Vehicle->GetActorRightVector();
        const FVector VelDir = Velocity.GetSafeNormal();
        const float LateralSlip = FVector::DotProduct(VelDir, Right);
        const float SteerInput = Vehicle->GetVehicleControl().Steer;

        const float SlipFactor = FMath::Clamp(
            FMath::Abs(LateralSlip) + FMath::Abs(SteerInput) * 0.2f,
            0.0f, 0.3f);   // 상한 0.6 → 0.3

        const float SpeedNorm = FMath::Clamp(Speed / 1500.0f, 0.0f, 1.0f);
        const float TurnEntryGuard = FMath::Clamp(
            1.0f - FMath::Abs(SteerInput) * (1.0f + SpeedNorm * 3.0f),
            0.05f, 1.0f);

        const float SpinGuard = FMath::Clamp(
            1.0f - (YawRate / 8.0f),
            0.0f, 1.0f);   // 최소값 0.1 → 0.0

        const float SpeedFactor = FMath::Sqrt(FMath::Clamp(
            Speed / 1500.0f, 0.0f, 1.0f));

        const float RawSlide = Mesh->GetMass()
            * SpeedFactor
            * SlideForceMultiplier
            * 1500.0f              // sqrt로 바꾸면서 스케일 보정
            * SlipFactor
            * SpinGuard
            * TurnEntryGuard
            * FMath::Sign(LateralSlip + SteerInput);

        /*const float SlideForceMag = Mesh->GetMass()
            * Speed
            * SlideForceMultiplier
            * SlipFactor
            * SpinGuard
            * FMath::Sign(LateralSlip + SteerInput);*/

        const float MaxSlide = Mesh->GetMass() * 80.0f;
        const float SlideForceMag = FMath::Clamp(RawSlide, -MaxSlide, MaxSlide);
        Mesh->AddForce(Right * SlideForceMag);
        //// 차량이 바라보는 방향과 실제 이동 방향의 차이(slip angle)
        //const FVector Forward = Vehicle->GetActorForwardVector();
        //const FVector VelDir = Velocity.GetSafeNormal();

        //// 횡방향 성분 = 속도 벡터를 차량 오른쪽 축에 투영
        //const FVector Right = Vehicle->GetActorRightVector();
        //const float LateralSlip = FVector::DotProduct(VelDir, Right);

        //// 스티어링 입력도 보조적으로 반영
        //const float SteerInput = Vehicle->GetVehicleControl().Steer;

        //// slip angle + 스티어링을 결합하여 슬라이드 힘 계산
        //const float SlipFactor = FMath::Clamp(
        //    FMath::Abs(LateralSlip) + FMath::Abs(SteerInput) * 0.3f,
        //    0.0f, 0.6f);

        //const FVector AngularVel = Mesh->GetPhysicsAngularVelocityInDegrees();
        //const float YawRate = FMath::Abs(AngularVel.Z);
        //const float SpinGuard = FMath::Clamp(
        //    1.0f - (YawRate / 15.0f),
        //    0.1f, 1.0f);

        //const float SlideForceMag = Mesh->GetMass()
        //    * Speed
        //    * SlideForceMultiplier
        //    * SlipFactor
        //    * SpinGuard              // ← 추가
        //    * FMath::Sign(LateralSlip + SteerInput);

        //// 횡방향으로 힘을 지속 적용 → 조향 불능 느낌
        //Mesh->AddForce(Right * SlideForceMag);
    }
}

void ABlackIceZone::OnEndOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (!OtherActor)
    {
        return;
    }

    ACarlaWheeledVehicle* Vehicle = Cast<ACarlaWheeledVehicle>(OtherActor);
    if (!Vehicle)
    {
        return;
    }

    if (!VehiclesInZone.Contains(Vehicle))
    {
        return;
    }

    VehiclesInZone.Remove(Vehicle);

    Vehicle->RestoreBlackIceFriction();

    if (VisualActor && VehiclesInZone.Num() == 0)
    {
        VisualActor->SetIceVisible(false);
    }

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
        UE_LOG(LogTemp, Warning, TEXT("[BlackIce] EXITED Tesla Model 3 - %s"), *OtherActor->GetName());
    }
}