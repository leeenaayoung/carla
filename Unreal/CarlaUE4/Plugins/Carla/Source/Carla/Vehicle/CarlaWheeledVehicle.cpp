// Copyright (c) 2025 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
// Copyright (c) 2019 Intel Corporation
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla/Vehicle/CarlaWheeledVehicle.h"

#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MovementComponents/DefaultMovementComponent.h"
#include "PhysXPublic.h"
#include "PhysXVehicleManager.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TireConfig.h"
#include "UObject/UObjectGlobals.h"
#include "VehicleWheel.h"

#include "Carla.h"
#include "Carla/Game/CarlaHUD.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/Trigger/FrictionTrigger.h"
#include "Carla/Util/ActorAttacher.h"
#include "Carla/Util/BoundingBoxCalculator.h"
#include "Carla/Util/EmptyActor.h"
#include "Carla/Vegetation/VegetationManager.h"

// =============================================================================
// -- Constructor and destructor -----------------------------------------------
// =============================================================================

ACarlaWheeledVehicle::ACarlaWheeledVehicle(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    VehicleBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleBounds"));
    VehicleBounds->SetupAttachment(RootComponent);
    VehicleBounds->SetHiddenInGame(true);
    VehicleBounds->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

    VelocityControl = CreateDefaultSubobject<UVehicleVelocityControl>(TEXT("VelocityControl"));
    VelocityControl->Deactivate();

    GetVehicleMovementComponent()->bReverseAsBrake = false;
    BaseMovementComponent = CreateDefaultSubobject<UBaseCarlaMovementComponent>(TEXT("BaseMovementComponent"));
}

ACarlaWheeledVehicle::~ACarlaWheeledVehicle() {}

// =============================================================================
// -- Wheel collision -----------------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::SetWheelCollision(
    UWheeledVehicleMovementComponent4W* Vehicle4W,
    const FVehiclePhysicsControl& PhysicsControl)
{
#ifdef WHEEL_SWEEP_ENABLED
    const bool IsBike = IsTwoWheeledVehicle();

    if (IsBike)
    {
        return;
    }

    const bool IsEqual = Vehicle4W->UseSweepWheelCollision == PhysicsControl.UseSweepWheelCollision;

    if (IsEqual)
    {
        return;
    }

    Vehicle4W->UseSweepWheelCollision = PhysicsControl.UseSweepWheelCollision;
#else
    if (PhysicsControl.UseSweepWheelCollision)
    {
        UE_LOG(
            LogCarla,
            Warning,
            TEXT("Error: Sweep for wheel collision is not available. Make sure you have installed the required patch."));
    }
#endif
}

void ACarlaWheeledVehicle::SetWheelCollisionNW(
    UWheeledVehicleMovementComponentNW* VehicleNW,
    const FVehiclePhysicsControl& PhysicsControl)
{
#ifdef WHEEL_SWEEP_ENABLED
    const bool IsEqual = VehicleNW->UseSweepWheelCollision == PhysicsControl.UseSweepWheelCollision;

    if (IsEqual)
    {
        return;
    }

    VehicleNW->UseSweepWheelCollision = PhysicsControl.UseSweepWheelCollision;
#else
    if (PhysicsControl.UseSweepWheelCollision)
    {
        UE_LOG(
            LogCarla,
            Warning,
            TEXT("Error: Sweep for wheel collision is not available. Make sure you have installed the required patch."));
    }
#endif
}

// =============================================================================
// -- BeginPlay / Tick / EndPlay -----------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::BeginPlay()
{
    Super::BeginPlay();

    UDefaultMovementComponent::CreateDefaultMovementComponent(this);

    FTransform ActorInverseTransform = GetActorTransform().Inverse();
    ConstraintsComponents.Empty();
    DoorComponentsTransform.Empty();
    ConstraintDoor.Empty();

    for (FName& ComponentName : ConstraintComponentNames)
    {
        UPhysicsConstraintComponent* ConstraintComponent =
            Cast<UPhysicsConstraintComponent>(GetDefaultSubobjectByName(ComponentName));

        if (ConstraintComponent)
        {
            UPrimitiveComponent* DoorComponent = Cast<UPrimitiveComponent>(
                GetDefaultSubobjectByName(ConstraintComponent->ComponentName1.ComponentName));

            if (DoorComponent)
            {
                UE_LOG(LogCarla, Warning, TEXT("Door name: %s"), *(DoorComponent->GetName()));

                FTransform ComponentWorldTransform = DoorComponent->GetComponentTransform();
                FTransform RelativeTransform = ComponentWorldTransform * ActorInverseTransform;

                DoorComponentsTransform.Add(DoorComponent, RelativeTransform);
                ConstraintDoor.Add(ConstraintComponent, DoorComponent);
                ConstraintsComponents.Add(ConstraintComponent);
                ConstraintComponent->TermComponentConstraint();
            }
            else
            {
                UE_LOG(
                    LogCarla,
                    Error,
                    TEXT("Missing component for constraint: %s"),
                    *(ConstraintComponent->GetName()));
            }
        }
    }

    ResetConstraints();

    CollisionDisableConstraints.Empty();
    TArray<UPhysicsConstraintComponent*> Constraints;
    GetComponents(Constraints);

    for (UPhysicsConstraintComponent* Constraint : Constraints)
    {
        if (!ConstraintsComponents.Contains(Constraint))
        {
            UPrimitiveComponent* CollisionDisabledComponent1 = Cast<UPrimitiveComponent>(
                GetDefaultSubobjectByName(Constraint->ComponentName1.ComponentName));
            UPrimitiveComponent* CollisionDisabledComponent2 = Cast<UPrimitiveComponent>(
                GetDefaultSubobjectByName(Constraint->ComponentName2.ComponentName));

            if (CollisionDisabledComponent1)
            {
                CollisionDisableConstraints.Add(CollisionDisabledComponent1, Constraint);
            }

            if (CollisionDisabledComponent2)
            {
                CollisionDisableConstraints.Add(CollisionDisabledComponent2, Constraint);
            }
        }
    }

    UWheeledVehicleMovementComponent* MovementComponent = GetVehicleMovementComponent();

    if (MovementComponent)
    {
        check(MovementComponent != nullptr);

        TArray<AActor*> OverlapActors;
        GetOverlappingActors(OverlapActors, AFrictionTrigger::StaticClass());

        for (const auto& Actor : OverlapActors)
        {
            AFrictionTrigger* FrictionTrigger = Cast<AFrictionTrigger>(Actor);
            if (FrictionTrigger)
            {
                FVehiclePhysicsControl PhysicsControl = GetVehiclePhysicsControl();

                for (int32 i = 0; i < PhysicsControl.Wheels.Num(); ++i)
                {
                    PhysicsControl.Wheels[i].TireFriction = FrictionTrigger->Friction;
                }

                ApplyVehiclePhysicsControl(PhysicsControl);
                break;
            }
        }

        LastAppliedPhysicsControl = GetVehiclePhysicsControl();
        AckermannController.UpdateVehiclePhysics(this);
    }

    AddReferenceToManager();
}

