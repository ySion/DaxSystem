#include "DaxSystem/Private/DaxNodeID.h"

FString FDaxNodeID::ToString() const {
    return FString::Printf(TEXT("ID[%d:%d]"), Index, Generation);
}

FArchive& operator<<(FArchive& Ar, FDaxNodeID& ID) {
    if (Ar.IsSaving()) {
        uint32 Idx = ID.Index;
        uint32 Gen = ID.Generation;
        Ar.SerializeIntPacked(Idx);
        Ar.SerializeIntPacked(Gen);
    } else {
        uint32 Idx = 0, Gen = 0;
        Ar.SerializeIntPacked(Idx);
        Ar.SerializeIntPacked(Gen);
        ID.Index = static_cast<uint16>(Idx);
        ID.Generation = static_cast<uint16>(Gen);
    }
    return Ar;
}
