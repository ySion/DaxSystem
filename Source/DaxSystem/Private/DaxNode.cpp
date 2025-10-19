#include "DaxSystem/Private/DaxNode.h"

using namespace ArzDax;

FName FDaxNode::GetTypeName() {
    
    if (IsEmpty()) return "Empty";
    
    if (Value.IsType<FDaxSmallValue>()) {
        const UScriptStruct* SS = Value.Get<FDaxSmallValue>().GetScriptStruct();
        return SS ? SS->GetFName() : "InvalidSmallValue";
    }
    
    if (Value.IsType<FDaxHeapValue>()) {
        const UScriptStruct* SS = Value.Get<FDaxHeapValue>().GetScriptStruct();
        return SS ? SS->GetFName() : "InvalidHeapValue";
    }
    
    if (IsArray()) return "Array";
    
    if (IsMap()) return "Map";
    
    return "UnknownType";
}

bool FDaxNode::Identical(const FDaxNode* Other, uint32 PortFlags) const {
    if (this == Other) return true;
    if (!Other) return false;
    const auto Index = Value.GetIndex();
    if (Index != Other->Value.GetIndex()) return false;

    switch (Index) {
        case 0: return true; // FDaxEmpty
        case 1: // FDaxSmallValue
            return Value.Get<FDaxSmallValue>().Identical(Other->Value.TryGet<FDaxSmallValue>(), PortFlags);
        case 2: // FDaxHeapValue
            return Value.Get<FDaxHeapValue>().Identical(Other->Value.TryGet<FDaxHeapValue>(), PortFlags);
        case 3: // FDaxArray
            return Value.Get<FDaxArray>().Identical(Other->Value.TryGet<FDaxArray>(), PortFlags);
        case 4: // FDaxMap
            return Value.Get<FDaxMap>().Identical(Other->Value.TryGet<FDaxMap>(), PortFlags);
        default: break;
    }
    return false;
}