void ACarlaWheeledVehicle::TickActor(
    float DeltaTime,
    enum ELevelTick TickType,
    FActorTickFunction& ThisTickFunction)
{
    Super::TickActor(DeltaTime, TickType, ThisTickFunction);

    FPoseSnapshot Pose;
    GetMesh()->SnapshotPose(Pose);

    for (FTransform& Transform : Pose.LocalTransforms)
    {
        Transform *= GetMesh()->GetComponentTransform();
    }

    WorldTransformedPose = Pose;
}

void ACarlaWheeledVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ShowDebugTelemetry(false);
    Super::EndPlay(EndPlayReason);
    RemoveReferenceToManager();
}

// =============================================================================
// -- Foliage / bounding box ----------------------------------------------------
// =============================================================================

bool ACarlaWheeledVehicle::IsInVehicleRange(const FVector& Location) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ACarlaWheeledVehicle::IsInVehicleRange);
    return FoliageBoundingBox.IsInside(Location);
}

void ACarlaWheeledVehicle::UpdateDetectionBox()
{
    const FTransform GlobalTransform = GetActorTransform();
    const FVector Vec{ DetectionSize, DetectionSize, DetectionSize };
    FBox Box = FBox(-Vec, Vec);

    const FTransform NonScaledTransform(
        GlobalTransform.GetRotation(),
        GlobalTransform.GetLocation(),
        { 1.0f, 1.0f, 1.0f });

    FoliageBoundingBox = Box.TransformBy(NonScaledTransform);
}

const TArray<int32> ACarlaWheeledVehicle::GetFoliageInstancesCloseToVehicle(
    const UInstancedStaticMeshComponent* Component) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ACarlaWheeledVehicle::GetFoliageInstancesCloseToVehicle);
    return Component->GetInstancesOverlappingBox(FoliageBoundingBox);
}

FBox ACarlaWheeledVehicle::GetDetectionBox() const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ACarlaWheeledVehicle::GetDetectionBox);
    return FoliageBoundingBox;
}

float ACarlaWheeledVehicle::GetDetectionSize() const
{
    return DetectionSize;
}

void ACarlaWheeledVehicle::DrawFoliageBoundingBox() const
{
    const FVector& Center = FoliageBoundingBox.GetCenter();
    const FVector& Extent = FoliageBoundingBox.GetExtent();
    const FQuat& Rotation = GetActorQuat();

    DrawDebugBox(GetWorld(), Center, Extent, Rotation, FColor::Magenta, false, 0.0f, 0, 5.0f);
}

FBoxSphereBounds ACarlaWheeledVehicle::GetBoxSphereBounds() const
{
    ALargeMapManager* LargeMap = UCarlaStatics::GetLargeMapManager(GetWorld());
    if (LargeMap)
    {
        FTransform GlobalTransform = LargeMap->LocalToGlobalTransform(GetActorTransform());
        return VehicleBounds->CalcBounds(GlobalTransform);
    }

    return VehicleBounds->CalcBounds(GetActorTransform());
}

void ACarlaWheeledVehicle::AdjustVehicleBounds()
{
    FBoundingBox BoundingBox = UBoundingBoxCalculator::GetVehicleBoundingBox(this);

    const FTransform& CompToWorldTransform = RootComponent->GetComponentTransform();
    const FRotator Rotation = CompToWorldTransform.GetRotation().Rotator();
    const FVector Translation = CompToWorldTransform.GetLocation();
    const FVector Scale = CompToWorldTransform.GetScale3D();

    BoundingBox.Origin -= Translation;
    BoundingBox.Origin = Rotation.UnrotateVector(BoundingBox.Origin);
    BoundingBox.Origin /= Scale;

    FTransform Transform;
    Transform.SetTranslation(BoundingBox.Origin);

    VehicleBounds->SetRelativeTransform(Transform);
    VehicleBounds->SetBoxExtent(BoundingBox.Extent);
}

// =============================================================================
// -- Get functions -------------------------------------------------------------
// =============================================================================

float ACarlaWheeledVehicle::GetVehicleForwardSpeed() const
{
    return BaseMovementComponent->GetVehicleForwardSpeed();
}

FVector ACarlaWheeledVehicle::GetVehicleOrientation() const
{
    return GetVehicleTransform().GetRotation().GetForwardVector();
}

int32 ACarlaWheeledVehicle::GetVehicleCurrentGear() const
{
    return BaseMovementComponent->GetVehicleCurrentGear();
}

FTransform ACarlaWheeledVehicle::GetVehicleBoundingBoxTransform() const
{
    return VehicleBounds->GetRelativeTransform();
}

FVector ACarlaWheeledVehicle::GetVehicleBoundingBoxExtent() const
{
    return VehicleBounds->GetScaledBoxExtent();
}

float ACarlaWheeledVehicle::GetMaximumSteerAngle() const
{
    const auto& Wheels = GetVehicleMovementComponent()->Wheels;
    check(Wheels.Num() > 0);

    const auto* FrontWheel = Wheels[0];
    check(FrontWheel != nullptr);

    return FrontWheel->SteerAngle;
}

FVehicleLightState ACarlaWheeledVehicle::GetVehicleLightState() const
{
    return InputControl.LightState;
}

