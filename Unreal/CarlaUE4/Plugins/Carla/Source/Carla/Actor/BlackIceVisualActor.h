// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackIceVisualActor.generated.h"

class UDecalComponent;
class UMaterialInterface;
class USceneComponent;

UCLASS()
class ABlackIceVisualActor : public AActor
{
    GENERATED_BODY()

public:
    ABlackIceVisualActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // 루트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackIce")
    USceneComponent* Root;

    // 시각 표현용 데칼
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackIce")
    UDecalComponent* IceDecal;

    // 에디터에서 지정할 머티리얼
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    UMaterialInterface* DecalMaterial;

    // 구역 크기 조절용
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackIce")
    FVector DecalSize;

    // 표시 여부
    UFUNCTION(BlueprintCallable, Category = "BlackIce")
    void SetIceVisible(bool bVisible);

    // 크기 갱신
    UFUNCTION(BlueprintCallable, Category = "BlackIce")
    void UpdateDecalSize(FVector NewSize);
};