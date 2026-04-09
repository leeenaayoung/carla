// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "BlackIceZone.h"
#include "BlackIceVisualActor.h"

#include "Components/BoxComponent.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"

#include "DrawDebugHelpers.h"

ABlackIceZone::ABlackIceZone()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    TriggerBox->SetBoxExtent(FVector(180.0f, 90.0f, 80.0f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
}

void ABlackIceZone::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABlackIceZone::OnBeginOverlap);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ABlackIceZone::OnEndOverlap);

    //DrawDebugBox(
    //    GetWorld(),
    //    TriggerBox->GetComponentLocation(),
    //    TriggerBox->GetScaledBoxExtent(),
    //    TriggerBox->GetComponentQuat(),
    //    FColor::Cyan,
    //    true,
    //    9999.0f,
    //    0,
    //    5.0f
    //);

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

    UE_LOG(LogTemp, Warning, TEXT("[BlackIce] Enter Vehicle=%s Comp=%s"),
        *OtherActor->GetName(),
        OtherComp ? *OtherComp->GetName() : TEXT("None"));

    Vehicle->SetBlackIceFriction(IceFrictionScale);

    if (VisualActor)
    {
        VisualActor->SetIceVisible(true);
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

    UE_LOG(LogTemp, Warning, TEXT("[BlackIce] Exit Vehicle=%s, ZoneLoc=%s, VehicleLoc=%s"),
        *OtherActor->GetName(),
        *GetActorLocation().ToString(),
        *OtherActor->GetActorLocation().ToString());

    if (VisualActor && VehiclesInZone.Num() == 0)
    {
        VisualActor->SetIceVisible(false);
    }
}