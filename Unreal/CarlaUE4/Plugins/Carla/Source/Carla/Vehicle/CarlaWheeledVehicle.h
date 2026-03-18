// Copyright (c) 2025 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
// Copyright (c) 2019 Intel Corporation
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"

#include "Vehicle/AckermannController.h"
#include "Vehicle/AckermannControllerSettings.h"
#include "Vehicle/CarlaWheeledVehicleState.h"
#include "Vehicle/VehicleAckermannControl.h"
#include "Vehicle/VehicleControl.h"
#include "Vehicle/VehicleLightState.h"
#include "Vehicle/VehicleInputPriority.h"
#include "Vehicle/VehiclePhysicsControl.h"
#include "Vehicle/VehicleTelemetryData.h"
#include "VehicleVelocityControl.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "WheeledVehicleMovementComponentNW.h"
#include "VehicleAnimInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "MovementComponents/BaseCarlaMovementComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

#ifdef WITH_CARSIM
#include "CarSimMovementComponent.h"
#endif

#include <utility>

#include "carla/rpc/VehicleFailureState.h"
#include "CarlaWheeledVehicle.generated.h"

class UBoxComponent;

UENUM()
enum class EVehicleWheelLocation : uint8
{
    FL_Wheel = 0,
    FR_Wheel = 1,
    BL_Wheel = 2,
    BR_Wheel = 3,
    ML_Wheel = 4,
    MR_Wheel = 5,

    // Use for bikes and bicycles
    Front_Wheel = 0,
    Back_Wheel = 1,
};

UENUM(BlueprintType)
enum class EVehicleDoor : uint8
{
    FL = 0,
    FR = 1,
    RL = 2,
    RR = 3,
    Hood = 4,
    Trunk = 5,
    All = 6
};

UCLASS()
class CARLA_API ACarlaWheeledVehicle : public AWheeledVehicle
{
    GENERATED_BODY()

public:
    // custom header to add BlackIce
    //UFUNCTION(BlueprintCallable, Category = "BlackIce")
    //void SetBlackIceFriction(float NewScale);

    //UFUNCTION(BlueprintCallable, Category = "BlackIce")
    //void RestoreBlackIceFriction();

