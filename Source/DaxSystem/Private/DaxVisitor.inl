#pragma once

#include "DaxNodeID.h"
#include "DaxSet.h"
#include "DaxSystem/Private/DaxNode.h"

template <typename T>
FDaxVisitor FDaxVisitor::SearchChildBy(T PredictFunc) const {
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (auto Arr = CachedNode->GetArray()) {
        for (int32 i = 0; i < Arr->Num(); ++i) {
            const FDaxNodeID ChildID = (*Arr)[i];
            if (auto* ChildNode = TargetSet->TryGetNode(ChildID)) {
                if (PredictFunc(ChildNode)) {
                    return MakeVisitorByIndex(i);
                }
            }
        }
    }
    else if (auto Map = CachedNode->GetMap()) {
        for (const auto& [Key, ChildID] : *Map) {
            if (auto* ChildNode = TargetSet->TryGetNode(ChildID)) {
                if (PredictFunc(ChildNode)) {
                    return MakeVisitorByName(Key);
                }
            }
        }
    }
    return {};
}
