#include "DaxSet.h"

#include "DaxComponent.h"
#include "DaxSystem/Node/DaxNode.h"

using namespace ArzDax;

FDaxSet::FDaxSet() {
    LiveToken = MakeShared<uint8>(0);
    RootID = Allocator.Allocate();
    BumpNodeDataVersionAndStruct(RootID);
}

FDaxSet::~FDaxSet() {
    if (GlobalSetID != 0)
        FDaxGlobalSetManager::UnregisterOnClient(this);
}

FDaxSet::FDaxSet(const FDaxSet& Other) {
    LiveToken = MakeShared<uint8>(0);
    CopySet(Other);
}

FDaxSet& FDaxSet::operator=(const FDaxSet& Other) {
    if (this != &Other) CopySet(Other);
    return *this;
}

FString FDaxSet::GetString() const {
    FString Out;
    Out += FString::Printf(TEXT("DaxSet(ID=%u) DataVer=%u StructVer=%u Nodes=%u\n"),
                           GlobalSetID, DataVersion, StructVersion, Allocator.GetCurrentActive());

    auto IndentOf = [](int32 Depth) {
        FString S;
        S.Reserve(Depth * 2);
        for (int32 i = 0; i < Depth; ++i) S += TEXT("   ");
        return S;
    };

    auto ValueToText = [](const FConstStructView& SV) -> FString {
        if (!SV.IsValid()) return TEXT("<invalid>");
        const UScriptStruct* SS = SV.GetScriptStruct();
        if (!SS) return TEXT("<null-type>");
        FString Txt;
        SS->ExportText(Txt, SV.GetMemory(), nullptr, nullptr, PPF_None, nullptr);
        Txt.ReplaceInline(TEXT("\n"), TEXT(" "));
        return Txt;
    };

    TFunction<void(const FDaxNodeID, int32, const FString&)> DumpNode;
    DumpNode = [&](const FDaxNodeID ID, int32 Depth, const FString& Label) {
        const FString Indent = IndentOf(Depth);
        if (!Allocator.IsNodeValid(ID)) {
            Out += FString::Printf(TEXT("%s- %s %s <invalid>\n"), *Indent, Label.Len() ? *Label : TEXT("(root)"),
                                   *ID.ToString());
            return;
        }

        const ArzDax::FDaxNode* Node = Allocator.TryGetNode(ID);
        const auto Common = Allocator.GetCommonInfo(ID);
        const UScriptStruct* Type = Common.ValueType;
        const uint32 Ver = Common.Version;

        FString TypeName = TEXT("Empty");
        if (Type == FDaxFakeTypeArray::StaticStruct()) TypeName = TEXT("Array");
        else if (Type == FDaxFakeTypeMap::StaticStruct()) TypeName = TEXT("Map");
        else if (Type == FDaxFakeTypeEmpty::StaticStruct()) TypeName = TEXT("Empty");
        else if (Type == nullptr) TypeName = TEXT("NULL");
        else TypeName = Type->GetName();

        const FString LabelText = Label.Len() ? Label : TEXT("(root)");

        if (!Node) {
            Out += FString::Printf(TEXT("%s- %s %s\t<no-node>\n"), *Indent, *LabelText, *TypeName);
            return;
        }

        if (Type == FDaxFakeTypeArray::StaticStruct()) {
            const auto* Arr = Node->GetArray();
            const int32 Count = Arr ? Arr->Num() : 0;
            Out += FString::Printf(TEXT("%s- %s Array(count=%d)\n"), *Indent, *LabelText, Count);
            if (Arr) {
                for (int32 i = 0; i < Count; ++i) {
                    DumpNode((*Arr)[i], Depth + 1, FString::Printf(TEXT("[%d]"), i));
                }
            }
        }
        else if (Type == FDaxFakeTypeMap::StaticStruct()) {
            const auto* Map = Node->GetMap();
            const int32 Count = Map ? static_cast<int32>(Map->size()) : 0;
            Out += FString::Printf(TEXT("%s- %s Map(count=%d)\n"), *Indent, *LabelText, Count);
            if (Map) {
                for (const auto& KV : *Map) {
                    const FString K = KV.first.ToString();
                    DumpNode(KV.second, Depth + 1, K);
                }
            }
        }
        else if (Type == FDaxFakeTypeEmpty::StaticStruct() || Type == nullptr) {
            Out += FString::Printf(TEXT("%s- %s Empty\n"), *Indent, *LabelText);
        }
        else {
            const FConstStructView SV = Node->TryGetValueGeneric();
            FString VTxt = ValueToText(SV);
            Out += FString::Printf(TEXT("%s- %s %s\t=%s\n"), *Indent, *LabelText, *TypeName, *VTxt);
        }
    };

    if (RootID.IsValid()) {
        DumpNode(RootID, 0, TEXT("Root"));
    }
    else {
        Out += TEXT("<Empty Root>\n");
    }

    return Out;
}

FString FDaxSet::GetStringDebug() const {
    FString Out;
    // 头部：全局ID、版本与分配器统计
    Out += FString::Printf(
        TEXT(
            "DaxSet(ID=%u) DataVer=%u StructVer=%u \n Allocator { TotalAlloc=%u, TotalFree=%u, Current=%u, Peak=%u, Chunks=%u, FreeRemain=%u }\n"),
        GlobalSetID,
        DataVersion,
        StructVersion,
        Allocator.GetTotalAllocated(),
        Allocator.GetTotalDeallocated(),
        Allocator.GetCurrentActive(),
        Allocator.GetPeakActive(),
        Allocator.GetChunkCount(),
        Allocator.GetFreeRemaining());

    auto IndentOf = [](int32 Depth) {
        FString S;
        S.Reserve(Depth * 3);
        for (int32 i = 0; i < Depth; ++i) S += TEXT("   ");
        return S;
    };

    auto ValueToText = [](const FConstStructView& SV) -> FString {
        if (!SV.IsValid()) return TEXT("<invalid>");
        const UScriptStruct* SS = SV.GetScriptStruct();
        if (!SS) return TEXT("<null-type>");
        FString Txt;
        SS->ExportText(Txt, SV.GetMemory(), nullptr, nullptr, PPF_None, nullptr);
        Txt.ReplaceInline(TEXT("\n"), TEXT(" "));
        return Txt;
    };

    TFunction<void(const FDaxNodeID, int32, const FString&)> DumpNode;
    DumpNode = [&](const FDaxNodeID ID, int32 Depth, const FString& Label) {
        const FString Indent = IndentOf(Depth);
        if (!Allocator.IsNodeValid(ID)) {
            Out += FString::Printf(TEXT("%s- %s %s <invalid>\n"), *Indent, Label.Len() ? *Label : TEXT("(root)"),
                                   *ID.ToString());
            return;
        }

        const ArzDax::FDaxNode* Node = Allocator.TryGetNode(ID);
        const auto Common = Allocator.GetCommonInfo(ID);
        const UScriptStruct* Type = Common.ValueType;
        const uint32 Ver = Common.Version;

        FString TypeName = TEXT("Empty");
        if (Type == FDaxFakeTypeArray::StaticStruct()) TypeName = TEXT("Array");
        else if (Type == FDaxFakeTypeMap::StaticStruct()) TypeName = TEXT("Map");
        else if (Type == FDaxFakeTypeEmpty::StaticStruct()) TypeName = TEXT("Empty");
        else if (Type == nullptr) TypeName = TEXT("NULL");
        else TypeName = Type->GetName();

        const FString LabelText = Label.Len() ? Label : TEXT("(root)");

        if (!Node) {
            Out += FString::Printf(
                TEXT("%s- %s %s\t[ID=%s Ver=%u] <no-node>\n"), *Indent, *LabelText, *TypeName, *ID.ToString(), Ver);
            return;
        }

        if (Type == FDaxFakeTypeArray::StaticStruct()) {
            const auto* Arr = Node->GetArray();
            const int32 Count = Arr ? Arr->Num() : 0;
            Out += FString::Printf(
                TEXT("%s- %s Array(count=%d) [ID=%s Ver=%u]\n"), *Indent, *LabelText, Count, *ID.ToString(), Ver);
            if (Arr) {
                for (int32 i = 0; i < Count; ++i) {
                    DumpNode((*Arr)[i], Depth + 1, FString::Printf(TEXT("[%d]"), i));
                }
            }
        }
        else if (Type == FDaxFakeTypeMap::StaticStruct()) {
            const auto* Map = Node->GetMap();
            const int32 Count = Map ? static_cast<int32>(Map->size()) : 0;
            Out += FString::Printf(
                TEXT("%s- %s Map(count=%d) [ID=%s Ver=%u]\n"), *Indent, *LabelText, Count, *ID.ToString(), Ver);
            if (Map) {
                for (const auto& KV : *Map) {
                    const FString K = KV.first.ToString();
                    DumpNode(KV.second, Depth + 1, K);
                }
            }
        }
        else if (Type == FDaxFakeTypeEmpty::StaticStruct() || Type == nullptr) {
            Out += FString::Printf(TEXT("%s- %s Empty [ID=%s Ver=%u]\n"), *Indent, *LabelText, *ID.ToString(), Ver);
        }
        else {
            const FConstStructView SV = Node->TryGetValueGeneric();
            const FString VTxt = ValueToText(SV);
            Out += FString::Printf(
                TEXT("%s- %s %s\t=%s [ID=%s Ver=%u]\n"), *Indent, *LabelText, *TypeName, *VTxt, *ID.ToString(), Ver);
        }
    };

    if (RootID.IsValid()) {
        DumpNode(RootID, 0, TEXT("Root"));
    }
    else {
        Out += TEXT("<Empty Root>\n");
    }

    return Out;
}

bool FDaxSet::IsNetRegistered() const {
    return FDaxGlobalSetManager::TryGetDaxSetInfo(GlobalSetID).IsValid();
}

bool FDaxSet::IsRemainingSpaceSupportCopy(const FDaxSet& Source, const FDaxNodeID SourceID) const {
    if (!Source.IsNodeValid(SourceID)) return false;
    return Allocator.GetFreeRemaining() > Source.GetNodeNumRecursive(SourceID);
}

bool FDaxSet::IsRemainingSpaceSupportInnerSwap(const FDaxNodeID A, const FDaxNodeID B) const {
    if (!IsNodeValid(A)) return false;
    if (!IsNodeValid(B)) return false;
    return Allocator.GetFreeRemaining() > (GetNodeNumRecursive(A) + GetNodeNumRecursive(B));
}

FDaxNodeID FDaxSet::DeepCopyNode(FDaxSet& Source, const FDaxNodeID SourceID) {
    if (!IsRemainingSpaceSupportCopy(Source, SourceID)) return FDaxNodeID();
    int CopyCount = 0;
    auto RetNode = DeepCopyNodeImpl(Source, SourceID, CopyCount);
    if (CopyCount > 0) BumpStructVersion();
    return RetNode;
}