FVehiclePhysicsControl ACarlaWheeledVehicle::GetVehiclePhysicsControl() const
{
    FVehiclePhysicsControl PhysicsControl;

    if (!bIsNWVehicle)
    {
        UWheeledVehicleMovementComponent4W* Vehicle4W =
            Cast<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());
        check(Vehicle4W != nullptr);

        PhysicsControl.TorqueCurve = Vehicle4W->EngineSetup.TorqueCurve.EditorCurveData;
        PhysicsControl.MaxRPM = Vehicle4W->EngineSetup.MaxRPM;
        PhysicsControl.MOI = Vehicle4W->EngineSetup.MOI;
        PhysicsControl.DampingRateFullThrottle = Vehicle4W->EngineSetup.DampingRateFullThrottle;
        PhysicsControl.DampingRateZeroThrottleClutchEngaged =
            Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchEngaged;
        PhysicsControl.DampingRateZeroThrottleClutchDisengaged =
            Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchDisengaged;

        PhysicsControl.bUseGearAutoBox = Vehicle4W->TransmissionSetup.bUseGearAutoBox;
        PhysicsControl.GearSwitchTime = Vehicle4W->TransmissionSetup.GearSwitchTime;
        PhysicsControl.ClutchStrength = Vehicle4W->TransmissionSetup.ClutchStrength;
        PhysicsControl.FinalRatio = Vehicle4W->TransmissionSetup.FinalRatio;

        TArray<FGearPhysicsControl> ForwardGears;
        for (const auto& Gear : Vehicle4W->TransmissionSetup.ForwardGears)
        {
            FGearPhysicsControl GearPhysicsControl;
            GearPhysicsControl.Ratio = Gear.Ratio;
            GearPhysicsControl.UpRatio = Gear.UpRatio;
            GearPhysicsControl.DownRatio = Gear.DownRatio;
            ForwardGears.Add(GearPhysicsControl);
        }
        PhysicsControl.ForwardGears = ForwardGears;

        PhysicsControl.Mass = Vehicle4W->Mass;
        PhysicsControl.DragCoefficient = Vehicle4W->DragCoefficient;

        UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
        check(UpdatedPrimitive != nullptr);
        PhysicsControl.CenterOfMass = UpdatedPrimitive->BodyInstance.COMNudge;

        PhysicsControl.SteeringCurve = Vehicle4W->SteeringCurve.EditorCurveData;

        TArray<FWheelPhysicsControl> Wheels;

        for (int32 i = 0; i < Vehicle4W->WheelSetups.Num(); ++i)
        {
            FWheelPhysicsControl PhysicsWheel;

            if (bPhysicsEnabled)
            {
                PxVehicleWheelData PWheelData = Vehicle4W->PVehicle->mWheelsSimData.getWheelData(i);

                PhysicsWheel.DampingRate = Cm2ToM2(PWheelData.mDampingRate);
                PhysicsWheel.MaxSteerAngle = FMath::RadiansToDegrees(PWheelData.mMaxSteer);
                PhysicsWheel.Radius = PWheelData.mRadius;
                PhysicsWheel.MaxBrakeTorque = Cm2ToM2(PWheelData.mMaxBrakeTorque);
                PhysicsWheel.MaxHandBrakeTorque = Cm2ToM2(PWheelData.mMaxHandBrakeTorque);

                PxVehicleTireData PTireData = Vehicle4W->PVehicle->mWheelsSimData.getTireData(i);
                PhysicsWheel.LatStiffMaxLoad = PTireData.mLatStiffX;
                PhysicsWheel.LatStiffValue = PTireData.mLatStiffY;
                PhysicsWheel.LongStiffValue = PTireData.mLongitudinalStiffnessPerUnitGravity;
                PhysicsWheel.TireFriction = Vehicle4W->Wheels[i]->TireConfig->GetFrictionScale();
                PhysicsWheel.Position = Vehicle4W->Wheels[i]->Location;
            }
            else
            {
                if (i < LastAppliedPhysicsControl.Wheels.Num())
                {
                    PhysicsWheel = LastAppliedPhysicsControl.Wheels[i];
                }
            }

            Wheels.Add(PhysicsWheel);
        }

        PhysicsControl.Wheels = Wheels;
    }
    else
    {
        UWheeledVehicleMovementComponentNW* VehicleNW =
            Cast<UWheeledVehicleMovementComponentNW>(GetVehicleMovement());
        check(VehicleNW != nullptr);

        PhysicsControl.TorqueCurve = VehicleNW->EngineSetup.TorqueCurve.EditorCurveData;
        PhysicsControl.MaxRPM = VehicleNW->EngineSetup.MaxRPM;
        PhysicsControl.MOI = VehicleNW->EngineSetup.MOI;
        PhysicsControl.DampingRateFullThrottle = VehicleNW->EngineSetup.DampingRateFullThrottle;
        PhysicsControl.DampingRateZeroThrottleClutchEngaged =
            VehicleNW->EngineSetup.DampingRateZeroThrottleClutchEngaged;
        PhysicsControl.DampingRateZeroThrottleClutchDisengaged =
            VehicleNW->EngineSetup.DampingRateZeroThrottleClutchDisengaged;

        PhysicsControl.bUseGearAutoBox = VehicleNW->TransmissionSetup.bUseGearAutoBox;
        PhysicsControl.GearSwitchTime = VehicleNW->TransmissionSetup.GearSwitchTime;
        PhysicsControl.ClutchStrength = VehicleNW->TransmissionSetup.ClutchStrength;
        PhysicsControl.FinalRatio = VehicleNW->TransmissionSetup.FinalRatio;

        TArray<FGearPhysicsControl> ForwardGears;
        for (const auto& Gear : VehicleNW->TransmissionSetup.ForwardGears)
        {
            FGearPhysicsControl GearPhysicsControl;
            GearPhysicsControl.Ratio = Gear.Ratio;
            GearPhysicsControl.UpRatio = Gear.UpRatio;
            GearPhysicsControl.DownRatio = Gear.DownRatio;
            ForwardGears.Add(GearPhysicsControl);
        }
        PhysicsControl.ForwardGears = ForwardGears;

        PhysicsControl.Mass = VehicleNW->Mass;
        PhysicsControl.DragCoefficient = VehicleNW->DragCoefficient;

        UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(VehicleNW->UpdatedComponent);
        check(UpdatedPrimitive != nullptr);
        PhysicsControl.CenterOfMass = UpdatedPrimitive->BodyInstance.COMNudge;

        PhysicsControl.SteeringCurve = VehicleNW->SteeringCurve.EditorCurveData;

        TArray<FWheelPhysicsControl> Wheels;

        for (int32 i = 0; i < VehicleNW->WheelSetups.Num(); ++i)
        {
            FWheelPhysicsControl PhysicsWheel;

            if (bPhysicsEnabled)
            {
                PxVehicleWheelData PWheelData = VehicleNW->PVehicle->mWheelsSimData.getWheelData(i);

                PhysicsWheel.DampingRate = Cm2ToM2(PWheelData.mDampingRate);
                PhysicsWheel.MaxSteerAngle = FMath::RadiansToDegrees(PWheelData.mMaxSteer);
                PhysicsWheel.Radius = PWheelData.mRadius;
                PhysicsWheel.MaxBrakeTorque = Cm2ToM2(PWheelData.mMaxBrakeTorque);
                PhysicsWheel.MaxHandBrakeTorque = Cm2ToM2(PWheelData.mMaxHandBrakeTorque);

                PxVehicleTireData PTireData = VehicleNW->PVehicle->mWheelsSimData.getTireData(i);
                PhysicsWheel.LatStiffMaxLoad = PTireData.mLatStiffX;
                PhysicsWheel.LatStiffValue = PTireData.mLatStiffY;
                PhysicsWheel.LongStiffValue = PTireData.mLongitudinalStiffnessPerUnitGravity;
            }
            else
            {
                if (i < LastAppliedPhysicsControl.Wheels.Num())
                {
                    PhysicsWheel = LastAppliedPhysicsControl.Wheels[i];
                }
            }

            PhysicsWheel.TireFriction = VehicleNW->Wheels[i]->TireConfig->GetFrictionScale();
            PhysicsWheel.Position = VehicleNW->Wheels[i]->Location;

            Wheels.Add(PhysicsWheel);
        }

        PhysicsControl.Wheels = Wheels;
    }

    return PhysicsControl;
}

