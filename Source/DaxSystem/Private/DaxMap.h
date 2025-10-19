#pragma once
#include "DaxSystem/Private/ThirdPart/udm.h"
#include "DaxSystem/Private/DaxStdAllocatorGlue.h"
#include "DaxSystem/Private/DaxNodeID.h"
#include "DaxSystem/Private/DaxCommon.h"

namespace ArzDax {
    using FDaxMapType = ankerl::unordered_dense::map<FName, FDaxNodeID, ArzDax::DaxFNameHash, ArzDax::DaxFNameEqual, TDaxAllocator<std::pair<FName, FDaxNodeID>>>;

    struct FDaxMap {
        TUniquePtr<FDaxMapType> MapPtr = nullptr;

        FDaxMapType& Children() {
            checkf(MapPtr.IsValid(), TEXT("FDaxMap::Children: MapPtr is nullptr, WTF?"));
            return *MapPtr;
        }

        const FDaxMapType& Children() const {
            checkf(MapPtr.IsValid(), TEXT("FDaxMap::Children const: MapPtr is nullptr, WTF?"));
            return *MapPtr;
        }

        FDaxMap() {
            MapPtr = TUniquePtr<FDaxMapType>(new FDaxMapType());
        }

        FDaxMap(const FDaxMap& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap: copy: Other.MapPtr is nullptr, WTF?"));
            MapPtr = TUniquePtr<FDaxMapType>(new FDaxMapType(*Other.MapPtr));
        };

        FDaxMap(FDaxMap&& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap: move: Other.MapPtr is nullptr, WTF?"));
            MapPtr = MoveTemp(Other.MapPtr);
        };

        FDaxMap& operator=(const FDaxMap& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap::operator= copy: Other.MapPtr is nullptr, WTF?"));
            if (this != &Other) MapPtr = TUniquePtr<FDaxMapType>(new FDaxMapType(*Other.MapPtr));
            return *this;
        }

        FDaxMap& operator=(FDaxMap&& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap::operator= move: Other.MapPtr is nullptr, WTF?"));
            if (this != &Other) MapPtr = MoveTemp(Other.MapPtr);
            return *this;
        }

        bool Identical(const FDaxMap* Other, uint32 /*PortFlags*/) const {
            if (this == Other) return true;
            if (!Other) return false;
            checkf(MapPtr.IsValid() && Other->MapPtr.IsValid(), TEXT("FDaxMap::Identical: MapPtr null"));
            return Children() == Other->Children();
        }
    };
}
