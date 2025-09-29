#pragma once

#include "CoreMinimal.h"

struct FDaxNodeID {
    static constexpr uint16 InvalidIndex = 0xFFFF; // 最多支持 65535 个节点 (0 - 65534), 65535这个ID不可用

    uint16 Index = InvalidIndex;
    uint16 Generation = InvalidIndex;

    FDaxNodeID() = default;

    explicit FDaxNodeID(uint16 InIndex, uint16 InGen) : Index(InIndex), Generation(InGen) {}

    FDaxNodeID(const FDaxNodeID& Other) : Index(Other.Index), Generation(Other.Generation) {}

    FDaxNodeID(FDaxNodeID&& Other) {
        Index = Other.Index;
        Generation = Other.Generation;
        Other.Index = InvalidIndex;
        Other.Generation = InvalidIndex;
    }

    FDaxNodeID& operator=(FDaxNodeID&& Other) {
        if (this == &Other) return *this;
        Index = Other.Index;
        Generation = Other.Generation;
        Other.Index = InvalidIndex;
        Other.Generation = InvalidIndex;
        return *this;
    }

    FDaxNodeID& operator=(const FDaxNodeID& Other) {
        if (this == &Other) return *this;
        Index = Other.Index;
        Generation = Other.Generation;
        return *this;
    }

    FORCEINLINE bool IsValid() const { return Index != InvalidIndex; }

    bool operator==(const FDaxNodeID Other) const { return Index == Other.Index && Generation == Other.Generation; }

    friend uint32 GetTypeHash(const FDaxNodeID ID) { return (static_cast<uint32>(ID.Index) << 16) | static_cast<uint32>(ID.Generation); }

    FString ToString() const;

    friend FArchive& operator<<(FArchive& Ar, FDaxNodeID& ID);
};

namespace ArzDax {
    struct FDaxNodeIDHash {
        using is_avalanching = void; //高质量的哈希函数
        auto operator()(const FDaxNodeID ID) const noexcept {
            return (static_cast<uint32>(ID.Index) << 16) | static_cast<uint32>(ID.Generation);
        }
    };

    struct FDaxNodeIDEqual {
        auto operator()(const FDaxNodeID A, const FDaxNodeID B) const noexcept {
            return A == B;
        }
    };
}