FDaxNodeID FDaxSet::DeepCopyNodeImpl(FDaxSet& Source, const FDaxNodeID SourceID, int& CopyCount) {
    auto SrcNode = Source.TryGetNode(SourceID);
    if (!SrcNode) return FDaxNodeID();

    // 分配新节点（此时 Version 已在槽分配时自增一次，Parent/ValueType 为默认值）
    auto ThisNewNodeID = Allocator.Allocate();
    auto ThisNewNode = TryGetNode(ThisNewNodeID);
    if (!ThisNewNode) return FDaxNodeID();

    if (SrcNode->IsMap()) {
        // Map：重建容器与子树，并同步 ValueType/Parent/版本
        ThisNewNode->ResetToEmptyMap();
        Allocator.UpdateValueType(ThisNewNodeID, FDaxFakeTypeMap::StaticStruct());

        if (auto* SourceMapData = SrcNode->GetMap()) {
            if (auto* NewMapData = ThisNewNode->GetMap()) {
                for (auto& KV : *SourceMapData) {
                    const FName& K = KV.first;
                    const FDaxNodeID SrcChildID = KV.second;
                    auto NewChildID = DeepCopyNodeImpl(Source, SrcChildID, CopyCount);
                    if (NewChildID.IsValid()) {
                        NewMapData->emplace(K, NewChildID);
                        if (auto* PRef = Allocator.GetParentRef(NewChildID)) *PRef = ThisNewNodeID;
                        Allocator.UpdateParentEdgeMap(NewChildID, K);
                    }
                }
            }
        }
        BumpOnlyNodeDataVersion(ThisNewNodeID);
        CopyCount++;
    }
    else if (SrcNode->IsArray()) {
        // Array：重建容器与子树，并同步 ValueType/Parent/版本
        ThisNewNode->ResetToEmptyArray();
        Allocator.UpdateValueType(ThisNewNodeID, FDaxFakeTypeArray::StaticStruct());

        if (auto* SourceListData = SrcNode->GetArray()) {
            if (auto* NewListData = ThisNewNode->GetArray()) {
                NewListData->Empty(SourceListData->Num());
                int32 __dax_idx = 0;
                for (FDaxNodeID ChildID : *SourceListData) {
                    auto NewChildID = DeepCopyNodeImpl(Source, ChildID, CopyCount);
                    if (NewChildID.IsValid()) {
                        NewListData->Add(NewChildID);
                        if (auto* PRef = Allocator.GetParentRef(NewChildID)) *PRef = ThisNewNodeID;
                        Allocator.UpdateParentEdgeArray(NewChildID, (uint16)__dax_idx);
                        ++__dax_idx;
                    }
                }
            }
        }
        BumpOnlyNodeDataVersion(ThisNewNodeID);
        CopyCount++;
    }
    else if (SrcNode->IsValue()) {
        // Value：复制值语义，并同步实际 ScriptStruct 类型
        *ThisNewNode = *SrcNode;
        auto SV = ThisNewNode->TryGetValueGeneric();
        const UScriptStruct* SS = SV.GetScriptStruct();
        if (SS) {
            Allocator.UpdateValueType(ThisNewNodeID, SS);
        }
        else {
            // 防御：若出现异常，降级为空
            ThisNewNode->ResetToEmpty();
            Allocator.UpdateValueType(ThisNewNodeID, FDaxFakeTypeEmpty::StaticStruct());
        }
        BumpOnlyNodeDataVersion(ThisNewNodeID);
        CopyCount++;
    }
    else {
        // Empty：显式同步为空类型
        ThisNewNode->ResetToEmpty();
        Allocator.UpdateValueType(ThisNewNodeID, FDaxFakeTypeEmpty::StaticStruct());
        BumpOnlyNodeDataVersion(ThisNewNodeID);
        CopyCount++;
    }

    return ThisNewNodeID;
}

void FDaxSet::CopySet(const FDaxSet& Other) {
    if (&Other == this) return;
    if (!Other.RootID.IsValid()) return;
    Allocator.Reset();
    RootID = DeepCopyNode(const_cast<FDaxSet&>(Other), Other.RootID);
}

bool FDaxSet::CopyNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID) {
    // 自拷贝无效
    if (&SrcSet == this && Target == SrcSetNodeID) return false;
    // 基本有效性
    if (!IsNodeValid(Target) || !SrcSet.IsNodeValid(SrcSetNodeID)) return false;

    // 空间检查：需要先分配完整副本，再替换 Target
    if (!IsRemainingSpaceSupportCopy(SrcSet, SrcSetNodeID)) return false;

    // 深拷贝出一棵新子树（包含 ValueType/Parent/Version 的一致性）
    const FDaxNodeID NewID = DeepCopyNode(const_cast<FDaxSet&>(SrcSet), SrcSetNodeID);
    if (!NewID.IsValid()) return false;

    // 将 Target 在其父容器中重定向到 NewID，并释放旧 Target 子树
    const bool RedirectOk = RedirectNode(Target, NewID);
    if (!RedirectOk) {
        // 回退：释放新子树
        ReleaseRecursive(NewID);
        return false;
    }
    return true;
}

bool FDaxSet::SwapNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID) {
    if (this == &SrcSet && Target == SrcSetNodeID) return false; // 自交换无效
    if (!IsNodeValid(Target) || !SrcSet.IsNodeValid(SrcSetNodeID)) return false; // 节点有效性检查
    if (IsNodeAncestorOrDescendant(Target, SrcSetNodeID)) return false; // 祖先/后代节点禁止交换
    if (this == &SrcSet) {
        // 空间充足性检查
        if (!IsRemainingSpaceSupportInnerSwap(Target, SrcSetNodeID)) return false; // 操作都会走统一的权威copy流程, 禁止任何形式的move
    }
    else {
        if (!IsRemainingSpaceSupportCopy(SrcSet, SrcSetNodeID)) return false;
        if (!SrcSet.IsRemainingSpaceSupportCopy(*this, Target)) return false;
    }

    const FDaxNodeID ThisOldID = Target;
    const FDaxNodeID SrcOldID = SrcSetNodeID;

    const FDaxNodeID ThisNewID = DeepCopyNode(const_cast<FDaxSet&>(SrcSet), SrcOldID); // 自己这里拷贝对面的
    const FDaxNodeID SrcNewID = const_cast<FDaxSet&>(SrcSet).DeepCopyNode(*this, ThisOldID); // 对面拷贝自己的

    const bool ThisResult = this->RedirectNode(ThisOldID, ThisNewID);
    const bool SrcResult = const_cast<FDaxSet&>(SrcSet).RedirectNode(SrcOldID, SrcNewID);

    if (!ThisResult || !SrcResult) {
        checkf(false, TEXT("FDaxSet::SwapNode 回退分支不应该被触发"));
        if (ThisResult) this->RedirectNode(ThisNewID, ThisOldID);
        if (SrcResult) const_cast<FDaxSet&>(SrcSet).RedirectNode(SrcNewID, SrcOldID);
        if (ThisNewID.IsValid()) this->ReleaseRecursive(ThisNewID);
        if (SrcNewID.IsValid()) const_cast<FDaxSet&>(SrcSet).ReleaseRecursive(SrcNewID);
        return false;
    }

    this->ReleaseRecursive(ThisOldID);
    const_cast<FDaxSet&>(SrcSet).ReleaseRecursive(SrcOldID);
    return true;
}

bool FDaxSet::MoveNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID) {
    if (this == &SrcSet && Target == SrcSetNodeID) return false;
    if (!IsNodeValid(Target) || !SrcSet.IsNodeValid(SrcSetNodeID)) return false;
    if (IsNodeAncestorOrDescendant(Target, SrcSetNodeID)) return false;
    if (this == &SrcSet) {
        if (!IsRemainingSpaceSupportCopy(SrcSet, SrcSetNodeID)) return false;
    }
    else {
        if (!IsRemainingSpaceSupportCopy(SrcSet, SrcSetNodeID)) return false;
        if (!SrcSet.IsRemainingSpaceSupportCopy(*this, Target)) return false;
    }

    const FDaxNodeID ThisNewID = DeepCopyNode(const_cast<FDaxSet&>(SrcSet), SrcSetNodeID);
    const bool ThisResult = this->RedirectNode(Target, ThisNewID);
    if (!ThisResult) {
        checkf(false, TEXT("FDaxSet::MoveNode 回退分支不应该被触发"));
        if (ThisResult) this->RedirectNode(ThisNewID, Target);
        if (ThisNewID.IsValid()) this->ReleaseRecursive(ThisNewID);
        return false;
    }

    this->ReleaseRecursive(Target);
    const_cast<FDaxSet&>(SrcSet).ReleaseRecursive(SrcSetNodeID);
    return true;
}

FDaxResultDetail FDaxSet::ResetToEmpty(const FDaxNodeID ID) {
    if (!Allocator.IsNodeValid(ID)) return EDaxResult::InvalidNode;
    ReleaseChildren(ID);
    if (auto* Node = Allocator.TryGetNode(ID)) {
        const EDaxResult Res = Node->ResetToEmpty();
        if (Res == EDaxResult::SameValueNotChange)
            return Res;

        if (Res == EDaxResult::SuccessOverrideEmpty) {
            Allocator.UpdateValueType(ID, FDaxFakeTypeEmpty::StaticStruct());
            BumpNodeDataVersion(ID);
            return Res;
        }

        Allocator.UpdateValueType(ID, FDaxFakeTypeEmpty::StaticStruct());
        BumpNodeDataVersionAndStruct(ID);
        return Res;
    }
    return EDaxResult::InvalidNode;
}

FDaxResultDetail FDaxSet::ResetToEmptyArray(const FDaxNodeID ID) {
    if (!Allocator.IsNodeValid(ID)) return EDaxResult::InvalidNode;
    ReleaseChildren(ID);
    if (auto* Node = Allocator.TryGetNode(ID)) {
        EDaxResult Res = Node->ResetToEmptyArray();
        if (Res == EDaxResult::SameValueNotChange)
            return Res;

        if (Res == EDaxResult::SuccessChangeValue) {
            BumpNodeDataVersionAndStruct(ID);
            return Res;
        }

        if (Res == EDaxResult::SuccessOverrideEmpty) {
            Allocator.UpdateValueType(ID, FDaxFakeTypeArray::StaticStruct());
            BumpNodeDataVersion(ID);
            return Res;
        }

        Allocator.UpdateValueType(ID, FDaxFakeTypeArray::StaticStruct());
        BumpNodeDataVersionAndStruct(ID);
        return Res;
    }
    return EDaxResult::InvalidNode;
}

