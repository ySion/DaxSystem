#pragma once

#include "CoreMinimal.h"
#include "StructUtils/StructView.h"
#include "UObject/Object.h"

struct DAXSYSTEM_API FDaxStructView {
  
    FConstStructView Data {};

    static void ConstructHandle(FDaxStructView* Handle) {
        new(Handle) FDaxStructView{};
    }

    static void CopyConstructHandle(FDaxStructView* Handle, const FDaxStructView* Other) {
        new(Handle) FDaxStructView{ Other->Data };
    }

    static FDaxStructView* AssignHandle(FDaxStructView* Handle, const FDaxStructView* Other) {
        Handle->Data = Other->Data;
        return Handle;
    }
};