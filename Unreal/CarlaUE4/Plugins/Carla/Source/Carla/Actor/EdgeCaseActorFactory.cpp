// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "EdgeCaseActorFactory.h"
#include "BlackIceZone.h"
#include "CrossWindZone.h"

#include "Carla/Actor/ActorBlueprintFunctionLibrary.h"
#include "Carla/Actor/ActorSpawnResult.h"

// ──────────────────────────────────────────────────────────
// GetDefinitions: Python blueprint library에 등록될 정의 목록
// ──────────────────────────────────────────────────────────
TArray<FActorDefinition> AEdgeCaseActorFactory::GetDefinitions()
{
    UE_LOG(LogTemp, Warning, TEXT("[EdgeCaseFactory] GetDefinitions() CALLED"));
    
    TArray<FActorDefinition> Definitions;

    // ── BlackIce Zone ──
    {
        // MakeGenericDefinition은 FillIdAndTags를 내부 호출
        // → Id = "edgecase.zone.blackice"
        // → Tags = "edgecase,zone,blackice"
        // → role_name, ros_name 등 기본 Variation 자동 추가
        FActorDefinition Def =
            UActorBlueprintFunctionLibrary::MakeGenericDefinition(
                TEXT("edgecase"), TEXT("zone"), TEXT("blackice"));

        // Python에서 설정 가능한 파라미터 추가
        FActorVariation FrictionVar;
        FrictionVar.Id = TEXT("ice_friction_scale");
        FrictionVar.Type = EActorAttributeType::Float;
        FrictionVar.RecommendedValues = { TEXT("0.17") };
        FrictionVar.bRestrictToRecommended = false;
        Def.Variations.Emplace(FrictionVar);

        // TriggerBox 크기 (Python에서 조정 가능)
        FActorVariation ExtentX;
        ExtentX.Id = TEXT("extent_x");
        ExtentX.Type = EActorAttributeType::Float;
        ExtentX.RecommendedValues = { TEXT("180.0") };
        ExtentX.bRestrictToRecommended = false;
        Def.Variations.Emplace(ExtentX);

        FActorVariation ExtentY;
        ExtentY.Id = TEXT("extent_y");
        ExtentY.Type = EActorAttributeType::Float;
        ExtentY.RecommendedValues = { TEXT("90.0") };
        ExtentY.bRestrictToRecommended = false;
        Def.Variations.Emplace(ExtentY);

        FActorVariation ExtentZ;
        ExtentZ.Id = TEXT("extent_z");
        ExtentZ.Type = EActorAttributeType::Float;
        ExtentZ.RecommendedValues = { TEXT("80.0") };
        ExtentZ.bRestrictToRecommended = false;
        Def.Variations.Emplace(ExtentZ);

        bool bValid = UActorBlueprintFunctionLibrary::CheckActorDefinition(Def);
        UE_LOG(LogTemp, Warning, TEXT("[EdgeCaseFactory] blackice valid=%d, Id=%s"),
            bValid, *Def.Id);

        // Validation
        if (UActorBlueprintFunctionLibrary::CheckActorDefinition(Def))
        {
            Definitions.Add(Def);
        }
    }

    // ── CrossWind Zone ──
    {
        FActorDefinition Def =
            UActorBlueprintFunctionLibrary::MakeGenericDefinition(
                TEXT("edgecase"), TEXT("zone"), TEXT("crosswind"));

        FActorVariation MaxAccelVar;
        MaxAccelVar.Id = TEXT("max_lateral_accel");
        MaxAccelVar.Type = EActorAttributeType::Float;
        MaxAccelVar.RecommendedValues = { TEXT("90.0") };
        MaxAccelVar.bRestrictToRecommended = false;
        Def.Variations.Emplace(MaxAccelVar);

        FActorVariation SwayVar;
        SwayVar.Id = TEXT("sway_frequency");
        SwayVar.Type = EActorAttributeType::Float;
        SwayVar.RecommendedValues = { TEXT("0.15") };
        SwayVar.bRestrictToRecommended = false;
        Def.Variations.Emplace(SwayVar);

        FActorVariation ExtentX;
        ExtentX.Id = TEXT("extent_x");
        ExtentX.Type = EActorAttributeType::Float;
        ExtentX.RecommendedValues = { TEXT("2000.0") };
        ExtentX.bRestrictToRecommended = false;
        Def.Variations.Emplace(ExtentX);

        FActorVariation ExtentY;
        ExtentY.Id = TEXT("extent_y");
        ExtentY.Type = EActorAttributeType::Float;
        ExtentY.RecommendedValues = { TEXT("400.0") };
        ExtentY.bRestrictToRecommended = false;
        Def.Variations.Emplace(ExtentY);

        FActorVariation ExtentZ;
        ExtentZ.Id = TEXT("extent_z");
        ExtentZ.Type = EActorAttributeType::Float;
        ExtentZ.RecommendedValues = { TEXT("150.0") };
        ExtentZ.bRestrictToRecommended = false;
        Def.Variations.Emplace(ExtentZ);

        if (UActorBlueprintFunctionLibrary::CheckActorDefinition(Def))
        {
            Definitions.Add(Def);
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[EdgeCaseFactory] Returning %d definitions"),
        Definitions.Num());

    return Definitions;
}

// ──────────────────────────────────────────────────────────
// SpawnActor: 실제 C++ 액터 인스턴스 생성
// ──────────────────────────────────────────────────────────
FActorSpawnResult AEdgeCaseActorFactory::SpawnActor(
    const FTransform& SpawnAtTransform,
    const FActorDescription& ActorDescription)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[EdgeCaseFactory] No World!"));
        return FActorSpawnResult(nullptr);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedActor = nullptr;

    // ── BlackIce ──
    if (ActorDescription.Id == TEXT("edgecase.zone.blackice"))
    {
        ABlackIceZone* Ice = World->SpawnActor<ABlackIceZone>(
            ABlackIceZone::StaticClass(),
            SpawnAtTransform,
            SpawnParams);

        if (Ice)
        {
            // Python에서 보낸 Variation 값 적용
            // RetrieveActorAttributeToFloat(id, Description.Variations, default)
            Ice->IceFrictionScale =
                UActorBlueprintFunctionLibrary::RetrieveActorAttributeToFloat(
                    TEXT("ice_friction_scale"),
                    ActorDescription.Variations,
                    0.17f);

            // TriggerBox 크기 조정
            float Ex = UActorBlueprintFunctionLibrary::RetrieveActorAttributeToFloat(
                TEXT("extent_x"), ActorDescription.Variations, 180.0f);
            float Ey = UActorBlueprintFunctionLibrary::RetrieveActorAttributeToFloat(
                TEXT("extent_y"), ActorDescription.Variations, 90.0f);
            float Ez = UActorBlueprintFunctionLibrary::RetrieveActorAttributeToFloat(
                TEXT("extent_z"), ActorDescription.Variations, 80.0f);

            if (Ice->TriggerBox)
            {
                Ice->TriggerBox->SetBoxExtent(FVector(Ex, Ey, Ez));
            }

            UE_LOG(LogTemp, Warning,
                TEXT("[EdgeCaseFactory] Spawned BlackIceZone at %s, friction=%.2f"),
                *SpawnAtTransform.GetLocation().ToString(),
                Ice->IceFrictionScale);
        }

        SpawnedActor = Ice;
    }
    // ── CrossWind ──
    else if (ActorDescription.Id == TEXT("edgecase.zone.crosswind"))
    {
        ACrossWindZone* Wind = World->SpawnActor<ACrossWindZone>(
            ACrossWindZone::StaticClass(),
            SpawnAtTransform,
            SpawnParams);

        if (Wind)
        {
            Wind->MaxLateralAccel =
                UActorBlueprintFunctionLibrary::RetrieveActorAttributeToFloat(
                    TEXT("max_lateral_accel"),
                    ActorDescription.Variations,
                    90.0f);

            Wind->SwayFrequency =
                UActorBlueprintFunctionLibrary::RetrieveActorAttributeToFloat(
                    TEXT("sway_frequency"),
                    ActorDescription.Variations,
                    0.15f);

            UE_LOG(LogTemp, Warning,
                TEXT("[EdgeCaseFactory] Spawned CrossWindZone at %s, accel=%.1f"),
                *SpawnAtTransform.GetLocation().ToString(),
                Wind->MaxLateralAccel);
        }

        SpawnedActor = Wind;
    }

    return FActorSpawnResult(SpawnedActor);
}
