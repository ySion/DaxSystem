#pragma once

#include "Templates/MakeUnsigned.h"
namespace ArzDax {
    template <typename TInt>
    void ZigZagInt(FArchive& Ar, TInt& Value) {
        using TUInt = typename TMakeUnsigned<TInt>::Type;

        if (Ar.IsSaving()) {
            TUInt ZigZag = (static_cast<TUInt>(Value) << 1) ^ static_cast<TUInt>(Value >> (sizeof(TInt) * 8 - 1));

            if constexpr (sizeof(TInt) <= 4) {
                uint32 Temp = static_cast<uint32>(ZigZag);
                Ar.SerializeIntPacked(Temp);
            } else {
                uint64 Temp = static_cast<uint64>(ZigZag);
                Ar.SerializeIntPacked64(Temp);
            }
        } else {
            if constexpr (sizeof(TInt) <= 4) {
                uint32 ZigZag;
                Ar.SerializeIntPacked(ZigZag);
                Value = static_cast<TInt>((ZigZag >> 1) ^ -static_cast<TInt>(ZigZag & 1));
            } else {
                uint64 ZigZag;
                Ar.SerializeIntPacked64(ZigZag);
                Value = static_cast<TInt>((ZigZag >> 1) ^ -static_cast<TInt>(ZigZag & 1));
            }
        }
    }
}