FDaxResultDetail FDaxSet::ResetToEmptyMap(const FDaxNodeID ID) {
    if (!Allocator.IsNodeValid(ID)) return EDaxResult::InvalidNode;
    ReleaseChildren(ID);
    if (auto* Node = TryGetNode(ID)) {
        EDaxResult Res = Node->ResetToEmptyMap();
        if (Res == EDaxResult::SameValueNotChange)
            return Res;

        if (Res == EDaxResult::SuccessChangeValue) {
            BumpNodeDataVersionAndStruct(ID);
            return Res;
        }

        if (Res == EDaxResult::SuccessOverrideEmpty) {
            Allocator.UpdateValueType(ID, FDaxFakeTypeMap::StaticStruct());
            BumpNodeDataVersion(ID);
            return Res;
        }

        Allocator.UpdateValueType(ID, FDaxFakeTypeMap::StaticStruct());
        BumpNodeDataVersionAndStruct(ID);
        return Res;
    }
    return EDaxResult::InvalidNode;
}

void FDaxSet::BumpDataVersion() {
    if (!bRunningOnServer) return;
    ++DataVersion;
    if (ParentComponent.IsValid()) {
        ParentComponent->MarkDirty();
    }
}

void FDaxSet::BumpStructVersion() {
    if (!bRunningOnServer) return;
    ++StructVersion;
    ++DataVersion;
    if (ParentComponent.IsValid()) {
        ParentComponent->MarkDirty();
    }
}

bool FDaxSet::RedirectNode(const FDaxNodeID Old, const FDaxNodeID New) {
    if (Old == New) return false;
    if (!IsNodeValid(Old) || !IsNodeValid(New)) return false;

    if (Old == RootID) {
        RootID = New;
        ReleaseRecursive(Old);
    }
    else {
        auto Parent = GetNodeParent(Old);
        auto ParentNode = TryGetNode(Parent);
        if (!ParentNode) return false; // 不应该执行这里

        if (ParentNode->IsArray()) {
            auto* ArrayData = ParentNode->GetArray();
            bool bDone = false;
            if (ArrayData) {
                const auto Kind = Allocator.GetParentEdgeKind(Old);
                if (Kind == ArzDax::EDaxParentEdgeKind::Array) {
                    const uint16 Idx = Allocator.GetParentEdgeIndex(Old);
                    if (ArrayData->IsValidIndex((int32)Idx) && (*ArrayData)[Idx] == Old) {
                        (*ArrayData)[Idx] = New;
                        if (auto* PRef = Allocator.GetParentRef(New)) *PRef = Parent;
                        Allocator.UpdateParentEdgeArray(New, Idx);
                        bDone = true;
                    }
                }
            }
            if (!bDone && ArrayData) {
                for (int32 i = 0; i < ArrayData->Num(); ++i) {
                    if ((*ArrayData)[i] == Old) {
                        (*ArrayData)[i] = New;
                        if (auto* PRef = Allocator.GetParentRef(New)) *PRef = Parent;
                        Allocator.UpdateParentEdgeArray(New, (uint16)i);
                        bDone = true;
                        break;
                    }
                }
            }
            if (!bDone) return false;
            BumpNodeDataVersionAndStruct(Parent);
        }
        else if (ParentNode->IsMap()) {
            auto* MapData = ParentNode->GetMap();
            bool bDone = false;
            if (MapData) {
                const auto Kind = Allocator.GetParentEdgeKind(Old);
                if (Kind == ArzDax::EDaxParentEdgeKind::Map) {
                    const FName K = Allocator.GetParentEdgeLabel(Old);
                    auto It = MapData->find(K);
                    if (It != MapData->end() && It->second == Old) {
                        It->second = New;
                        if (auto* PRef = Allocator.GetParentRef(New)) *PRef = Parent;
                        Allocator.UpdateParentEdgeMap(New, K);
                        bDone = true;
                    }
                }
                if (!bDone) {
                    for (auto& KV : *MapData) {
                        if (KV.second == Old) {
                            const FName K = KV.first;
                            KV.second = New;
                            if (auto* PRef = Allocator.GetParentRef(New)) *PRef = Parent;
                            Allocator.UpdateParentEdgeMap(New, K);
                            bDone = true;
                            break;
                        }
                    }
                }
            }
            if (!bDone) return false;
            BumpNodeDataVersionAndStruct(Parent);
        }
        else {
            return false;
        }
        ReleaseRecursive(Old);
    }

    return true;
}

bool FDaxSet::ReleaseNode(const FDaxNodeID ID) {
    if (!Allocator.IsNodeValid(ID)) return false;
    return Allocator.Deallocate(ID);
}

int32 FDaxSet::ReleaseRecursive(const FDaxNodeID ID) {
    uint32 Cleared = 0;
    ReleaseSubtreeImp(ID, Cleared);
    if (Cleared > 0) BumpStructVersion();
    return static_cast<int32>(Cleared);
}

int32 FDaxSet::ReleaseChildren(const FDaxNodeID ID) {
    if (!Allocator.IsNodeValid(ID)) return 0;
    uint32 Cleared = 0;
    if (auto* Node = Allocator.TryGetNode(ID)) {
        if (Node->IsArray()) {
            if (auto* Arr = Node->GetArray()) {
                for (const FDaxNodeID& Child : *Arr) {
                    if (Child.IsValid()) ReleaseSubtreeImp(Child, Cleared);
                }
                Arr->Empty();
            }
        }
        else if (Node->IsMap()) {
            if (auto* Map = Node->GetMap()) {
                for (auto& KV : *Map) {
                    const FDaxNodeID Child = KV.second;
                    if (Child.IsValid()) ReleaseSubtreeImp(Child, Cleared);
                }
                Map->clear();
            }
        }
        if (Cleared > 0) {
            BumpNodeDataVersionAndStruct(ID);
        }
    }
    return static_cast<int32>(Cleared);
}

uint32 FDaxSet::GetNodeNumRecursive(const FDaxNodeID ID) const {
    int32 Count = 0;
    GetNodeNumRecursiveImp(ID, Count);
    return Count;
}

void FDaxSet::ReleaseSubtreeImp(const FDaxNodeID ID, uint32& ClearNum) {
    if (!Allocator.IsNodeValid(ID)) return;
    if (auto* Node = Allocator.TryGetNode(ID)) {
        // 先释放子树
        if (Node->IsArray()) {
            if (auto* Arr = Node->GetArray()) {
                for (const FDaxNodeID& Child : *Arr) {
                    if (Child.IsValid()) ReleaseSubtreeImp(Child, ClearNum);
                }
            }
        }
        else if (Node->IsMap()) {
            if (auto* Map = Node->GetMap()) {
                for (auto& KV : *Map) {
                    const FDaxNodeID Child = KV.second;
                    if (Child.IsValid()) ReleaseSubtreeImp(Child, ClearNum);
                }
            }
        }
    }

    if (Allocator.Deallocate(ID)) {
        ++ClearNum;
    }
}

void FDaxSet::GetNodeNumRecursiveImp(const FDaxNodeID ID, int32& ClearNum) const {
    if (!Allocator.IsNodeValid(ID)) return;
    ++ClearNum;
    if (auto* Node = const_cast<ArzDax::FDaxNode*>(Allocator.TryGetNode(ID))) {
        if (Node->IsArray()) {
            if (auto* Arr = Node->GetArray()) {
                for (const FDaxNodeID& Child : *Arr) {
                    if (Child.IsValid()) GetNodeNumRecursiveImp(Child, ClearNum);
                }
            }
        }
        else if (Node->IsMap()) {
            if (auto* Map = Node->GetMap()) {
                for (auto& KV : *Map) {
                    const FDaxNodeID Child = KV.second;
                    if (Child.IsValid()) GetNodeNumRecursiveImp(Child, ClearNum);
                }
            }
        }
    }
}

void FDaxSet::Clear() {
    Allocator.Reset();
    RootID = {};
}

void FDaxSet::RegisterDaxSetOnNetWork(FArchive& Ar) {
    if (Ar.IsSaving()) {
        uint32 ID = ArzDax::FDaxGlobalSetManager::Register(this);
        Ar.SerializeIntPacked(ID);
    }
    else if (Ar.IsLoading()) {
        // 客户端
        uint32 ID;
        Ar.SerializeIntPacked(ID);
        if (ID == 0) return;
        ArzDax::FDaxGlobalSetManager::RegisterOnClient(ID, this);
    }
}

bool FDaxSet::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms) {
    if (DeltaParms.bUpdateUnmappedObjects) return true;
    SCOPE_CYCLE_COUNTER(STAT_NetSyncTick);

    // Sync: 新实现入口
    if (DeltaParms.Writer) {
        bRunningOnServer = true;
        if (GetNodeNum() <= 1) return false;
        if (DataVersion == 0 && StructVersion == 0) return false; // 空容器无需同步

        FDaxSetBaseState* OldState = static_cast<FDaxSetBaseState*>(DeltaParms.OldState);
        if (OldState == nullptr) {
            TSharedPtr<FDaxSetBaseState> NewState = MakeShared<FDaxSetBaseState>();
            *DeltaParms.NewState = NewState;
            NewState->ContainerVersion = DataVersion;
            for (uint32 ci = 0; ci < Allocator.GetChunkCount(); ++ci) {
                const FDaxNodeChunkMeta* Meta = Allocator.GetChunkMetadata(ci);
                if (!Meta) continue;
                auto Copy = MakeUnique<FDaxNodeChunkMeta>(*Meta);
                NewState->ChildStates.Add(MoveTemp(Copy));
            }

            Allocator.ForEachNode([&](const FDaxNodeID NodeID, FDaxNode& Node, const uint32 Version, const FDaxNodeID Parent, const UScriptStruct* Type) {
                if (Type == FDaxFakeTypeArray::StaticStruct()) {
                    if (auto* Arr = Node.GetArray()) { NewState->ArrayMirror[NodeID] = *Arr; }
                }
                else if (Type == FDaxFakeTypeMap::StaticStruct()) {
                    if (auto* Map = Node.GetMap()) { NewState->MapMirror[NodeID] = *Map; }
                }
            });

            return Sync_ServerFullSyncImpl(DeltaParms);
        }
        else {
            if (OldState->ContainerVersion == DataVersion) return false;
            return Sync_ServerDeltaSyncImpl(DeltaParms, OldState);
        }
    }

    if (DeltaParms.Reader) {
        bRunningOnServer = false;
        OldValueMap.clear();
        FBitReader& Reader = *DeltaParms.Reader;
        const bool IsFullSync = Reader.ReadBit() != 0;
        if (IsFullSync) return Sync_ClientFullSyncImpl(DeltaParms);
        return Sync_ClientDeltaSyncImpl(DeltaParms);
    }

    return false;
}

// ================== 监听/OldValue API实现 ==================

