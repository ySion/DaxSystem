#pragma once

#include "DaxSystem/Basic/DaxNodeID.h"

namespace ArzDax {
    struct FDaxArray { // 48B
        using ChildrenType = TArray<FDaxNodeID, TInlineAllocator<8>>;
        ChildrenType Children;
        FDaxArray() = default;
        FDaxArray(const FDaxArray& other) : Children(other.Children) {};
        FDaxArray(FDaxArray&& other) : Children(MoveTemp(other.Children)) {};

        FDaxArray& operator=(const FDaxArray& other) {
            if (this != &other) Children = other.Children;
            return *this;
        }

        FDaxArray& operator=(FDaxArray&& other) {
            if (this != &other) Children = MoveTemp(other.Children);
            return *this;
        }

        bool Identical(const FDaxArray* Other, uint32 PortFlags) const {
            if (this == Other) return true;
            if (!Other) return false;
            return Children == Other->Children;
        }
    };
}
