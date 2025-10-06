#pragma once

#include "CoreMinimal.h"
#include "DaxAux.h"
#include "DaxBuiltinTypes.generated.h"

// 这些是为 DataX System 内置的小型值类型提供的轻量包装
// 目的：避免上层为基础类型手写 USTRUCT，同时复用现有 StructView/TrySetValue 流程


USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinBool {
    GENERATED_BODY()

    UPROPERTY()
    bool Value = false;
};

USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinInt64 {
    GENERATED_BODY()

    UPROPERTY()
    int64 Value = 0;

    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) {
        ArzDax::ZigZagInt(Ar, Value);
        bOutSuccess = true;
        return true;
    }
};

template <>
struct TStructOpsTypeTraits<FDaxBuiltinInt64> : public TStructOpsTypeTraitsBase2<FDaxBuiltinInt64> {
    enum { WithNetSerializer = true };
};

USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinFloat {
    GENERATED_BODY()

    UPROPERTY()
    float Value = 0;
};

USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinName {
    GENERATED_BODY()

    UPROPERTY()
    FName Value{};
};

USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinString {
    GENERATED_BODY()

    UPROPERTY()
    FString Value{};
};

USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinVector {
    GENERATED_BODY()

    UPROPERTY()
    FVector Value{};
};

USTRUCT()
struct DAXSYSTEM_API FDaxBuiltinRotator {
    GENERATED_BODY()

    UPROPERTY()
    FRotator Value{};
};