bool FDaxSet::BindOnChanged(const FDaxVisitor& Position, int32 Depth, const FDaxOnChangedDynamic& Delegate) {
    //if (bRunningOnServer) return false; // 仅客户端
    if (!Position.IsValid()) return false;
    if (!Delegate.IsBound()) return false;
    FDaxOnChangedBinding B;
    B.Position = Position;
    B.Depth = FMath::Max(0, Depth);
    B.Delegate = Delegate;
    OnChangedBindings.Add(B);
    OnChangedIndexStructVersion = 0; // 触发下次派发前重建索引
    return true;
}

void FDaxSet::UnbindOnChanged(const FDaxVisitor& Position) {
    OnChangedBindings.RemoveAll([&](const FDaxOnChangedBinding& B) {
        return B.Position == Position;
    });
    OnChangedIndexStructVersion = 0;
}

void FDaxSet::UnbindAllFor(UObject* TargetObject) {
    if (!TargetObject) return;
    OnChangedBindings.RemoveAll([&](const FDaxOnChangedBinding& B) {
        return B.Delegate.IsBound() && B.Delegate.GetUObject() == TargetObject;
    });
    OnChangedIndexStructVersion = 0;
}

FConstStructView FDaxSet::TryGetOldValueByNodeID(const FDaxNodeID NodeID) const {
    if (const auto Found = OldValueMap.findPtr(NodeID)) {
        return FConstStructView(*Found);
    }
    return {};
}

void FDaxSet::RebuildOnChangedIndex() {
    OnChangedIndex.clear();
    for (int32 i = 0; i < OnChangedBindings.Num(); ++i) {
        FDaxOnChangedBinding& B = OnChangedBindings[i];
        if (!B.IsValid()) continue;
        FDaxVisitor Anchor = B.Position;
        if (!Anchor.EnsureResolvedReadOnly()) continue;
        const FDaxNodeID AnchorID = Anchor.GetCachedNodeID();
        if (!AnchorID.IsValid()) continue;
        B.AnchorID = AnchorID;
        B.AnchorStructVersion = StructVersion;
        TArray<int32>& List = OnChangedIndex[AnchorID];
        List.Add(i);
    }
}

// ================== Sync*Impl（拆分实现） ==================

bool FDaxSet::Sync_ServerFullSyncImpl(FNetDeltaSerializeInfo& DeltaParms) {
    FBitWriter& Writer = *DeltaParms.Writer;
    DAX_NET_SYNC_LOG(Warning, TEXT("FDaxSet::Sync_ServerFullSyncImpl"));
    TSharedPtr<FDaxSetBaseState> NewState = MakeShared<FDaxSetBaseState>();
    *DeltaParms.NewState = NewState;

    Writer.WriteBit(true); // 全量同步标记
    RegisterDaxSetOnNetWork(Writer);

    NewState->ContainerVersion = DataVersion;

    const uint32 ChunkCount = Allocator.GetChunkCount();
    for (uint32 ci = 0; ci < ChunkCount; ++ci) {
        const FDaxNodeChunkMeta* Meta = Allocator.GetChunkMetadata(ci);
        if (!Meta) continue;
        auto Copy = MakeUnique<FDaxNodeChunkMeta>(*Meta);
        NewState->ChildStates.Add(MoveTemp(Copy));
    }

    uint32 NodeCount = GetNodeNum();
    Writer.SerializeIntPacked(NodeCount);

    Allocator.ForEachNode([&](FDaxNodeID NodeID, FDaxNode& Node, uint32 Version, FDaxNodeID Parent,
                              const UScriptStruct* Type) {
        Writer << NodeID;
        Writer << Parent;
        UObject* TempTypeObject = const_cast<UScriptStruct*>(Type);
        DeltaParms.Map->SerializeObject(Writer, UScriptStruct::StaticClass(), TempTypeObject);

        if (Type == nullptr || Type == FDaxFakeTypeEmpty::StaticStruct()) {
            // 空
        }
        else if (Type == FDaxFakeTypeArray::StaticStruct()) {
            auto* Arr = Node.GetArray();
            uint32 Count = Arr ? static_cast<uint32>(Arr->Num()) : 0;
            Writer.SerializeIntPacked(Count);
            NewState->ArrayMirror[NodeID] = {};
            auto& ArrRef = NewState->ArrayMirror[NodeID];
            for (uint32 i = 0; i < Count; ++i) {
                Writer << (*Arr)[i];
                ArrRef.Add((*Arr)[i]);
            }
        }
        else if (Type == FDaxFakeTypeMap::StaticStruct()) {
            auto* Map = Node.GetMap();
            uint32 Count = Map ? static_cast<uint32>(Map->size()) : 0;
            Writer.SerializeIntPacked(Count);
            if (Map) {
                NewState->MapMirror[NodeID] = {};
                auto& MapRef = NewState->MapMirror[NodeID];
                for (auto& Kv : *Map) {
                    Writer << Kv.first;
                    Writer << Kv.second;
                    MapRef[Kv.first] = Kv.second;
                }
            }
        }
        else {
            Node.SerializeValueData(Writer, DeltaParms.Map, Type);
        }
    });
    DAX_NET_SYNC_LOG(Warning, TEXT("FDaxSet::Sync_ServerFullSyncImpl End"));
    return true;
}

