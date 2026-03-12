// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "BlackIceZone.h"
#include "BlackIceVisualActor.h"

#include "Components/BoxComponent.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"

ABlackIceZone::ABlackIceZone()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    TriggerBox->SetBoxExtent(FVector(300.0f, 150.0f, 100.0f));
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

    if (bAffectOnlyEgo)
    {
        const FString ActorName = OtherActor->GetName();
        if (!ActorName.Contains(TEXT("Tesla")) && !ActorName.Contains(TEXT("Ego")) && !ActorName.Contains(TEXT("Audi")))
        {
            return;
        }
    }

    if (bZoneActive && CurrentVehicle == Vehicle)
    {
        return;
    }

    CurrentVehicle = Vehicle;
    bZoneActive = true;

    // low friction value
    Vehicle->SetBlackIceFriction(IceFrictionScale);

    if (VisualActor)
    {
        VisualActor->SetIceVisible(true);
    }
    UE_LOG(LogTemp, Warning, TEXT("[BlackIce] Enter Vehicle = %s"), *OtherActor->GetName());
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

	// Restore original friction
    if (Vehicle != CurrentVehicle)
    {
        return;
    }

    Vehicle->RestoreBlackIceFriction();

    if (VisualActor)
    {
        VisualActor->SetIceVisible(false);
    }

    UE_LOG(LogTemp, Warning, TEXT("[BlackIce] Exit Vehicle = %s"), *OtherActor->GetName());

    CurrentVehicle = nullptr;
    bZoneActive = false;
}