    ACarlaWheeledVehicle(const FObjectInitializer& ObjectInitializer);
    ~ACarlaWheeledVehicle();

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    const FVehicleControl& GetVehicleControl() const
    {
        return LastAppliedControl;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    const FVehicleAckermannControl& GetVehicleAckermannControl() const
    {
        return LastAppliedAckermannControl;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FTransform GetVehicleTransform() const
    {
        return GetActorTransform();
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    float GetVehicleForwardSpeed() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FVector GetVehicleOrientation() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    int32 GetVehicleCurrentGear() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FTransform GetVehicleBoundingBoxTransform() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FVector GetVehicleBoundingBoxExtent() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    UBoxComponent* GetVehicleBoundingBox() const
    {
        return VehicleBounds;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    float GetMaximumSteerAngle() const;

    void SetAIVehicleState(ECarlaWheeledVehicleState InState)
    {
        State = InState;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    ECarlaWheeledVehicleState GetAIVehicleState() const
    {
        return State;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FVehiclePhysicsControl GetVehiclePhysicsControl() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FAckermannControllerSettings GetAckermannControllerSettings() const
    {
        return AckermannController.GetSettings();
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void RestoreVehiclePhysicsControl();

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FVehicleLightState GetVehicleLightState() const;

    void ApplyVehiclePhysicsControl(const FVehiclePhysicsControl& PhysicsControl);

    void ApplyAckermannControllerSettings(const FAckermannControllerSettings& AckermannControllerSettings)
    {
        AckermannController.ApplySettings(AckermannControllerSettings);
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetSimulatePhysics(bool enabled);

    void SetWheelCollision(UWheeledVehicleMovementComponent4W* Vehicle4W, const FVehiclePhysicsControl& PhysicsControl);
    void SetWheelCollisionNW(UWheeledVehicleMovementComponentNW* VehicleNW, const FVehiclePhysicsControl& PhysicsControl);

    void SetVehicleLightState(const FVehicleLightState& LightState);
    void SetFailureState(const carla::rpc::VehicleFailureState& FailureState);

    UFUNCTION(BlueprintNativeEvent)
    bool IsTwoWheeledVehicle();

    virtual bool IsTwoWheeledVehicle_Implementation()
    {
        return false;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ApplyVehicleControl(const FVehicleControl& Control, EVehicleInputPriority Priority)
    {
        if (ActiveVehicleController::AckermannControl == CurrentActiveController)
        {
            AckermannController.Reset();
        }

        CurrentActiveController = ActiveVehicleController::VehicleControl;

        if (InputControl.Priority <= Priority)
        {
            InputControl.Control = Control;
            InputControl.Priority = Priority;
        }
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ApplyVehicleAckermannControl(const FVehicleAckermannControl& AckermannControl, EVehicleInputPriority Priority)
    {
        CurrentActiveController = ActiveVehicleController::AckermannControl;
        LastAppliedAckermannControl = AckermannControl;
        AckermannController.SetTargetPoint(AckermannControl);
    }

    bool IsAckermannControlActive() const
    {
        return ActiveVehicleController::AckermannControl == CurrentActiveController;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ActivateVelocityControl(const FVector& Velocity);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void DeactivateVelocityControl();

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    FVehicleTelemetryData GetVehicleTelemetryData() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ShowDebugTelemetry(bool Enabled);

    void FlushVehicleControl();

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetThrottleInput(float Value);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetSteeringInput(float Value);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetBrakeInput(float Value);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetReverse(bool Value);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ToggleReverse()
    {
        SetReverse(!LastAppliedControl.bReverse);
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetHandbrakeInput(bool Value);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void HoldHandbrake()
    {
        SetHandbrakeInput(true);
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ReleaseHandbrake()
    {
        SetHandbrakeInput(false);
    }

    TArray<float> GetWheelsFrictionScale();
    void SetWheelsFrictionScale(TArray<float>& WheelsFrictionScale);

    void SetCarlaMovementComponent(UBaseCarlaMovementComponent* MovementComponent);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetBlackIceFriction(float NewScale);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void RestoreBlackIceFriction();

    /*UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    bool IsBlackIceActive() const
    {
        return bBlackIceActive;
    }*/

    template <typename T = UBaseCarlaMovementComponent>
    T* GetCarlaMovementComponent() const
    {
        return Cast<T>(BaseMovementComponent);
    }

protected:

    virtual void BeginPlay() override;
    virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintImplementableEvent)
    void RefreshLightState(const FVehicleLightState& VehicleLightState);

    UFUNCTION(BlueprintCallable, CallInEditor)
    void AdjustVehicleBounds();

    UPROPERTY(Category = "Door Animation", EditAnywhere, BlueprintReadWrite)
    TArray<FName> ConstraintComponentNames;

    UPROPERTY(Category = "Door Animation", EditAnywhere, BlueprintReadWrite)
    float DoorOpenStrength = 100.0f;

    UFUNCTION(BlueprintCallable, CallInEditor)
    void ResetConstraints();

private:

    TArray<float> OriginalTireFrictionScales;

    bool bSavedOriginalTireFriction = false;

    UPROPERTY(Category = "AI Controller", VisibleAnywhere)
    ECarlaWheeledVehicleState State = ECarlaWheeledVehicleState::UNKNOWN;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", EditAnywhere)
    UVehicleVelocityControl* VelocityControl = nullptr;

    struct
    {
        EVehicleInputPriority Priority = EVehicleInputPriority::INVALID;
        FVehicleControl Control;
        FVehicleLightState LightState;
    } InputControl;

    enum class ActiveVehicleController
    {
        VehicleControl,
        AckermannControl
    };

    ActiveVehicleController CurrentActiveController = ActiveVehicleController::VehicleControl;

    FVehicleControl LastAppliedControl;
    FVehicleAckermannControl LastAppliedAckermannControl;
    FVehiclePhysicsControl LastAppliedPhysicsControl;

    FAckermannController AckermannController;

    float RolloverBehaviorForce = 0.35f;
    int RolloverBehaviorTracker = 0;
    float RolloverFlagTime = 5.0f;

    carla::rpc::VehicleFailureState FailureState = carla::rpc::VehicleFailureState::None;

    /*UPROPERTY()
    TArray<FWheelPhysicsControl> SavedWheelPhysics;*/

    /*UPROPERTY()
    bool bBlackIceActive = false;*/

public:

    UPROPERTY(Category = "CARLA Wheeled Vehicle", EditDefaultsOnly)
    float DetectionSize = 750.0f;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere, BlueprintReadOnly)
    FBox FoliageBoundingBox;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", EditAnywhere)
    UBoxComponent* VehicleBounds = nullptr;

    UFUNCTION()
    FBox GetDetectionBox() const;

    UFUNCTION()
    float GetDetectionSize() const;

    UFUNCTION()
    void UpdateDetectionBox();

    UFUNCTION()
    const TArray<int32> GetFoliageInstancesCloseToVehicle(const UInstancedStaticMeshComponent* Component) const;

    UFUNCTION(BlueprintCallable)
    void DrawFoliageBoundingBox() const;

    UFUNCTION()
    FBoxSphereBounds GetBoxSphereBounds() const;

    UFUNCTION()
    bool IsInVehicleRange(const FVector& Location) const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetWheelSteerDirection(EVehicleWheelLocation WheelLocation, float AngleInDeg);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    float GetWheelSteerAngle(EVehicleWheelLocation WheelLocation);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetWheelPitchAngle(EVehicleWheelLocation WheelLocation, float AngleInDeg);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    float GetWheelPitchAngle(EVehicleWheelLocation WheelLocation);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void OpenDoor(const EVehicleDoor DoorIdx);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void CloseDoor(const EVehicleDoor DoorIdx);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void OpenDoorPhys(const EVehicleDoor DoorIdx);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void CloseDoorPhys(const EVehicleDoor DoorIdx);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void RecordDoorChange(const EVehicleDoor DoorIdx, const bool bIsOpen);

    virtual FVector GetVelocity() const override;

    UFUNCTION()
    FPoseSnapshot GetWorldTransformedPose();

    UPROPERTY(Category = "CARLA Wheeled Vehicle", EditAnywhere)
    float CarSimOriginOffset = 150.0f;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere)
    bool bIsNWVehicle = false;

    void SetRolloverFlag();
    carla::rpc::VehicleFailureState GetFailureState() const;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    static FRotator GetPhysicsConstraintAngle(UPhysicsConstraintComponent* Component);

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    static void SetPhysicsConstraintAngle(UPhysicsConstraintComponent* Component, const FRotator& NewAngle);

private:

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere)
    bool bPhysicsEnabled = true;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UBaseCarlaMovementComponent* BaseMovementComponent = nullptr;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TArray<UPhysicsConstraintComponent*> ConstraintsComponents;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TMap<UPhysicsConstraintComponent*, UPrimitiveComponent*> ConstraintDoor;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TMap<UPrimitiveComponent*, FTransform> DoorComponentsTransform;

    UPROPERTY(Category = "CARLA Wheeled Vehicle", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TMap<UPrimitiveComponent*, UPhysicsConstraintComponent*> CollisionDisableConstraints;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void ApplyRolloverBehavior();

    void CheckRollover(const float roll, const std::pair<float, float> threshold_roll);

    void AddReferenceToManager();
    void RemoveReferenceToManager();

    FTimerHandle TimerHandler;

public:

    float SpeedAnim = 0.0f;
    float RotationAnim = 0.0f;
    FPoseSnapshot WorldTransformedPose;

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    float GetSpeedAnim() const
    {
        return SpeedAnim;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetSpeedAnim(float Speed)
    {
        SpeedAnim = Speed;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    float GetRotationAnim() const
    {
        return RotationAnim;
    }

    UFUNCTION(Category = "CARLA Wheeled Vehicle", BlueprintCallable)
    void SetRotationAnim(float Rotation)
    {
        RotationAnim = Rotation;
    }
};