bool FDaxSet::Sync_ServerDeltaSyncImpl(FNetDeltaSerializeInfo& DeltaParms, FDaxSetBaseState* OldState) {
    FBitWriter& Writer = *DeltaParms.Writer;
    DAX_NET_SYNC_LOG(Warning, TEXT("FDaxSet::Sync_ServerDeltaSyncImpl"));

    Writer.WriteBit(false); // 增量标记
    if (OldState->GlobalSetID <= 1) {
        Writer.WriteBit(true);
        RegisterDaxSetOnNetWork(Writer);
    }
    else {
        Writer.WriteBit(false);
    }

    const int32 CurrChunkCount = static_cast<int32>(Allocator.GetChunkCount());
    const int32 OldChunkCount = OldState ? OldState->ChildStates.Num() : 0;
    const int32 MaxChunkCount = FMath::Max(CurrChunkCount, OldChunkCount);

    struct FAddRec {
        FDaxNodeID ID;
        uint32 Version;
        FDaxNodeID Parent;
        const UScriptStruct* Type;
    };
    struct FUpdRec {
        FDaxNodeID ID;
        uint32 Version;
        FDaxNodeID Parent;
        const UScriptStruct* Type;
    };
    TArray<FDaxNodeID> Removes;
    TArray<FAddRec> Adds;
    TArray<FUpdRec> Updates;

    auto WriteFullArray = [&](FArchive& Ar, const FDaxNodeID ID) {
        auto* Node = Allocator.TryGetNode(ID);
        auto* Arr = Node ? Node->GetArray() : nullptr;
        uint32 Count = Arr ? static_cast<uint32>(Arr->Num()) : 0;
        Ar.SerializeIntPacked(Count);
        for (uint32 i = 0; i < Count; ++i) Ar << (*Arr)[i];
    };
    auto WriteFullMap = [&](FArchive& Ar, const FDaxNodeID ID) {
        auto* Node = Allocator.TryGetNode(ID);
        auto* Map = Node ? Node->GetMap() : nullptr;
        uint32 Count = Map ? static_cast<uint32>(Map->size()) : 0;
        Ar.SerializeIntPacked(Count);
        if (Map)
            for (auto& Kv : *Map) {
                Ar << Kv.first;
                Ar << Kv.second;
            }
    };
    auto WriteMapDelta = [&](FArchive& Ar, const FDaxNodeID ID, const FDaxSetBaseState* Old) {
        TArray<FName> RemovesKeys;
        TArray<std::pair<FName, FDaxNodeID>> AddPairs;
        TArray<std::pair<FName, FDaxNodeID>> RebindPairs;
        const auto& OldMapRefOpt = Old->MapMirror.find(ID);
        const FDaxMapType* OldMap = (OldMapRefOpt != Old->MapMirror.end()) ? &OldMapRefOpt->second : nullptr;
        auto* Node = Allocator.TryGetNode(ID);
        auto* NewMap = Node ? Node->GetMap() : nullptr;
        if (OldMap) {
            for (const auto& KV : *OldMap) {
                const FName K = KV.first;
                const FDaxNodeID OldChild = KV.second;
                if (!NewMap || NewMap->find(K) == NewMap->end()) RemovesKeys.Add(K);
                else {
                    const FDaxNodeID NewChild = NewMap->at(K);
                    if (!(NewChild == OldChild)) RebindPairs.Emplace(K, NewChild);
                }
            }
        }
        if (NewMap) {
            for (const auto& KV : *NewMap) {
                const FName K = KV.first;
                if (!OldMap || OldMap->find(K) == OldMap->end()) AddPairs.Emplace(K, KV.second);
            }
        }
        uint32 Rm = RemovesKeys.Num();
        uint32 Ad = static_cast<uint32>(AddPairs.Num());
        uint32 Rb = static_cast<uint32>(RebindPairs.Num());
        Ar.SerializeIntPacked(Rm);
        Ar.SerializeIntPacked(Ad);
        Ar.SerializeIntPacked(Rb);
        for (const auto& K : RemovesKeys) Ar << const_cast<FName&>(K);
        for (const auto& P : AddPairs) {
            Ar << const_cast<FName&>(P.first);
            Ar << const_cast<FDaxNodeID&>(P.second);
        }
        for (const auto& P : RebindPairs) {
            Ar << const_cast<FName&>(P.first);
            Ar << const_cast<FDaxNodeID&>(P.second);
        }
        return Rm + Ad + Rb;
    };
    auto WriteArrayDelta = [&](FArchive& Ar, const FDaxNodeID ID, const FDaxSetBaseState* Old) {
        const auto& OldArrRefOpt = Old->ArrayMirror.find(ID);
        const TArray<FDaxNodeID>* OldArr = (OldArrRefOpt != Old->ArrayMirror.end()) ? &OldArrRefOpt->second : nullptr;
        auto* Node = Allocator.TryGetNode(ID);
        auto* NewArr = Node ? Node->GetArray() : nullptr;
        const int32 oldN = OldArr ? OldArr->Num() : 0;
        const int32 newN = NewArr ? NewArr->Num() : 0;
        if (newN == 0) {
            uint8 kind = 1;
            Ar << kind;
            return 1u;
        }
        int32 L = 0;
        while (L < oldN && L < newN) {
            if ((*OldArr)[L] != (*NewArr)[L]) break;
            ++L;
        }
        int32 R = 0;
        while ((oldN - R - 1) >= L && (newN - R - 1) >= L) {
            const FDaxNodeID A = (*OldArr)[oldN - R - 1];
            const FDaxNodeID B = (*NewArr)[newN - R - 1];
            if (!(A == B)) break;
            ++R;
        }
        const int32 oldMid = oldN - L - R;
        const int32 newMid = newN - L - R;
        uint8 kind = 0;
        Ar << kind;
        uint32 Start = static_cast<uint32>(L);
        uint32 OldCount = static_cast<uint32>(oldMid);
        uint32 NewCount = static_cast<uint32>(newMid);
        Ar.SerializeIntPacked(Start);
        Ar.SerializeIntPacked(OldCount);
        Ar.SerializeIntPacked(NewCount);
        for (int32 i = 0; i < newMid; ++i) {
            FDaxNodeID C = (*NewArr)[L + i];
            Ar << C;
        }
        return static_cast<uint32>(1);
    };

    for (int32 ChunkIndex = 0; ChunkIndex < MaxChunkCount; ++ChunkIndex) {
        const auto OldMeta = (OldState && OldState->ChildStates.IsValidIndex(ChunkIndex))
                                 ? OldState->ChildStates[ChunkIndex].Get()
                                 : nullptr;
        const auto NewMeta = Allocator.GetChunkMetadata(ChunkIndex);
        const uint32 MainUsedMask = NewMeta ? NewMeta->UsedMask : 0;
        const uint32 OldUsedMask = OldMeta ? OldMeta->UsedMask : 0;
        if (MainUsedMask == 0 && OldUsedMask == 0) continue;
        if (OldMeta && NewMeta) {
            if (OldUsedMask == MainUsedMask) {
                if (memcmp(OldMeta->Generations, NewMeta->Generations, sizeof(NewMeta->Generations)) == 0 &&
                    memcmp(OldMeta->Versions, NewMeta->Versions, sizeof(NewMeta->Versions)) == 0 &&
                    memcmp(OldMeta->Parent, NewMeta->Parent, sizeof(NewMeta->Parent)) == 0 &&
                    memcmp(OldMeta->ValueType, NewMeta->ValueType, sizeof(NewMeta->ValueType)) == 0) {
                    continue;
                }
            }
        }
        uint32 CombineMask = MainUsedMask | OldUsedMask;
        while (CombineMask) {
            const uint16 LocalIndex = static_cast<uint16>(FMath::CountTrailingZeros(CombineMask));
            const uint32 CurrentBit = (1u << LocalIndex);
            const bool bInNew = (MainUsedMask & CurrentBit) != 0;
            const bool bInOld = (OldUsedMask & CurrentBit) != 0;
            const uint16 GlobalIndex = static_cast<uint16>((ChunkIndex << DAX_NODE_POOR_CHUNK_SHIFT) | LocalIndex);
            if (bInNew && bInOld) {
                const uint16 OldGen = OldMeta->Generations[LocalIndex];
                const uint16 NewGen = NewMeta->Generations[LocalIndex];
                const FDaxNodeID OldID(GlobalIndex, OldGen);
                const FDaxNodeID NewID(GlobalIndex, NewGen);
                if (!(OldID == NewID)) {
                    Removes.Add(OldID);
                    Adds.Add(FAddRec{
                        NewID, NewMeta->Versions[LocalIndex], NewMeta->Parent[LocalIndex],
                        NewMeta->ValueType[LocalIndex]
                    });
                }
                else {
                    bool bChanged = false;
                    bChanged |= (OldMeta->Versions[LocalIndex] != NewMeta->Versions[LocalIndex]);
                    bChanged |= !(OldMeta->Parent[LocalIndex] == NewMeta->Parent[LocalIndex]);
                    bChanged |= (OldMeta->ValueType[LocalIndex] != NewMeta->ValueType[LocalIndex]);
                    if (bChanged)
                        Updates.Add(FUpdRec{
                            NewID, NewMeta->Versions[LocalIndex], NewMeta->Parent[LocalIndex],
                            NewMeta->ValueType[LocalIndex]
                        });
                }
            }
            else if (!bInNew && bInOld) {
                const uint16 OldGen = OldMeta->Generations[LocalIndex];
                Removes.Add(FDaxNodeID(GlobalIndex, OldGen));
            }
            else if (bInNew && !bInOld) {
                const uint16 NewGen = NewMeta->Generations[LocalIndex];
                Adds.Add(FAddRec{
                    FDaxNodeID(GlobalIndex, NewGen), NewMeta->Versions[LocalIndex], NewMeta->Parent[LocalIndex],
                    NewMeta->ValueType[LocalIndex]
                });
            }
            CombineMask &= ~CurrentBit;
        }
    }

    uint32 AddsCount = Adds.Num();
    uint32 RemovesCount = Removes.Num();
    uint32 UpdatesCount = Updates.Num();
    Writer.SerializeIntPacked(AddsCount);
    Writer.SerializeIntPacked(RemovesCount);
    Writer.SerializeIntPacked(UpdatesCount);

    for (auto ID : Removes) { Writer << ID; }
    for (const auto& R : Adds) {
        const bool IsValueType = (R.Type && R.Type != FDaxFakeTypeEmpty::StaticStruct() && R.Type !=
            FDaxFakeTypeArray::StaticStruct() && R.Type != FDaxFakeTypeMap::StaticStruct());
        const bool IsContainer = (R.Type == FDaxFakeTypeArray::StaticStruct() || R.Type ==
            FDaxFakeTypeMap::StaticStruct());
        bool bSendValue = false;
        if (IsValueType) {
            if (auto* Node = Allocator.TryGetNode(R.ID)) {
                if (Node->IsValue()) {
                    auto SV = Node->TryGetValueGenericMutable();
                    auto SS = SV.GetScriptStruct();
                    bSendValue = (SS == R.Type);
                }
            }
        }
        const bool bSendCFull = IsContainer; // Add 容器统一发全量
        uint8 Flags = ArzDax::DaxMakeDeltaFlags(EDaxDeltaOp::Add, true, true, bSendValue, false, bSendCFull);
        Writer << const_cast<FDaxNodeID&>(R.ID);
        Writer << Flags;
        Writer << const_cast<FDaxNodeID&>(R.Parent);
        UObject* TempTypeObject = const_cast<UScriptStruct*>(R.Type);
        DeltaParms.Map->SerializeObject(Writer, UScriptStruct::StaticClass(), TempTypeObject);
        if (bSendValue) {
            if (auto* Node = Allocator.TryGetNode(R.ID)) Node->SerializeValueData(Writer, DeltaParms.Map, R.Type);
        }
        else if (bSendCFull) {
            if (R.Type == FDaxFakeTypeArray::StaticStruct()) WriteFullArray(Writer, R.ID);
            else if (R.Type == FDaxFakeTypeMap::StaticStruct()) WriteFullMap(Writer, R.ID);
        }
    }

    for (const auto& R : Updates) {
        const uint16 ChunkIndex = static_cast<uint16>(R.ID.Index >> DAX_NODE_POOR_CHUNK_SHIFT);
        const uint16 LocalIndex = static_cast<uint16>(R.ID.Index & DAX_NODE_POOR_CHUNK_MASK);
        const auto* OldMeta = (OldState && OldState->ChildStates.IsValidIndex(ChunkIndex))
                                  ? OldState->ChildStates[ChunkIndex].Get()
                                  : nullptr;
        const auto* NewMeta = Allocator.GetChunkMetadata(ChunkIndex);
        bool bParentChanged = false, bTypeChanged = false, bVersionChanged = false;
        if (OldMeta && NewMeta) {
            bParentChanged = !(OldMeta->Parent[LocalIndex] == NewMeta->Parent[LocalIndex]);
            bTypeChanged = (OldMeta->ValueType[LocalIndex] != NewMeta->ValueType[LocalIndex]);
            bVersionChanged = (OldMeta->Versions[LocalIndex] != NewMeta->Versions[LocalIndex]);
        }
        const bool IsValueType = (R.Type && R.Type != FDaxFakeTypeEmpty::StaticStruct() && R.Type !=
            FDaxFakeTypeArray::StaticStruct() && R.Type != FDaxFakeTypeMap::StaticStruct());
        const bool IsArrayType = (R.Type == FDaxFakeTypeArray::StaticStruct());
        const bool IsMapType = (R.Type == FDaxFakeTypeMap::StaticStruct());
        bool bSendParent = bParentChanged;
        bool bSendCDelta = false;
        bool bSendCFull = false;
        if (IsArrayType) {
            bool bHasOld = (OldState->ArrayMirror.find(R.ID) != OldState->ArrayMirror.end());
            if (!bHasOld) bSendCFull = true;
            else {
                auto* Node = Allocator.TryGetNode(R.ID);
                auto* Arr = Node ? Node->GetArray() : nullptr;
                const int32 newN = Arr ? Arr->Num() : 0;
                const int32 oldN = OldState->ArrayMirror[R.ID].Num();
                const int32 approxChange = FMath::Abs(newN - oldN);
                if (approxChange > FMath::Max(1, newN / 2)) bSendCFull = true;
                else bSendCDelta = true;
            }
        }
        else if (IsMapType) {
            bool bHasOld = (OldState->MapMirror.find(R.ID) != OldState->MapMirror.end());
            if (!bHasOld) bSendCFull = true;
            else {
                const auto& OldMap = OldState->MapMirror[R.ID];
                auto* Node = Allocator.TryGetNode(R.ID);
                auto* Map = Node ? Node->GetMap() : nullptr;
                const int32 newN = Map ? static_cast<int32>(Map->size()) : 0;
                const int32 oldN = static_cast<int32>(OldMap.size());
                const int32 approxChange = FMath::Abs(newN - oldN);
                if (approxChange > FMath::Max(1, newN / 2)) bSendCFull = true;
                else bSendCDelta = true;
            }
        }
        bool bSendValue = false;
        if (IsValueType && (bVersionChanged || bTypeChanged)) {
            if (auto* NodePtr = Allocator.TryGetNode(R.ID)) {
                if (NodePtr->IsValue()) {
                    auto SVm = NodePtr->TryGetValueGenericMutable();
                    auto SSm = SVm.GetScriptStruct();
                    bSendValue = (SSm == R.Type);
                }
            }
        }
        bool bSendType = bTypeChanged || bSendValue || bSendCDelta || bSendCFull;
        uint8 Flags = ArzDax::DaxMakeDeltaFlags(EDaxDeltaOp::Update, bSendParent, bSendType, bSendValue, bSendCDelta,
                                                bSendCFull);
        Writer << const_cast<FDaxNodeID&>(R.ID);
        Writer << Flags;
        if (bSendParent) Writer << const_cast<FDaxNodeID&>(R.Parent);
        if (bSendType) {
            UObject* TempTypeObject = const_cast<UScriptStruct*>(R.Type);
            DeltaParms.Map->SerializeObject(Writer, UScriptStruct::StaticClass(), TempTypeObject);
        }
        if (bSendValue) {
            if (auto* Node = Allocator.TryGetNode(R.ID)) {
                TObjectPtr<const UScriptStruct> TempType = R.Type;
                Node->SerializeValueData(Writer, DeltaParms.Map, TempType);
            }
        }
        else if (bSendCFull) {
            if (IsArrayType) WriteFullArray(Writer, R.ID);
            else if (IsMapType) WriteFullMap(Writer, R.ID);
        }
        else if (bSendCDelta) {
            if (IsArrayType) WriteArrayDelta(Writer, R.ID, OldState);
            else if (IsMapType) WriteMapDelta(Writer, R.ID, OldState);
        }
    }

    TSharedPtr<FDaxSetBaseState> NewState = MakeShared<FDaxSetBaseState>();
    NewState->ContainerVersion = DataVersion;
    for (int32 ci = 0; ci < CurrChunkCount; ++ci) {
        const FDaxNodeChunkMeta* Meta = Allocator.GetChunkMetadata((uint16)ci);
        if (!Meta) continue;
        auto Copy = MakeUnique<FDaxNodeChunkMeta>(*Meta);
        NewState->ChildStates.Add(MoveTemp(Copy));
    }
    if (OldState) {
        for (const auto& P : OldState->ArrayMirror) { NewState->ArrayMirror.emplace(P.first, P.second); }
        for (const auto& P : OldState->MapMirror) { NewState->MapMirror.emplace(P.first, P.second); }
    }
    for (const auto& ID : Removes) {
        NewState->ArrayMirror.erase(ID);
        NewState->MapMirror.erase(ID);
    }
    auto RefreshContainerMirror = [&](const FDaxNodeID ID, const UScriptStruct* Type) {
        if (Type == FDaxFakeTypeArray::StaticStruct()) {
            if (auto* Node = Allocator.TryGetNode(ID)) {
                if (auto* Arr = Node->GetArray()) { NewState->ArrayMirror[ID] = *Arr; }
                else { NewState->ArrayMirror.erase(ID); }
            }
            NewState->MapMirror.erase(ID);
        }
        else if (Type == FDaxFakeTypeMap::StaticStruct()) {
            if (auto* Node = Allocator.TryGetNode(ID)) {
                if (auto* Map = Node->GetMap()) { NewState->MapMirror[ID] = *Map; }
                else { NewState->MapMirror.erase(ID); }
            }
            NewState->ArrayMirror.erase(ID);
        }
        else {
            NewState->ArrayMirror.erase(ID);
            NewState->MapMirror.erase(ID);
        }
    };
    for (const auto& R : Adds) RefreshContainerMirror(R.ID, R.Type);
    for (const auto& R : Updates) RefreshContainerMirror(R.ID, R.Type);

    *DeltaParms.NewState = NewState;
    DAX_NET_SYNC_LOG(Warning, TEXT("FDaxSet::Sync_ServerDeltaSyncImpl End"));
    return true;
}

