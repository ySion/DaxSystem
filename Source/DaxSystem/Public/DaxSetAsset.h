#pragma once

#include "CoreMinimal.h"
#include "DaxSystem/Private/DaxSet.h"
#include "DaxSetAsset.generated.h"

UCLASS(BlueprintType)
class DAXSYSTEM_API UDaxSetAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY()
    FDaxSet DaxSet {};
};