FVector ACarlaWheeledVehicle::GetVelocity() const
{
    return BaseMovementComponent->GetVelocity();
}

FPoseSnapshot ACarlaWheeledVehicle::GetWorldTransformedPose()
{
    if (WorldTransformedPose.bIsValid == false)
    {
        SetActorTickEnabled(true);
        GetMesh()->SnapshotPose(WorldTransformedPose);

        for (FTransform& Transform : WorldTransformedPose.LocalTransforms)
        {
            Transform *= GetMesh()->GetComponentTransform();
        }
    }

    return WorldTransformedPose;
}

carla::rpc::VehicleFailureState ACarlaWheeledVehicle::GetFailureState() const
{
    return FailureState;
}

// =============================================================================
// -- Set functions -------------------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::FlushVehicleControl()
{
    if (IsAckermannControlActive())
    {
        AckermannController.UpdateVehicleState(this);
        AckermannController.RunLoop(InputControl.Control);
    }

    BaseMovementComponent->ProcessControl(InputControl.Control);
    InputControl.Control.bReverse = InputControl.Control.Gear < 0;
    LastAppliedControl = InputControl.Control;
    InputControl.Priority = EVehicleInputPriority::INVALID;
}

void ACarlaWheeledVehicle::SetThrottleInput(const float Value)
{
    FVehicleControl Control = InputControl.Control;
    Control.Throttle = Value;
    ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetSteeringInput(const float Value)
{
    FVehicleControl Control = InputControl.Control;
    Control.Steer = Value;
    ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetBrakeInput(const float Value)
{
    FVehicleControl Control = InputControl.Control;
    Control.Brake = Value;
    ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetReverse(const bool Value)
{
    FVehicleControl Control = InputControl.Control;
    Control.bReverse = Value;
    ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetHandbrakeInput(const bool Value)
{
    FVehicleControl Control = InputControl.Control;
    Control.bHandBrake = Value;
    ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

TArray<float> ACarlaWheeledVehicle::GetWheelsFrictionScale()
{
    UWheeledVehicleMovementComponent* Movement = GetVehicleMovement();
    TArray<float> WheelsFrictionScale;

    if (Movement)
    {
        check(Movement != nullptr);

        for (auto& Wheel : Movement->Wheels)
        {
            WheelsFrictionScale.Add(Wheel->TireConfig->GetFrictionScale());
        }
    }

    return WheelsFrictionScale;
}

void ACarlaWheeledVehicle::SetWheelsFrictionScale(TArray<float>& WheelsFrictionScale)
{
    UWheeledVehicleMovementComponent* Movement = GetVehicleMovement();
    if (Movement)
    {
        check(Movement != nullptr);
        check(Movement->Wheels.Num() == WheelsFrictionScale.Num());

        for (int32 i = 0; i < Movement->Wheels.Num(); ++i)
        {
            Movement->Wheels[i]->TireConfig->SetFrictionScale(WheelsFrictionScale[i]);
        }
    }
}

void ACarlaWheeledVehicle::SetBlackIceFriction(float NewFrictionScale)
{
    UWheeledVehicleMovementComponent* Movement = GetVehicleMovement();
    if (!Movement)
    {
        return;
    }

    FVehiclePhysicsControl PhysicsControl = GetVehiclePhysicsControl();

    if (PhysicsControl.Wheels.Num() != Movement->Wheels.Num())
    {
        UE_LOG(LogCarla, Warning, TEXT("SetBlackIceFriction: invalid wheel count."));
        return;
    }

    if (!bBlackIceActive)
    {
        SavedWheelPhysics = PhysicsControl.Wheels;
    }

    if (SavedWheelPhysics.Num() != Movement->Wheels.Num())
    {
        UE_LOG(LogCarla, Warning, TEXT("SetBlackIceFriction: invalid saved wheel physics."));
        SavedWheelPhysics.Empty();
        return;
    }

    const float ClampedFriction = FMath::Clamp(NewFrictionScale, 0.02f, 0.3f);

    for (int32 i = 0; i < PhysicsControl.Wheels.Num(); ++i)
    {
        PhysicsControl.Wheels[i].TireFriction = ClampedFriction;
        PhysicsControl.Wheels[i].LatStiffValue *= 0.35f;
        PhysicsControl.Wheels[i].LongStiffValue *= 0.35f;
    }

    ApplyVehiclePhysicsControl(PhysicsControl);
    bBlackIceActive = true;

    UE_LOG(LogCarla, Warning, TEXT("[BlackIce] Friction applied: %.3f"), ClampedFriction);
}

void ACarlaWheeledVehicle::RestoreBlackIceFriction()
{
    UWheeledVehicleMovementComponent* Movement = GetVehicleMovement();
    if (!Movement)
    {
        return;
    }

    if (!bBlackIceActive)
    {
        return;
    }

    FVehiclePhysicsControl PhysicsControl = GetVehiclePhysicsControl();

    if (SavedWheelPhysics.Num() != PhysicsControl.Wheels.Num())
    {
        UE_LOG(LogCarla, Warning, TEXT("RestoreBlackIceFriction: saved wheel count mismatch."));
        SavedWheelPhysics.Empty();
        bBlackIceActive = false;
        return;
    }

    for (int32 i = 0; i < PhysicsControl.Wheels.Num(); ++i)
    {
        PhysicsControl.Wheels[i] = SavedWheelPhysics[i];
    }

    ApplyVehiclePhysicsControl(PhysicsControl);

    SavedWheelPhysics.Empty();
    bBlackIceActive = false;

    UE_LOG(LogCarla, Warning, TEXT("[BlackIce] Friction restored."));
}

void ACarlaWheeledVehicle::RestoreVehiclePhysicsControl()
{
    ApplyVehiclePhysicsControl(LastAppliedPhysicsControl);
}

void ACarlaWheeledVehicle::ApplyVehiclePhysicsControl(const FVehiclePhysicsControl& PhysicsControl)
{
    LastAppliedPhysicsControl = PhysicsControl;

    if (!bIsNWVehicle)
    {
        UWheeledVehicleMovementComponent4W* Vehicle4W =
            Cast<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());
        check(Vehicle4W != nullptr);

        Vehicle4W->EngineSetup.TorqueCurve.EditorCurveData = PhysicsControl.TorqueCurve;
        Vehicle4W->EngineSetup.MaxRPM = PhysicsControl.MaxRPM;
        Vehicle4W->EngineSetup.MOI = PhysicsControl.MOI;
        Vehicle4W->EngineSetup.DampingRateFullThrottle = PhysicsControl.DampingRateFullThrottle;
        Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchEngaged =
            PhysicsControl.DampingRateZeroThrottleClutchEngaged;
        Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchDisengaged =
            PhysicsControl.DampingRateZeroThrottleClutchDisengaged;

        Vehicle4W->TransmissionSetup.bUseGearAutoBox = PhysicsControl.bUseGearAutoBox;
        Vehicle4W->TransmissionSetup.GearSwitchTime = PhysicsControl.GearSwitchTime;
        Vehicle4W->TransmissionSetup.ClutchStrength = PhysicsControl.ClutchStrength;
        Vehicle4W->TransmissionSetup.FinalRatio = PhysicsControl.FinalRatio;

        TArray<FVehicleGearData> ForwardGears;
        for (const auto& Gear : PhysicsControl.ForwardGears)
        {
            FVehicleGearData GearData;
            GearData.Ratio = Gear.Ratio;
            GearData.UpRatio = Gear.UpRatio;
            GearData.DownRatio = Gear.DownRatio;
            ForwardGears.Add(GearData);
        }
        Vehicle4W->TransmissionSetup.ForwardGears = ForwardGears;

        Vehicle4W->Mass = PhysicsControl.Mass;
        Vehicle4W->DragCoefficient = PhysicsControl.DragCoefficient;

        UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
        check(UpdatedPrimitive != nullptr);
        UpdatedPrimitive->BodyInstance.COMNudge = PhysicsControl.CenterOfMass;

        Vehicle4W->SteeringCurve.EditorCurveData = PhysicsControl.SteeringCurve;

        const int32 PhysicsWheelsNum = PhysicsControl.Wheels.Num();
        if (PhysicsWheelsNum != 4)
        {
            UE_LOG(LogCarla, Error, TEXT("Number of WheelPhysicsControl is not 4."));
            return;
        }

        SetWheelCollision(Vehicle4W, PhysicsControl);

        TArray<FWheelSetup> NewWheelSetups = Vehicle4W->WheelSetups;

        for (int32 i = 0; i < PhysicsWheelsNum; ++i)
        {
            UVehicleWheel* Wheel = NewWheelSetups[i].WheelClass.GetDefaultObject();
            check(Wheel != nullptr);

            Wheel->TireConfig = DuplicateObject<UTireConfig>(Wheel->TireConfig, nullptr);
            Wheel->TireConfig->SetFrictionScale(PhysicsControl.Wheels[i].TireFriction);
        }

        Vehicle4W->WheelSetups = NewWheelSetups;

        GetWorld()->GetPhysicsScene()->GetPxScene()->lockWrite();
        Vehicle4W->RecreatePhysicsState();
        GetWorld()->GetPhysicsScene()->GetPxScene()->unlockWrite();

        for (int32 i = 0; i < PhysicsWheelsNum; ++i)
        {
            PxVehicleWheelData PWheelData = Vehicle4W->PVehicle->mWheelsSimData.getWheelData(i);

            PWheelData.mRadius = PhysicsControl.Wheels[i].Radius;
            PWheelData.mMaxSteer = FMath::DegreesToRadians(PhysicsControl.Wheels[i].MaxSteerAngle);
            PWheelData.mDampingRate = M2ToCm2(PhysicsControl.Wheels[i].DampingRate);
            PWheelData.mMaxBrakeTorque = M2ToCm2(PhysicsControl.Wheels[i].MaxBrakeTorque);
            PWheelData.mMaxHandBrakeTorque = M2ToCm2(PhysicsControl.Wheels[i].MaxHandBrakeTorque);
            Vehicle4W->PVehicle->mWheelsSimData.setWheelData(i, PWheelData);

            PxVehicleTireData PTireData = Vehicle4W->PVehicle->mWheelsSimData.getTireData(i);
            PTireData.mLatStiffX = PhysicsControl.Wheels[i].LatStiffMaxLoad;
            PTireData.mLatStiffY = PhysicsControl.Wheels[i].LatStiffValue;
            PTireData.mLongitudinalStiffnessPerUnitGravity = PhysicsControl.Wheels[i].LongStiffValue;
            Vehicle4W->PVehicle->mWheelsSimData.setTireData(i, PTireData);
        }

        ResetConstraints();
    }
    else
    {
        UWheeledVehicleMovementComponentNW* VehicleNW =
            Cast<UWheeledVehicleMovementComponentNW>(GetVehicleMovement());
        check(VehicleNW != nullptr);

        VehicleNW->EngineSetup.TorqueCurve.EditorCurveData = PhysicsControl.TorqueCurve;
        VehicleNW->EngineSetup.MaxRPM = PhysicsControl.MaxRPM;
        VehicleNW->EngineSetup.MOI = PhysicsControl.MOI;
        VehicleNW->EngineSetup.DampingRateFullThrottle = PhysicsControl.DampingRateFullThrottle;
        VehicleNW->EngineSetup.DampingRateZeroThrottleClutchEngaged =
            PhysicsControl.DampingRateZeroThrottleClutchEngaged;
        VehicleNW->EngineSetup.DampingRateZeroThrottleClutchDisengaged =
            PhysicsControl.DampingRateZeroThrottleClutchDisengaged;

        VehicleNW->TransmissionSetup.bUseGearAutoBox = PhysicsControl.bUseGearAutoBox;
        VehicleNW->TransmissionSetup.GearSwitchTime = PhysicsControl.GearSwitchTime;
        VehicleNW->TransmissionSetup.ClutchStrength = PhysicsControl.ClutchStrength;
        VehicleNW->TransmissionSetup.FinalRatio = PhysicsControl.FinalRatio;

        TArray<FVehicleNWGearData> ForwardGears;
        for (const auto& Gear : PhysicsControl.ForwardGears)
        {
            FVehicleNWGearData GearData;
            GearData.Ratio = Gear.Ratio;
            GearData.UpRatio = Gear.UpRatio;
            GearData.DownRatio = Gear.DownRatio;
            ForwardGears.Add(GearData);
        }
        VehicleNW->TransmissionSetup.ForwardGears = ForwardGears;

        VehicleNW->Mass = PhysicsControl.Mass;
        VehicleNW->DragCoefficient = PhysicsControl.DragCoefficient;

        UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(VehicleNW->UpdatedComponent);
        check(UpdatedPrimitive != nullptr);
        UpdatedPrimitive->BodyInstance.COMNudge = PhysicsControl.CenterOfMass;

        VehicleNW->SteeringCurve.EditorCurveData = PhysicsControl.SteeringCurve;

        const int32 PhysicsWheelsNum = PhysicsControl.Wheels.Num();

        SetWheelCollisionNW(VehicleNW, PhysicsControl);

        TArray<FWheelSetup> NewWheelSetups = VehicleNW->WheelSetups;

        for (int32 i = 0; i < PhysicsWheelsNum; ++i)
        {
            UVehicleWheel* Wheel = NewWheelSetups[i].WheelClass.GetDefaultObject();
            check(Wheel != nullptr);

            Wheel->TireConfig = DuplicateObject<UTireConfig>(Wheel->TireConfig, nullptr);
            Wheel->TireConfig->SetFrictionScale(PhysicsControl.Wheels[i].TireFriction);
        }

        VehicleNW->WheelSetups = NewWheelSetups;

        GetWorld()->GetPhysicsScene()->GetPxScene()->lockWrite();
        VehicleNW->RecreatePhysicsState();
        GetWorld()->GetPhysicsScene()->GetPxScene()->unlockWrite();

        for (int32 i = 0; i < PhysicsWheelsNum; ++i)
        {
            PxVehicleWheelData PWheelData = VehicleNW->PVehicle->mWheelsSimData.getWheelData(i);

            PWheelData.mRadius = PhysicsControl.Wheels[i].Radius;
            PWheelData.mMaxSteer = FMath::DegreesToRadians(PhysicsControl.Wheels[i].MaxSteerAngle);
            PWheelData.mDampingRate = M2ToCm2(PhysicsControl.Wheels[i].DampingRate);
            PWheelData.mMaxBrakeTorque = M2ToCm2(PhysicsControl.Wheels[i].MaxBrakeTorque);
            PWheelData.mMaxHandBrakeTorque = M2ToCm2(PhysicsControl.Wheels[i].MaxHandBrakeTorque);
            VehicleNW->PVehicle->mWheelsSimData.setWheelData(i, PWheelData);

            PxVehicleTireData PTireData = VehicleNW->PVehicle->mWheelsSimData.getTireData(i);
            PTireData.mLatStiffX = PhysicsControl.Wheels[i].LatStiffMaxLoad;
            PTireData.mLatStiffY = PhysicsControl.Wheels[i].LatStiffValue;
            PTireData.mLongitudinalStiffnessPerUnitGravity = PhysicsControl.Wheels[i].LongStiffValue;
            VehicleNW->PVehicle->mWheelsSimData.setTireData(i, PTireData);
        }

        ResetConstraints();
    }

    auto* Recorder = UCarlaStatics::GetRecorder(GetWorld());
    if (Recorder && Recorder->IsEnabled())
    {
        Recorder->AddPhysicsControl(*this);
    }

    AckermannController.UpdateVehiclePhysics(this);
}

void ACarlaWheeledVehicle::ActivateVelocityControl(const FVector& Velocity)
{
    VelocityControl->Activate(Velocity);
}

void ACarlaWheeledVehicle::DeactivateVelocityControl()
{
    VelocityControl->Deactivate();
}

void ACarlaWheeledVehicle::SetVehicleLightState(const FVehicleLightState& LightState)
{
    if (LightState.Position != InputControl.LightState.Position ||
        LightState.LowBeam != InputControl.LightState.LowBeam ||
        LightState.HighBeam != InputControl.LightState.HighBeam ||
        LightState.Brake != InputControl.LightState.Brake ||
        LightState.RightBlinker != InputControl.LightState.RightBlinker ||
        LightState.LeftBlinker != InputControl.LightState.LeftBlinker ||
        LightState.Reverse != InputControl.LightState.Reverse ||
        LightState.Fog != InputControl.LightState.Fog ||
        LightState.Interior != InputControl.LightState.Interior ||
        LightState.Special1 != InputControl.LightState.Special1 ||
        LightState.Special2 != InputControl.LightState.Special2)
    {
        InputControl.LightState = LightState;
        RefreshLightState(LightState);
    }
}

void ACarlaWheeledVehicle::SetFailureState(const carla::rpc::VehicleFailureState& InFailureState)
{
    FailureState = InFailureState;
}

void ACarlaWheeledVehicle::SetCarlaMovementComponent(UBaseCarlaMovementComponent* MovementComponent)
{
    if (BaseMovementComponent)
    {
        BaseMovementComponent->DestroyComponent();
    }

    BaseMovementComponent = MovementComponent;
}

// =============================================================================
// -- Wheel animation helpers ---------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::SetWheelSteerDirection(EVehicleWheelLocation WheelLocation, float AngleInDeg)
{
    if (bPhysicsEnabled == false)
    {
        check((uint8)WheelLocation >= 0);

        UVehicleAnimInstance* VehicleAnim = Cast<UVehicleAnimInstance>(GetMesh()->GetAnimInstance());
        check(VehicleAnim != nullptr);

        VehicleAnim->SetWheelRotYaw((uint8)WheelLocation, AngleInDeg);
    }
    else
    {
        UE_LOG(LogCarla, Warning, TEXT("Cannot set wheel steer direction. Physics are enabled."));
    }
}

float ACarlaWheeledVehicle::GetWheelSteerAngle(EVehicleWheelLocation WheelLocation)
{
    check((uint8)WheelLocation >= 0);

    UVehicleAnimInstance* VehicleAnim = Cast<UVehicleAnimInstance>(GetMesh()->GetAnimInstance());
    check(VehicleAnim != nullptr);
    check(VehicleAnim->GetWheeledVehicleMovementComponent() != nullptr);

    if (bPhysicsEnabled == true)
    {
        return VehicleAnim->GetWheeledVehicleMovementComponent()->Wheels[(uint8)WheelLocation]->GetSteerAngle();
    }
    else
    {
        return VehicleAnim->GetWheelRotAngle((uint8)WheelLocation);
    }
}

void ACarlaWheeledVehicle::SetWheelPitchAngle(EVehicleWheelLocation WheelLocation, float AngleInDeg)
{
    if (bPhysicsEnabled == false)
    {
        check((uint8)WheelLocation >= 0);

        UVehicleAnimInstance* VehicleAnim = Cast<UVehicleAnimInstance>(GetMesh()->GetAnimInstance());
        check(VehicleAnim != nullptr);

        VehicleAnim->SetWheelPitchAngle((uint8)WheelLocation, AngleInDeg);
    }
    else
    {
        UE_LOG(LogCarla, Warning, TEXT("Cannot set wheel pitch angle. Physics are enabled."));
    }
}

float ACarlaWheeledVehicle::GetWheelPitchAngle(EVehicleWheelLocation WheelLocation)
{
    check((uint8)WheelLocation >= 0);

    UVehicleAnimInstance* VehicleAnim = Cast<UVehicleAnimInstance>(GetMesh()->GetAnimInstance());
    check(VehicleAnim != nullptr);
    check(VehicleAnim->GetWheeledVehicleMovementComponent() != nullptr);

    if (bPhysicsEnabled == true)
    {
        return VehicleAnim->GetWheeledVehicleMovementComponent()->Wheels[(uint8)WheelLocation]->GetRotationAngle();
    }
    else
    {
        return VehicleAnim->GetWheelPitchAngle((uint8)WheelLocation);
    }
}

void ACarlaWheeledVehicle::SetSimulatePhysics(bool enabled)
{
    if (!GetCarlaMovementComponent<UDefaultMovementComponent>())
    {
        return;
    }

    UWheeledVehicleMovementComponent* Movement = GetVehicleMovement();
    if (Movement)
    {
        check(Movement != nullptr);

        if (bPhysicsEnabled == enabled)
        {
            return;
        }

        SetActorEnableCollision(true);

        auto RootPrimitive = Cast<UPrimitiveComponent>(GetRootComponent());
        RootPrimitive->SetSimulatePhysics(enabled);
        RootPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        UVehicleAnimInstance* VehicleAnim = Cast<UVehicleAnimInstance>(GetMesh()->GetAnimInstance());
        check(VehicleAnim != nullptr);

        GetWorld()->GetPhysicsScene()->GetPxScene()->lockWrite();
        if (enabled)
        {
            Movement->RecreatePhysicsState();
            VehicleAnim->ResetWheelCustomRotations();
        }
        else
        {
            Movement->DestroyPhysicsState();
        }
        GetWorld()->GetPhysicsScene()->GetPxScene()->unlockWrite();

        bPhysicsEnabled = enabled;
        ResetConstraints();
    }
}

// =============================================================================
// -- Telemetry -----------------------------------------------------------------
// =============================================================================

FVehicleTelemetryData ACarlaWheeledVehicle::GetVehicleTelemetryData() const
{
    FVehicleTelemetryData TelemetryData;

    auto* MovementComponent = GetVehicleMovement();

    TelemetryData.Speed = GetVehicleForwardSpeed() / 100.0f;
    TelemetryData.Steer = LastAppliedControl.Steer;
    TelemetryData.Throttle = LastAppliedControl.Throttle;
    TelemetryData.Brake = LastAppliedControl.Brake;
    TelemetryData.EngineRPM = MovementComponent->GetEngineRotationSpeed();
    TelemetryData.Gear = GetVehicleCurrentGear();
    TelemetryData.Drag = MovementComponent->DebugDragMagnitude / 100.0f;

    FPhysXVehicleManager* MyVehicleManager =
        FPhysXVehicleManager::GetVehicleManagerFromScene(GetWorld()->GetPhysicsScene());

    SCOPED_SCENE_READ_LOCK(MyVehicleManager->GetScene());
    PxWheelQueryResult* WheelsStates =
        MyVehicleManager->GetWheelsStates_AssumesLocked(MovementComponent);
    check(WheelsStates);

    TArray<FWheelTelemetryData> Wheels;
    for (uint32 w = 0; w < MovementComponent->PVehicle->mWheelsSimData.getNbWheels(); ++w)
    {
        FWheelTelemetryData WheelTelemetryData;

        WheelTelemetryData.TireFriction = WheelsStates[w].tireFriction;
        WheelTelemetryData.LatSlip = FMath::RadiansToDegrees(WheelsStates[w].lateralSlip);
        WheelTelemetryData.LongSlip = WheelsStates[w].longitudinalSlip;
        WheelTelemetryData.Omega = MovementComponent->PVehicle->mWheelsDynData.getWheelRotationSpeed(w);

        UVehicleWheel* Wheel = MovementComponent->Wheels[w];
        WheelTelemetryData.TireLoad = Wheel->DebugTireLoad / 100.0f;
        WheelTelemetryData.NormalizedTireLoad = Wheel->DebugNormalizedTireLoad;
        WheelTelemetryData.Torque = Wheel->DebugWheelTorque / (100.0f * 100.0f);
        WheelTelemetryData.LongForce = Wheel->DebugLongForce / 100.f;
        WheelTelemetryData.LatForce = Wheel->DebugLatForce / 100.f;
        WheelTelemetryData.NormalizedLongForce =
            (FMath::Abs(WheelTelemetryData.LongForce) * WheelTelemetryData.NormalizedTireLoad) /
            (WheelTelemetryData.TireLoad);
        WheelTelemetryData.NormalizedLatForce =
            (FMath::Abs(WheelTelemetryData.LatForce) * WheelTelemetryData.NormalizedTireLoad) /
            (WheelTelemetryData.TireLoad);

        Wheels.Add(WheelTelemetryData);
    }

    TelemetryData.Wheels = Wheels;

    return TelemetryData;
}

void ACarlaWheeledVehicle::ShowDebugTelemetry(bool Enabled)
{
    if (GetWorld()->GetFirstPlayerController())
    {
        ACarlaHUD* Hud = Cast<ACarlaHUD>(GetWorld()->GetFirstPlayerController()->GetHUD());
        if (Hud)
        {
            if (Enabled)
            {
                Hud->AddDebugVehicleForTelemetry(GetVehicleMovementComponent());
            }
            else
            {
                if (Hud->DebugVehicle == GetVehicleMovementComponent())
                {
                    Hud->AddDebugVehicleForTelemetry(nullptr);
                    GetVehicleMovementComponent()->StopTelemetry();
                }
            }
        }
        else
        {
            UE_LOG(
                LogCarla,
                Warning,
                TEXT("ACarlaWheeledVehicle::ShowDebugTelemetry:: Cannot find HUD for debug info"));
        }
    }
}

// =============================================================================
// -- Door functions ------------------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::ResetConstraints()
{
    for (int32 i = 0; i < ConstraintsComponents.Num(); i++)
    {
        OpenDoorPhys(EVehicleDoor(i));
    }

    for (int32 i = 0; i < ConstraintsComponents.Num(); i++)
    {
        CloseDoorPhys(EVehicleDoor(i));
    }
}

void ACarlaWheeledVehicle::OpenDoor(const EVehicleDoor DoorIdx)
{
    if (int(DoorIdx) >= ConstraintsComponents.Num() && DoorIdx != EVehicleDoor::All)
    {
        UE_LOG(LogCarla, Warning, TEXT("This door is not configured for this car."));
        return;
    }

    if (DoorIdx == EVehicleDoor::All)
    {
        for (int32 i = 0; i < ConstraintsComponents.Num(); i++)
        {
            OpenDoorPhys(EVehicleDoor(i));
        }
        return;
    }

    OpenDoorPhys(DoorIdx);
}

void ACarlaWheeledVehicle::CloseDoor(const EVehicleDoor DoorIdx)
{
    if (int(DoorIdx) >= ConstraintsComponents.Num() && DoorIdx != EVehicleDoor::All)
    {
        UE_LOG(LogCarla, Warning, TEXT("This door is not configured for this car."));
        return;
    }

    if (DoorIdx == EVehicleDoor::All)
    {
        for (int32 i = 0; i < ConstraintsComponents.Num(); i++)
        {
            CloseDoorPhys(EVehicleDoor(i));
        }
        return;
    }

    CloseDoorPhys(DoorIdx);
}

void ACarlaWheeledVehicle::OpenDoorPhys(const EVehicleDoor DoorIdx)
{
    UPhysicsConstraintComponent* Constraint = ConstraintsComponents[static_cast<int>(DoorIdx)];
    UPrimitiveComponent* DoorComponent = ConstraintDoor[Constraint];

    DoorComponent->DetachFromComponent(
        FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));

    FTransform DoorInitialTransform = DoorComponentsTransform[DoorComponent] * GetActorTransform();
    DoorComponent->SetWorldTransform(DoorInitialTransform);
    DoorComponent->SetSimulatePhysics(true);
    DoorComponent->SetCollisionProfileName(TEXT("BlockAll"));

    float AngleLimit = Constraint->ConstraintInstance.GetAngularSwing1Limit();

    if (Constraint->ConstraintInstance.AngularRotationOffset.Yaw < 0.0f)
    {
        AngleLimit = -AngleLimit;
    }

    Constraint->SetAngularOrientationTarget(FRotator(0, AngleLimit, 0));
    Constraint->SetAngularDriveParams(DoorOpenStrength, 1.0, 0.0);
    Constraint->InitComponentConstraint();

    UPhysicsConstraintComponent** CollisionDisable = CollisionDisableConstraints.Find(DoorComponent);
    if (CollisionDisable)
    {
        (*CollisionDisable)->InitComponentConstraint();
    }

    RecordDoorChange(DoorIdx, true);
}

void ACarlaWheeledVehicle::CloseDoorPhys(const EVehicleDoor DoorIdx)
{
    UPhysicsConstraintComponent* Constraint = ConstraintsComponents[static_cast<int>(DoorIdx)];
    UPrimitiveComponent* DoorComponent = ConstraintDoor[Constraint];

    FTransform DoorInitialTransform = DoorComponentsTransform[DoorComponent] * GetActorTransform();

    DoorComponent->SetSimulatePhysics(false);
    DoorComponent->SetCollisionProfileName(TEXT("NoCollision"));
    DoorComponent->SetWorldTransform(DoorInitialTransform);
    DoorComponent->AttachToComponent(
        GetMesh(),
        FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));

    RecordDoorChange(DoorIdx, false);
}

void ACarlaWheeledVehicle::RecordDoorChange(const EVehicleDoor DoorIdx, bool bIsOpen)
{
    auto* Recorder = UCarlaStatics::GetRecorder(GetWorld());
    if (Recorder && Recorder->IsEnabled())
    {
        Recorder->AddVehicleDoor(*this, DoorIdx, bIsOpen);
    }
}

// =============================================================================
// -- Rollover ------------------------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::ApplyRolloverBehavior()
{
    auto Roll = GetVehicleTransform().Rotator().Roll;

    switch (RolloverBehaviorTracker)
    {
    case 0: CheckRollover(Roll, std::make_pair(130.0, 230.0)); break;
    case 1: CheckRollover(Roll, std::make_pair(140.0, 220.0)); break;
    case 2: CheckRollover(Roll, std::make_pair(150.0, 210.0)); break;
    case 3: CheckRollover(Roll, std::make_pair(160.0, 200.0)); break;
    case 4:
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandler,
            this,
            &ACarlaWheeledVehicle::SetRolloverFlag,
            RolloverFlagTime);
        RolloverBehaviorTracker += 1;
        break;
    case 5:
        break;
    default:
        RolloverBehaviorTracker = 5;
        break;
    }

    if (RolloverBehaviorTracker > 0 && -30 < Roll && Roll < 30)
    {
        RolloverBehaviorTracker = 0;
        FailureState = carla::rpc::VehicleFailureState::None;
    }
}

void ACarlaWheeledVehicle::CheckRollover(
    const float Roll,
    const std::pair<float, float> ThresholdRoll)
{
    if (ThresholdRoll.first < Roll && Roll < ThresholdRoll.second)
    {
        auto RootPrimitive = Cast<UPrimitiveComponent>(GetRootComponent());
        auto AngularVelocity = RootPrimitive->GetPhysicsAngularVelocityInDegrees();
        RootPrimitive->SetPhysicsAngularVelocity((1 - RolloverBehaviorForce) * AngularVelocity);
        RolloverBehaviorTracker += 1;
    }
}

void ACarlaWheeledVehicle::SetRolloverFlag()
{
    if (RolloverBehaviorTracker >= 4)
    {
        FailureState = carla::rpc::VehicleFailureState::Rollover;
    }
}

// =============================================================================
// -- Vegetation manager --------------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::AddReferenceToManager()
{
    const UObject* World = GetWorld();
    TArray<AActor*> ActorsInLevel;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), ActorsInLevel);

    for (AActor* Actor : ActorsInLevel)
    {
        AVegetationManager* Manager = Cast<AVegetationManager>(Actor);
        if (!IsValid(Manager))
        {
            continue;
        }

        Manager->AddVehicle(this);
        return;
    }
}

void ACarlaWheeledVehicle::RemoveReferenceToManager()
{
    const UObject* World = GetWorld();
    TArray<AActor*> ActorsInLevel;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), ActorsInLevel);

    for (AActor* Actor : ActorsInLevel)
    {
        AVegetationManager* Manager = Cast<AVegetationManager>(Actor);
        if (!IsValid(Manager))
        {
            continue;
        }

        Manager->RemoveVehicle(this);
        return;
    }
}

// =============================================================================
// -- Constraint helpers --------------------------------------------------------
// =============================================================================

FRotator ACarlaWheeledVehicle::GetPhysicsConstraintAngle(UPhysicsConstraintComponent* Component)
{
    return Component->ConstraintInstance.AngularRotationOffset;
}

void ACarlaWheeledVehicle::SetPhysicsConstraintAngle(
    UPhysicsConstraintComponent* Component,
    const FRotator& NewAngle)
{
    Component->ConstraintInstance.AngularRotationOffset = NewAngle;
}