bool FDaxSet::Sync_ClientFullSyncImpl(FNetDeltaSerializeInfo& DeltaParms) {
    DAX_NET_SYNC_LOG(Warning, TEXT("FDaxSet::Sync_ClientFullSyncImpl"));
    FBitReader& Reader = *DeltaParms.Reader;

    // 入口已读取 IsFullSync=true
    Allocator.Reset();
    ClearOverlayMap();
    RootID = {};

    RegisterDaxSetOnNetWork(Reader);

    uint32 NodeCount = 0;
    Reader.SerializeIntPacked(NodeCount);
    FDaxNodeID RootCandidate{};

    for (uint32 i = 0; i < NodeCount; ++i) {
        FDaxNodeID NodeID;
        Reader << NodeID;
        FDaxNodeID Parent;
        Reader << Parent;
        UObject* TempTypeObject = nullptr;
        const bool bTypeMapped = DeltaParms.Map->SerializeObject(Reader, UScriptStruct::StaticClass(), TempTypeObject);
        if (!bTypeMapped) {
            DAX_NET_SYNC_LOG(Warning, "FullSync: Unmapped UScriptStruct at node {0}. Defer reading.", NodeID.ToString());
            DeltaParms.bOutHasMoreUnmapped = true;
            return true;
        }
        TObjectPtr<const UScriptStruct> TempType = Cast<UScriptStruct>(TempTypeObject);

        Allocator.AllocateSlotAt(NodeID);
        if (auto InfoRef = Allocator.GetCommonInfoRef(NodeID); InfoRef.IsValid()) {
            if (InfoRef.pParent) *InfoRef.pParent = Parent;
            if (TempType) Allocator.UpdateValueType(NodeID, TempType);
        }

        auto* Node = Allocator.TryGetNode(NodeID);
        if (!Node) continue;

        if (TempType == nullptr || TempType == FDaxFakeTypeEmpty::StaticStruct()) {
            Node->ResetToEmpty();
            Allocator.UpdateValueType(NodeID, FDaxFakeTypeEmpty::StaticStruct());
        }
        else if (TempType == FDaxFakeTypeArray::StaticStruct()) {
            Node->ResetToEmptyArray();
            uint32 Count = 0;
            Reader.SerializeIntPacked(Count);
            if (auto* Arr = Node->GetArray()) {
                Arr->Empty();
                Arr->Reserve(Count);
                for (uint32 k = 0; k < Count; ++k) {
                    FDaxNodeID C;
                    Reader << C;
                    Arr->Add(C);
                }
            }
            else {
                for (uint32 k = 0; k < Count; ++k) {
                    FDaxNodeID Dummy;
                    Reader << Dummy;
                }
            }
            Allocator.UpdateValueType(NodeID, FDaxFakeTypeArray::StaticStruct());
        }
        else if (TempType == FDaxFakeTypeMap::StaticStruct()) {
            Node->ResetToEmptyMap();
            uint32 Count = 0;
            Reader.SerializeIntPacked(Count);
            if (auto* Map = Node->GetMap()) {
                Map->clear();
                for (uint32 k = 0; k < Count; ++k) {
                    FName K;
                    FDaxNodeID C;
                    Reader << K;
                    Reader << C;
                    (*Map)[K] = C;
                }
            }
            else {
                for (uint32 k = 0; k < Count; ++k) {
                    FName K;
                    FDaxNodeID Dummy;
                    Reader << K;
                    Reader << Dummy;
                }
            }
            Allocator.UpdateValueType(NodeID, FDaxFakeTypeMap::StaticStruct());
        }
        else {
            Node->SerializeValueData(Reader, DeltaParms.Map, TempType);
            Allocator.UpdateValueType(NodeID, TempType);
        }

        if (!Parent.IsValid()) RootCandidate = NodeID;
    }

    if (RootCandidate.IsValid()) RootID = RootCandidate;
    ++StructVersion;
    ++DataVersion;
    DAX_NET_SYNC_LOG(Warning, TEXT("FDaxSet::Sync_ClientFullSyncImpl End"));
    DAX_NET_SYNC_LOG(Warning, "Full ReaderBits pos={0}/{1}", Reader.GetPosBits(), Reader.GetNumBits());
    return true;
}

bool FDaxSet::Sync_ClientDeltaSyncImpl(FNetDeltaSerializeInfo& DeltaParms) {
    SCOPE_CYCLE_COUNTER(STAT_NetDeltaSync);
    DAX_NET_SYNC_LOG(Warning, "DaxSet::Sync_ClientDeltaSyncImpl");
    FBitReader& Reader = *DeltaParms.Reader;
    if (const bool IsNeedRegister = Reader.ReadBit() != 0) {
        RegisterDaxSetOnNetWork(Reader);
    }
    ClearOverlayMap();

    bool bLocalStructChanged = false;
    bool bLocalDataChanged = false;

    struct FChangedEvent {
        FDaxNodeID ID;
        TArray<TVariant<FName, int32>> Path;
    };
    TArray<FChangedEvent> ChangedEvents;

    auto BuildPathFromNodeID = [&](const FDaxNodeID Leaf, TArray<TVariant<FName, int32>>& OutPath) {
        OutPath.Reset();
        if (!Allocator.IsNodeValid(Leaf)) return false;
        struct FSeg {
            TVariant<FName, int32> V;
        };
        TArray<FSeg, TInlineAllocator<32>> Rev;
        FDaxNodeID Curr = Leaf;
        while (Curr.IsValid()) {
            const FDaxNodeID P = GetNodeParent(Curr);
            if (!P.IsValid()) break;
            ArzDax::FDaxNode* PNode = Allocator.TryGetNode(P);
            if (!PNode) {
                Rev.Empty();
                break;
            }
            if (PNode->IsArray()) {
                auto* Arr = PNode->GetArray();
                int32 Found = -1;
                if (Arr) {
                    // 快路径：反向映射
                    if (Allocator.GetParentEdgeKind(Curr) == ArzDax::EDaxParentEdgeKind::Array) {
                        const uint16 EdgeIdx = Allocator.GetParentEdgeIndex(Curr);
                        if (Arr->IsValidIndex((int32)EdgeIdx) && (*Arr)[EdgeIdx] == Curr) {
                            Found = (int32)EdgeIdx;
                        }
                    }
                    // 回退：线性扫描，并修正反向映射
                    if (Found < 0) {
                        for (int32 i = 0; i < Arr->Num(); ++i) {
                            if ((*Arr)[i] == Curr) {
                                Found = i;
                                break;
                            }
                        }
                        if (Found >= 0) Allocator.UpdateParentEdgeArray(Curr, (uint16)Found);
                    }
                }
                if (Found < 0) {
                    Rev.Empty();
                    break;
                }
                Rev.Add(FSeg{TVariant<FName, int32>(TInPlaceType<int32>(), Found)});
            }
            else if (PNode->IsMap()) {
                auto* Map = PNode->GetMap();
                FName FoundKey = NAME_None;
                if (Map) {
                    // 快路径：反向映射
                    if (Allocator.GetParentEdgeKind(Curr) == ArzDax::EDaxParentEdgeKind::Map) {
                        const FName Label = Allocator.GetParentEdgeLabel(Curr);
                        auto It = Map->find(Label);
                        if (It != Map->end() && It->second == Curr) {
                            FoundKey = Label;
                        }
                    }
                    // 回退：线性扫描并修正
                    if (FoundKey.IsNone()) {
                        for (const auto& KV : *Map) {
                            if (KV.second == Curr) {
                                FoundKey = KV.first;
                                break;
                            }
                        }
                        if (!FoundKey.IsNone()) Allocator.UpdateParentEdgeMap(Curr, FoundKey);
                    }
                }
                if (FoundKey.IsNone()) {
                    Rev.Empty();
                    break;
                }
                Rev.Add(FSeg{TVariant<FName, int32>(TInPlaceType<FName>(), FoundKey)});
            }
            else {
                Rev.Empty();
                break;
            }
            Curr = P;
        }
        if (Rev.IsEmpty() && Leaf != RootID) return false;
        OutPath.Reserve(Rev.Num());
        for (int32 i = Rev.Num() - 1; i >= 0; --i) OutPath.Add(Rev[i].V);
        return true;
    };

    auto CaptureOldIfValue = [&](const FDaxNodeID ID) {
        if (OldValueMap.contains(ID)) return;
        if (!Allocator.IsNodeValid(ID)) return;
        ArzDax::FDaxNode* Node = Allocator.TryGetNode(ID);
        if (!Node) return;
        const UScriptStruct* Type = Allocator.GetValueType(ID);
        const bool IsValueType = (Type && Type != FDaxFakeTypeEmpty::StaticStruct() && Type !=
            FDaxFakeTypeArray::StaticStruct() && Type != FDaxFakeTypeMap::StaticStruct());
        if (!IsValueType) return;
        const FConstStructView SV = Node->TryGetValueGeneric();
        if (!SV.IsValid()) return;
        OldValueMap.emplace(ID, FInstancedStruct(SV));
    };

    uint32 AddsCount = 0, RemovesCount = 0, UpdatesCount = 0;
    Reader.SerializeIntPacked(AddsCount);
    Reader.SerializeIntPacked(RemovesCount);
    Reader.SerializeIntPacked(UpdatesCount);
    DAX_NET_SYNC_LOG(Warning, "Delta counts: Adds={0} Removes={1} Updates={2}", AddsCount, RemovesCount, UpdatesCount);
    if (RemovesCount > 0 || AddsCount > 0) { bLocalStructChanged = true; }

    for (uint32 i = 0; i < RemovesCount; ++i) {
        FDaxNodeID NodeID;
        Reader << NodeID;
        if (Allocator.IsNodeValid(NodeID)) {
            CaptureOldIfValue(NodeID);
            FChangedEvent Ev{NodeID, {}};
            BuildPathFromNodeID(NodeID, Ev.Path);
            ChangedEvents.Add(MoveTemp(Ev));
            ReleaseRecursive(NodeID);
        }
    }

    for (uint32 i = 0; i < AddsCount; ++i) {
        const int32 StartBits = Reader.GetPosBits();
        FDaxNodeID NodeID;
        Reader << NodeID;
        uint8 Flags = 0;
        Reader << Flags;
        FDaxNodeID Parent{};
        TObjectPtr<const UScriptStruct> TempType = nullptr;
        if (ArzDax::DaxFlagHasParent(Flags)) Reader << Parent;
        if (ArzDax::DaxFlagHasType(Flags)) {
            UObject* TempTypeObject = nullptr;
            const bool bTypeMapped = DeltaParms.Map->SerializeObject(Reader, UScriptStruct::StaticClass(), TempTypeObject);
            if (!bTypeMapped) {
                DAX_NET_SYNC_LOG(Warning, "Delta-Add: Unmapped UScriptStruct for node {0}, flags={1}. Defer.", NodeID.ToString(), FString::Printf(TEXT("0x%X"), Flags));
                DeltaParms.bOutHasMoreUnmapped = true;
                return true;
            }
            TempType = Cast<UScriptStruct>(TempTypeObject);
        }
        Allocator.AllocateSlotAt(NodeID);
        if (auto InfoRef = Allocator.GetCommonInfoRef(NodeID); InfoRef.IsValid()) {
            if (InfoRef.pParent && ArzDax::DaxFlagHasParent(Flags)) *InfoRef.pParent = Parent;
            if (TempType) Allocator.UpdateValueType(NodeID, TempType);
        }

        ArzDax::FDaxNode* Node = Allocator.TryGetNode(NodeID);
        auto ConsumeValuePayload = [&](const UScriptStruct* Type) {
            if (Node) {
                Node->SerializeValueData(Reader, DeltaParms.Map, Type);
            }
            else {
                FInstancedStruct Temp;
                Temp.InitializeAs(Type);
                if (Type && Type->GetCppStructOps()->HasNetSerializer()) {
                    bool Success = true;
                    Type->GetCppStructOps()->NetSerialize(Reader, DeltaParms.Map, Success, Temp.GetMutableMemory());
                }
                else if (Type) {
                    Type->SerializeBin(Reader, Temp.GetMutableMemory());
                }
            }
        };
        const FString TypeName = TempType ? TempType->GetName() : TEXT("null");
        DAX_NET_SYNC_LOG(Warning, "Delta-Add[{0}] Node={1} Flags={2} Type={3}", i, NodeID.ToString(), FString::Printf(TEXT("0x%X"), Flags), TypeName);

        if (!TempType || TempType == FDaxFakeTypeEmpty::StaticStruct()) {
            if (Node) {
                Node->ResetToEmpty();
                Allocator.UpdateValueType(NodeID, FDaxFakeTypeEmpty::StaticStruct());
            }
            bLocalDataChanged = true;
        }
        else if (TempType == FDaxFakeTypeArray::StaticStruct()) {
            if (Node && !Node->IsArray()) Node->ResetToEmptyArray();
            if (ArzDax::DaxFlagIsCFull(Flags)) {
                uint32 Count = 0;
                Reader.SerializeIntPacked(Count);
                auto* Arr = Node ? Node->GetArray() : nullptr;
                if (Arr) {
                    Arr->Empty();
                    Arr->Reserve(Count);
                    for (uint32 k = 0; k < Count; ++k) {
                        FDaxNodeID C;
                        Reader << C;
                        Arr->Add(C);
                    }
                    // 构建反向映射：数组
                    for (int32 iArr = 0; iArr < Arr->Num(); ++iArr) {
                        const FDaxNodeID Cid = (*Arr)[iArr];
                        if (Cid.IsValid()) Allocator.UpdateParentEdgeArray(Cid, (uint16)iArr);
                    }
                }
                else {
                    for (uint32 k = 0; k < Count; ++k) {
                        FDaxNodeID Dummy;
                        Reader << Dummy;
                    }
                }
            }
            else if (ArzDax::DaxFlagHasCDelta(Flags)) {
                uint8 kind = 0;
                Reader << kind;
                if (kind == 1) { if (Node) { if (auto* Arr = Node->GetArray()) Arr->Empty(); } }
                else {
                    uint32 Start = 0, OldCount = 0, NewCount = 0;
                    Reader.SerializeIntPacked(Start);
                    Reader.SerializeIntPacked(OldCount);
                    Reader.SerializeIntPacked(NewCount);
                    if (Node) {
                        if (auto* Arr = Node->GetArray()) {
                            Arr->Empty();
                            Arr->Reserve(Start + NewCount);
                            for (uint32 k = 0; k < NewCount; ++k) {
                                FDaxNodeID C;
                                Reader << C;
                                Arr->Add(C);
                            }
                            // 构建反向映射：数组
                            for (int32 iArr = 0; iArr < Arr->Num(); ++iArr) {
                                const FDaxNodeID Cid = (*Arr)[iArr];
                                if (Cid.IsValid()) Allocator.UpdateParentEdgeArray(Cid, (uint16)iArr);
                            }
                        }
                        else {
                            for (uint32 k = 0; k < NewCount; ++k) {
                                FDaxNodeID C;
                                Reader << C;
                            }
                        }
                    }
                    else {
                        for (uint32 k = 0; k < NewCount; ++k) {
                            FDaxNodeID C;
                            Reader << C;
                        }
                    }
                }
            }
            if (Node) Allocator.UpdateValueType(NodeID, FDaxFakeTypeArray::StaticStruct());
            bLocalDataChanged = true;
            bLocalStructChanged = true;
        }
        else if (TempType == FDaxFakeTypeMap::StaticStruct()) {
            if (Node && !Node->IsMap()) Node->ResetToEmptyMap();
            if (ArzDax::DaxFlagIsCFull(Flags)) {
                uint32 Count = 0;
                Reader.SerializeIntPacked(Count);
                if (Node) {
                    if (auto* Map = Node->GetMap()) {
                        Map->clear();
                        for (uint32 k = 0; k < Count; ++k) {
                            FName K;
                            FDaxNodeID C;
                            Reader << K;
                            Reader << C;
                            (*Map)[K] = C;
                            Allocator.UpdateParentEdgeMap(C, K);
                        }
                    }
                    else {
                        for (uint32 k = 0; k < Count; ++k) {
                            FName K;
                            FDaxNodeID Dummy;
                            Reader << K;
                            Reader << Dummy;
                        }
                    }
                }
                else {
                    for (uint32 k = 0; k < Count; ++k) {
                        FName K;
                        FDaxNodeID Dummy;
                        Reader << K;
                        Reader << Dummy;
                    }
                }
            }
            else if (ArzDax::DaxFlagHasCDelta(Flags)) {
                uint32 Rm = 0, Ad = 0, Rb = 0;
                Reader.SerializeIntPacked(Rm);
                Reader.SerializeIntPacked(Ad);
                Reader.SerializeIntPacked(Rb);
                if (Node) {
                    if (auto* Map = Node->GetMap()) {
                        for (uint32 k = 0; k < Rm; ++k) {
                            FName K;
                            Reader << K;
                            Map->erase(K);
                        }
                        for (uint32 k = 0; k < Ad; ++k) {
                            FName K;
                            FDaxNodeID C;
                            Reader << K;
                            Reader << C;
                            (*Map)[K] = C;
                            Allocator.UpdateParentEdgeMap(C, K);
                        }
                        for (uint32 k = 0; k < Rb; ++k) {
                            FName K;
                            FDaxNodeID C;
                            Reader << K;
                            Reader << C;
                            (*Map)[K] = C;
                            Allocator.UpdateParentEdgeMap(C, K);
                        }
                    }
                    else {
                        for (uint32 k = 0; k < Rm; ++k) {
                            FName K;
                            Reader << K;
                        }
                        for (uint32 k = 0; k < Ad; ++k) {
                            FName K;
                            FDaxNodeID C;
                            Reader << K;
                            Reader << C;
                        }
                        for (uint32 k = 0; k < Rb; ++k) {
                            FName K;
                            FDaxNodeID C;
                            Reader << K;
                            Reader << C;
                        }
                    }
                }
                else {
                    for (uint32 k = 0; k < Rm; ++k) {
                        FName K;
                        Reader << K;
                    }
                    for (uint32 k = 0; k < Ad; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                    }
                    for (uint32 k = 0; k < Rb; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                    }
                }
            }
            if (Node) Allocator.UpdateValueType(NodeID, FDaxFakeTypeMap::StaticStruct());
            bLocalDataChanged = true;
            bLocalStructChanged = true;
        }
        else {
            if (ArzDax::DaxFlagHasValue(Flags)) {
                ConsumeValuePayload(TempType);
                if (Node) Allocator.UpdateValueType(NodeID, TempType);
                bLocalDataChanged = true;
            }
        }

        ChangedEvents.Add(FChangedEvent{NodeID, {}});
        const int32 EndBits = Reader.GetPosBits();
        DAX_NET_SYNC_LOG(Warning, "Delta-Add[{0}] bits {1}->{2} (+{3})", i, StartBits, EndBits, EndBits - StartBits);
    }

    for (uint32 i = 0; i < UpdatesCount; ++i) {
        FDaxNodeID NodeID;
        Reader << NodeID;
        uint8 Flags = 0;
        Reader << Flags;
        FDaxNodeID Parent{};
        TObjectPtr<const UScriptStruct> TempType = nullptr;
        if (ArzDax::DaxFlagHasParent(Flags)) {
            Reader << Parent;
            bLocalStructChanged = true;
        }
        if (ArzDax::DaxFlagHasType(Flags)) {
            UObject* TempTypeObject = nullptr;
            const bool bTypeMapped = DeltaParms.Map->SerializeObject(Reader, UScriptStruct::StaticClass(), TempTypeObject);
            if (!bTypeMapped) {
                DAX_NET_SYNC_LOG(Warning, "Delta-Update: Unmapped UScriptStruct for node {0}, flags={1}. Defer.", NodeID.ToString(), FString::Printf(TEXT("0x%X"), Flags));
                DeltaParms.bOutHasMoreUnmapped = true;
                return true;
            }
            TempType = Cast<UScriptStruct>(TempTypeObject);
            bLocalStructChanged = true;
        }
        Allocator.AllocateSlotAt(NodeID);
        if (auto InfoRef = Allocator.GetCommonInfoRef(NodeID); InfoRef.IsValid()) {
            if (InfoRef.pParent && ArzDax::DaxFlagHasParent(Flags)) *InfoRef.pParent = Parent;
            if (TempType) Allocator.UpdateValueType(NodeID, TempType);
        }
        const UScriptStruct* EffType = TempType ? TempType.Get() : Allocator.GetValueType(NodeID);
        ArzDax::FDaxNode* Node = Allocator.TryGetNode(NodeID);
        if (Node && Node->IsValue()) { CaptureOldIfValue(NodeID); }
        if (!EffType || EffType == FDaxFakeTypeEmpty::StaticStruct()) {
            if (Node) {
                Node->ResetToEmpty();
                Allocator.UpdateValueType(NodeID, FDaxFakeTypeEmpty::StaticStruct());
            }
            continue;
        }
        const bool IsArrayType = (EffType == FDaxFakeTypeArray::StaticStruct());
        const bool IsMapType = (EffType == FDaxFakeTypeMap::StaticStruct());
        if (ArzDax::DaxFlagHasValue(Flags)) {
            if (Node) {
                Node->SerializeValueData(Reader, DeltaParms.Map, EffType);
                Allocator.UpdateValueType(NodeID, EffType);
            }
            else {
                FInstancedStruct Temp;
                Temp.InitializeAs(EffType);
                if (EffType->GetCppStructOps()->HasNetSerializer()) {
                    bool Success = true;
                    EffType->GetCppStructOps()->NetSerialize(Reader, DeltaParms.Map, Success, Temp.GetMutableMemory());
                }
                else {
                    EffType->SerializeBin(Reader, Temp.GetMutableMemory());
                }
            }
            bLocalDataChanged = true;
        }
        else if (IsArrayType) {
            if (Node && !Node->IsArray()) { Node->ResetToEmptyArray(); }
            auto* Arr = Node ? Node->GetArray() : nullptr;
            if (ArzDax::DaxFlagIsCFull(Flags)) {
                uint32 Count = 0;
                Reader.SerializeIntPacked(Count);
                if (Arr) {
                    Arr->Empty();
                    Arr->Reserve(Count);
                    for (uint32 k = 0; k < Count; ++k) {
                        FDaxNodeID C;
                        Reader << C;
                        Arr->Add(C);
                    }
                    // 构建反向映射：数组（更新分支全量）
                    for (int32 iArr = 0; iArr < Arr->Num(); ++iArr) {
                        const FDaxNodeID Cid = (*Arr)[iArr];
                        if (Cid.IsValid()) Allocator.UpdateParentEdgeArray(Cid, (uint16)iArr);
                    }
                }
                else {
                    for (uint32 k = 0; k < Count; ++k) {
                        FDaxNodeID Dummy;
                        Reader << Dummy;
                    }
                }
                if (Node) Allocator.UpdateValueType(NodeID, FDaxFakeTypeArray::StaticStruct());
                bLocalStructChanged = true;
                bLocalDataChanged = true;
            }
            else if (ArzDax::DaxFlagHasCDelta(Flags)) {
                uint8 kind = 0;
                Reader << kind;
                if (kind == 1) {
                    if (Arr) Arr->Empty();
                }
                else {
                    uint32 Start = 0, OldCount = 0, NewCount = 0;
                    Reader.SerializeIntPacked(Start);
                    Reader.SerializeIntPacked(OldCount);
                    Reader.SerializeIntPacked(NewCount);
                    if (Arr) {
                        const int32 S = static_cast<int32>(Start);
                        const int32 OC = static_cast<int32>(OldCount);
                        const int32 NC = static_cast<int32>(NewCount);
                        if (S >= 0 && S <= Arr->Num()) {
                            const int32 Rem = FMath::Clamp(OC, 0, Arr->Num() - S);
                            if (Rem > 0) Arr->RemoveAt(S, Rem, EAllowShrinking::No);
                            for (int32 k = 0; k < NC; ++k) {
                                FDaxNodeID C;
                                Reader << C;
                                Arr->Insert(C, S + k);
                            }
                            // 刷新从 S 开始到末尾的反向映射
                            for (int32 iArr = S; iArr < Arr->Num(); ++iArr) {
                                const FDaxNodeID Cid = (*Arr)[iArr];
                                if (Cid.IsValid()) Allocator.UpdateParentEdgeArray(Cid, (uint16)iArr);
                            }
                        }
                        else {
                            for (uint32 k = 0; k < NewCount; ++k) {
                                FDaxNodeID C;
                                Reader << C;
                                Arr->Add(C);
                            }
                        }
                    }
                    else {
                        for (uint32 k = 0; k < NewCount; ++k) {
                            FDaxNodeID C;
                            Reader << C;
                        }
                    }
                }
                if (Node) Allocator.UpdateValueType(NodeID, FDaxFakeTypeArray::StaticStruct());
                bLocalStructChanged = true;
                bLocalDataChanged = true;
            }
        }
        else if (IsMapType) {
            if (Node && !Node->IsMap()) { Node->ResetToEmptyMap(); }
            auto* Map = Node ? Node->GetMap() : nullptr;
            if (ArzDax::DaxFlagIsCFull(Flags)) {
                uint32 Count = 0;
                Reader.SerializeIntPacked(Count);
                if (Map) {
                    Map->clear();
                    for (uint32 k = 0; k < Count; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                        (*Map)[K] = C;
                        Allocator.UpdateParentEdgeMap(C, K);
                    }
                }
                else {
                    for (uint32 k = 0; k < Count; ++k) {
                        FName K;
                        FDaxNodeID Dummy;
                        Reader << K;
                        Reader << Dummy;
                    }
                }
                if (Node) Allocator.UpdateValueType(NodeID, FDaxFakeTypeMap::StaticStruct());
                bLocalStructChanged = true;
                bLocalDataChanged = true;
            }
            else if (ArzDax::DaxFlagHasCDelta(Flags)) {
                uint32 Rm = 0, Ad = 0, Rb = 0;
                Reader.SerializeIntPacked(Rm);
                Reader.SerializeIntPacked(Ad);
                Reader.SerializeIntPacked(Rb);
                if (Map) {
                    for (uint32 k = 0; k < Rm; ++k) {
                        FName K;
                        Reader << K;
                        Map->erase(K);
                    }
                    for (uint32 k = 0; k < Ad; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                        (*Map)[K] = C;
                        Allocator.UpdateParentEdgeMap(C, K);
                    }
                    for (uint32 k = 0; k < Rb; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                        (*Map)[K] = C;
                        Allocator.UpdateParentEdgeMap(C, K);
                    }
                }
                else {
                    for (uint32 k = 0; k < Rm; ++k) {
                        FName K;
                        Reader << K;
                    }
                    for (uint32 k = 0; k < Ad; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                    }
                    for (uint32 k = 0; k < Rb; ++k) {
                        FName K;
                        FDaxNodeID C;
                        Reader << K;
                        Reader << C;
                    }
                }
                if (Node) Allocator.UpdateValueType(NodeID, FDaxFakeTypeMap::StaticStruct());
                bLocalStructChanged = true;
                bLocalDataChanged = true;
            }
        }
        else {
            // 仅 Meta 更新
        }
        ChangedEvents.Add(FChangedEvent{NodeID, {}});
    }

    if (bLocalStructChanged) {
        ++StructVersion;
        ++DataVersion;
    }
    else if (bLocalDataChanged) { ++DataVersion; }

    if (!OnChangedBindings.IsEmpty() && !ChangedEvents.IsEmpty()) {
        TSet<FDaxNodeID> Seen;
        TArray<FChangedEvent> FinalEvents;
        FinalEvents.Reserve(ChangedEvents.Num());
        for (FChangedEvent& E : ChangedEvents) {
            const FDaxNodeID Key = E.ID;
            if (Seen.Contains(Key)) continue;
            Seen.Add(Key);
            FinalEvents.Add(MoveTemp(E));
        }
        if (OnChangedIndexStructVersion != StructVersion) {
            RebuildOnChangedIndex();
            OnChangedIndexStructVersion = StructVersion;
        }
        
        PathCache.clear();
        auto DispatchToBinding = [&](int32 BindingIndex, int32 DepthDiff, const FChangedEvent& E) {
            if (!OnChangedBindings.IsValidIndex(BindingIndex)) return;
            const FDaxOnChangedBinding& B = OnChangedBindings[BindingIndex];
            if (!B.IsValid() || DepthDiff < 0 || DepthDiff > B.Depth) return;
            const TArray<TVariant<FName, int32>>* PathPtr = &E.Path;
            if (PathPtr->Num() == 0) {
                if (auto Existing = PathCache.findPtr(E.ID)) { PathPtr = Existing; }
                else {
                    TArray<TVariant<FName, int32>> NewPath;
                    if (BuildPathFromNodeID(E.ID, NewPath)) {
                        PathCache.emplace(E.ID, NewPath);
                        PathPtr = PathCache.findPtr(E.ID);
                    }
                    else { return; }
                }
            }
            FDaxVisitor ChangePos(this, LiveToken, *PathPtr);
            B.Delegate.ExecuteIfBound(ChangePos);
        };
        for (const FChangedEvent& E : FinalEvents) {
            int32 Dist = 0;
            FDaxNodeID Cur = E.ID;
            while (Cur.IsValid()) {
                if (const TArray<int32>* List = OnChangedIndex.findPtr(Cur)) {
                    for (int32 BindingIndex : *List) {
                        DispatchToBinding(BindingIndex, Dist, E);
                    }
                }
                Cur = GetNodeParent(Cur);
                ++Dist;
            }
        }
    }
    DAX_NET_SYNC_LOG(Warning, "DaxSet::Sync_ClientDeltaSyncImpl End");
    DAX_NET_SYNC_LOG(Warning, "Delta ReaderBits pos={0}/{1}", Reader.GetPosBits(), Reader.GetNumBits());
    return true;
}