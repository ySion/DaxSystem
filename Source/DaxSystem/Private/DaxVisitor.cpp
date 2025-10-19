#include "DaxSystem/Public/DaxVisitor.h"
#include "DaxSystem/Private/DaxVisitor.inl"
#include "DaxSystem/Public/DaxComponent.h"
#include "DaxSystem/Private/DaxNode.h"
#include "DaxSystem/Private/DaxSet.h"
#include "DaxSystem/Public/DaxBuiltinTypes.h"

#include "String/ParseTokens.h"

bool FDaxVisitor::HasData() const {
    return ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk();
}

bool FDaxVisitor::IsAncestor(const FDaxVisitor& Other) const {
    // 必须同一 Set 与 LiveToken
    if (!IsValid() || !Other.IsValid()) return false;
    if (TargetLiveToken.Pin().Get() != Other.TargetLiveToken.Pin().Get()) return false;

    // 仅做只读解析，保证路径有效即可（不产生副作用）
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return false;
    if (!Other.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return false;

    const int32 A = NodePath.Num();
    const int32 B = Other.NodePath.Num();
    if (A >= B) return false; // 严格祖先（排除自身与相等长度）

    for (int32 i = 0; i < A; ++i) {
        const auto& S1 = NodePath[i];
        const auto& S2 = Other.NodePath[i];
        if (S1.GetIndex() != S2.GetIndex()) return false;
        if (S1.GetIndex() == 0) {
            if (S1.Get<FName>() != S2.Get<FName>()) return false;
        }
        else {
            if (S1.Get<int32>() != S2.Get<int32>()) return false;
        }
    }
    return true;
}

bool FDaxVisitor::IsAEmptyMap() const {
    return ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && CachedNode && CachedNode->IsEmptyMap();
}

bool FDaxVisitor::IsAEmptyArray() const {
    return ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && CachedNode && CachedNode->IsEmptyArray();
}

bool FDaxVisitor::IsAEmptyNode() const {
    return ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && CachedNode && CachedNode->IsEmpty();
}

bool FDaxVisitor::IsANoEmptyMap() const {
    return ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && CachedNode && CachedNode->IsMap() && !CachedNode->IsEmptyMap();
}

bool FDaxVisitor::IsANoEmptyArray() const {
    return ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && CachedNode && CachedNode->IsArray() && !CachedNode->IsEmptyArray();
}

int32 FDaxVisitor::GetDepthRelativeTo(const FDaxVisitor& Ancestor) const {
    // 前置：需同一Set与LiveToken
    if (!IsValid() || !Ancestor.IsValid()) return -1;
    if (TargetLiveToken.Pin().Get() != Ancestor.TargetLiveToken.Pin().Get()) return -1;
    // 祖先判定由 IsAncestor 保证严格祖先
    if (!Ancestor.IsAncestor(*this)) return -1;
    return NodePath.Num() - Ancestor.NodePath.Num();
}

FDaxVisitor FDaxVisitor::MakeVisitorToParent() const {
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    FDaxVisitor NewVisitor{};
    NewVisitor.TargetSet = this->TargetSet;
    NewVisitor.TargetLiveToken = this->TargetLiveToken;
    NewVisitor.NodePath = this->NodePath;
    if (NewVisitor.NodePath.Num() > 0) {
        NewVisitor.NodePath.RemoveAt(NewVisitor.NodePath.Num() - 1);
        return NewVisitor;
    }
    else {
        return {}; // 已经是根节点，无法再上移
    }
}

FDaxVisitor FDaxVisitor::MakeVisitorByName(FName Key) const {
    if (!IsValid()) return FDaxVisitor(); // 无效访问器
    FDaxVisitor NewVisitor{};
    NewVisitor.TargetSet = this->TargetSet;
    NewVisitor.TargetLiveToken = this->TargetLiveToken;
    NewVisitor.NodePath = this->NodePath;
    NewVisitor.NodePath.Add(TVariant<FName, int32>(TInPlaceType<FName>(), Key));

    if (CachedSetStructVersion == TargetSet->StructVersion) {
        if (CachedNode) {
            if (auto* MapData = CachedNode->GetMap()) {
                auto It = MapData->find(Key);
                if (It != MapData->end()) {
                    const FDaxNodeID NodeID = It->second;
                    NewVisitor.CachedNodeID = NodeID;
                    NewVisitor.CachedNode = TargetSet->TryGetNode(NodeID);
                    return NewVisitor;
                }
            }
        }
    }

    return NewVisitor;
}

FDaxVisitor FDaxVisitor::MakeVisitorByIndex(int32 Index) const {
    if (!IsValid()) return FDaxVisitor(); // 无效访问器
    FDaxVisitor NewVisitor{};
    NewVisitor.TargetSet = this->TargetSet;
    NewVisitor.TargetLiveToken = this->TargetLiveToken;
    NewVisitor.NodePath = this->NodePath;
    NewVisitor.NodePath.Add(TVariant<FName, int32>(TInPlaceType<int32>(), Index));

    if (CachedSetStructVersion == TargetSet->StructVersion) {
        if (CachedNode) {
            if (auto* ArrayData = CachedNode->GetArray()) {
                if (ArrayData->IsValidIndex(Index)) {
                    const FDaxNodeID NodeID = (*ArrayData)[Index];
                    NewVisitor.CachedNodeID = NodeID;
                    NewVisitor.CachedNode = TargetSet->TryGetNode(NodeID);
                    return NewVisitor;
                }
            }
        }
    }

    return NewVisitor;
}

FDaxVisitor FDaxVisitor::MakeVisitorByFullPath(const TArray<TVariant<FName, int32>>& Path) {
    if (!IsValid()) return FDaxVisitor(); // 无效访问器
    FDaxVisitor NewVisitor{};
    NewVisitor.TargetSet = this->TargetSet;
    NewVisitor.TargetLiveToken = this->TargetLiveToken;
    NewVisitor.NodePath = Path;

    return NewVisitor;
}

FDaxVisitor FDaxVisitor::MakeVisitorByParsePath(const FString& FullPath) {
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};

    FDaxVisitor NewVisitor{};
    NewVisitor.TargetSet = this->TargetSet;
    NewVisitor.TargetLiveToken = this->TargetLiveToken;
    NewVisitor.NodePath = this->NodePath;

    auto TryParseDollarIndex = [](FStringView Seg, int32& OutIndex)-> bool {
        if (Seg.Len() < 2 || Seg[0] != TEXT('$')) return false;
        int64 acc = 0;
        for (TCHAR c : Seg.RightChop(1)) {
            if (!TChar<TCHAR>::IsDigit(c)) return false;
            acc = acc * 10 + (c - '0');
            if (acc > TNumericLimits<int32>::Max()) return false;
        }
        OutIndex = static_cast<int32>(acc);
        return true;
    };

    if (FullPath.IsEmpty()) {
        UE_LOGFMT(DataXSystem, Warning, "FDaxVisitor::MakeVisitorByParsePath - Empty path");
        return {};
    }

    int32 SlashIdx = INDEX_NONE;
    if (!FullPath.FindChar(TEXT('/'), SlashIdx)) {
        const FStringView Seg(FullPath);
        if (Seg.IsEmpty()) {
            UE_LOGFMT(DataXSystem, Warning,
                      "FDaxVisitor::MakeVisitorByParsePath - Invalid empty segment in path: {0}", FullPath);
            return {};
        }

        int32 Index = 0;
        if (TryParseDollarIndex(Seg, Index))
            NewVisitor.NodePath.Emplace(TInPlaceType<int32>(), Index);
        else
            NewVisitor.NodePath.Emplace(TInPlaceType<FName>(), FName(Seg));

        return NewVisitor;
    }


    bool bOk = true;

    ParseTokens(FStringView(FullPath), TEXT('/'),
                [&](FStringView Seg) {
                    if (!bOk) return;

                    if (Seg.IsEmpty()) {
                        UE_LOGFMT(DataXSystem, Warning,
                                  "FDaxVisitor::MakeVisitorByParsePath - Invalid empty segment in path: {0}", FullPath);
                        bOk = false;
                        return;
                    }

                    int32 Index = 0;
                    if (TryParseDollarIndex(Seg, Index)) {
                        NewVisitor.NodePath.Emplace(TInPlaceType<int32>(), Index);
                    }
                    else {
                        NewVisitor.NodePath.Emplace(TInPlaceType<FName>(), FName(Seg));
                    }
                },
                UE::String::EParseTokensOptions::None
    );

    return bOk ? NewVisitor : FDaxVisitor{};
}

FDaxResultDetail FDaxVisitor::EnsureAndCopyFrom(const FDaxVisitor& Src) const {
    if (!TargetSet->bRunningOnServer) return EDaxResult::PermissionDenied;
    if (!ResolvePathInternal(EDaxPathResolveMode::EnsureCreate).IsOk()) return EDaxResult::InvalidVisitor;
    if (!Src.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return EDaxResult::InvalidVisitor;
    if (this->CachedNodeID == Src.CachedNodeID) return EDaxResult::SameValueNotChange; // 自拷贝无效
    if (!TargetSet->CopyNode(CachedNodeID, *Src.TargetSet, Src.CachedNodeID)) {
        return EDaxResult::UnknownFailure;
    }
    return EDaxResult::Success;
}

FDaxResultDetail FDaxVisitor::EnsureAndSwapWith(const FDaxVisitor& Src) const {
    if (!TargetSet->bRunningOnServer) return EDaxResult::PermissionDenied;
    if (!ResolvePathInternal(EDaxPathResolveMode::EnsureCreate).IsOk()) return EDaxResult::InvalidVisitor;
    if (!Src.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return EDaxResult::InvalidVisitor;
    if (this->CachedNodeID == Src.CachedNodeID) return EDaxResult::SameValueNotChange;
    if (!TargetSet->SwapNode(CachedNodeID, *Src.TargetSet, Src.CachedNodeID)) {
        return EDaxResult::UnknownFailure;
    }
    return EDaxResult::Success;
}

FDaxVisitor FDaxVisitor::SearchEmptyChildNode() const {
    return SearchChildBy([](const ArzDax::FDaxNode* ChildNode) {
        return ChildNode->IsEmpty();
    });
}

FDaxVisitor FDaxVisitor::SearchEmptyChildMapNode() const {
    return SearchChildBy([](const ArzDax::FDaxNode* ChildNode) {
        return ChildNode->IsEmptyMap();
    });
}

FDaxVisitor FDaxVisitor::SearchEmptyChildArrayNode() const {
    return SearchChildBy([](const ArzDax::FDaxNode* ChildNode) {
        return ChildNode->IsEmptyArray();
    });
}

FDaxResultDetail FDaxVisitor::TrySetToEmpty() const {
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    // 服务端：直接重置权威节点为空（会释放子树并更新类型/版本）
    if (TargetSet->bRunningOnServer) {
        return TargetSet->ResetToEmpty(CachedNodeID);
    }

    // 客户端：不允许改动权威结构，仅在 Overlay 上覆写为空以达成交互上的“清空”效果
    // 若底层本就为空，则避免无意义创建 Overlay
    if (TargetSet->OverlayMap.find(CachedNodeID) == TargetSet->OverlayMap.end()) {
        if (auto* BaseNode = TargetSet->Allocator.TryGetNode(CachedNodeID)) {
            if (BaseNode->IsEmpty()) return {}; // 与底层一致，无需写 Overlay
        }
    }

    return TargetSet->GetOrCreateOverlayValueNode(CachedNodeID)->ResetToEmpty();
}

FConstStructView FDaxVisitor::TryGetValue(const UScriptStruct* ValueType) const {
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    return CachedNode->TryGetValue(ValueType);
}

FConstStructView FDaxVisitor::TryGetValueGeneric() const {
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    return CachedNode->TryGetValueGeneric();
}

FDaxResultDetail FDaxVisitor::TrySetValue(const FConstStructView Value) const {
    const auto Res = ResolvePathInternal(EDaxPathResolveMode::ReadOnly);
    if (!Res.IsOk()) return Res;
    if (TargetSet->bRunningOnServer) {
        switch (const auto SetRes = CachedNode->TrySetValue(Value)) {
        case EDaxResult::SuccessChangeValue:
        case EDaxResult::SuccessOverrideEmpty:
            TargetSet->BumpNodeType(CachedNodeID, Value.GetScriptStruct());
            TargetSet->BumpNodeDataVersion(CachedNodeID);
            return SetRes;
        case EDaxResult::SuccessChangeValueAndType:
            TargetSet->BumpNodeType(CachedNodeID, Value.GetScriptStruct());
            TargetSet->BumpNodeDataVersionAndStruct(CachedNodeID);
            return SetRes;
        default:
            return SetRes;
        }
    }
    else {
        // 客户端：若无 Overlay，则先与底层权威值比较，相同则早退且不创建 Overlay
        if (TargetSet->OverlayMap.find(CachedNodeID) == TargetSet->OverlayMap.end()) {
            if (auto* BaseNode = TargetSet->Allocator.TryGetNode(CachedNodeID)) {
                const UScriptStruct* SS = Value.GetScriptStruct();
                if (SS) {
                    if (FConstStructView Curr = BaseNode->TryGetValue(SS); Curr.IsValid()) {
                        if (SS->CompareScriptStruct(Curr.GetMemory(), Value.GetMemory(), PPF_None)) {
                            return EDaxResult::SameValueNotChange;
                        }
                    }
                }
            }
        }
        // 发生实际差异时，才创建 Overlay 并写入
        return TargetSet->GetOrCreateOverlayValueNode(CachedNodeID)->TrySetValue(Value);
    }
}

FDaxResultDetail FDaxVisitor::EnsureAndSetValue(const FConstStructView Value) const {
    const auto Res = ResolvePathInternal(EDaxPathResolveMode::EnsureCreate);
    if (!Res.IsOk()) return Res;
    if (TargetSet->bRunningOnServer) {
        switch (const FDaxResultDetail SetRes = CachedNode->TrySetValue(Value)) {
        case EDaxResult::SuccessChangeValue:
        case EDaxResult::SuccessOverrideEmpty:
            TargetSet->BumpNodeType(CachedNodeID, Value.GetScriptStruct());
            TargetSet->BumpNodeDataVersion(CachedNodeID);
            return SetRes;
        case EDaxResult::SuccessChangeValueAndType:
            TargetSet->BumpNodeType(CachedNodeID, Value.GetScriptStruct());
            TargetSet->BumpNodeDataVersionAndStruct(CachedNodeID);
            return SetRes;
        default:
            return SetRes;
        }
    }
    else { // Client
        // 客户端：若无 Overlay，则先与底层权威值比较，相同则早退且不创建 Overlay
        if (TargetSet->OverlayMap.find(CachedNodeID) == TargetSet->OverlayMap.end()) {
            if (auto* BaseNode = TargetSet->Allocator.TryGetNode(CachedNodeID)) {
                const UScriptStruct* SS = Value.GetScriptStruct();
                if (SS) {
                    if (FConstStructView Curr = BaseNode->TryGetValue(SS); Curr.IsValid()) {
                        if (SS->CompareScriptStruct(Curr.GetMemory(), Value.GetMemory(), PPF_None)) {
                            return EDaxResult::SameValueNotChange;
                        }
                    }
                }
            }
        }
        // 发生实际差异时，才创建 Overlay 并写入
        return TargetSet->GetOrCreateOverlayValueNode(CachedNodeID)->TrySetValue(Value);
    }
}

FDaxVisitor FDaxVisitor::EnsureArray() const {
    if (!IsValid()) return {};
    if (!ResolvePathInternal(EDaxPathResolveMode::EnsureCreate).IsOk()) return {};
    if (CachedNode->IsArray()) return *this;
    if (TargetSet->bRunningOnServer && CachedNode->IsEmpty() && TargetSet->ResetToEmptyArray(CachedNodeID).IsOk())
        return *this;
    return {};
}

FDaxVisitor FDaxVisitor::EnsureMap() const {
    if (!IsValid()) return {};
    if (!ResolvePathInternal(EDaxPathResolveMode::EnsureCreate).IsOk()) return {};
    if (CachedNode->IsMap()) return *this;
    if (TargetSet->bRunningOnServer && CachedNode->IsEmpty() && TargetSet->ResetToEmptyMap(CachedNodeID).IsOk())
        return
            *this;
    return {};
}

FConstStructView FDaxVisitor::TryGetOldValue(const UScriptStruct* ValueType) const {
    if (IsValid() && OldNodeID.IsValid()) {
        auto Temp = TargetSet->TryGetOldValueByNodeID(OldNodeID);
        if (Temp.IsValid() && Temp.GetScriptStruct() == ValueType) {
            return Temp;
        }
    }
    return {};
}

DAX_BUILTIN_TYPE_DEFINE(bool, bool Value, Bool, FDaxBuiltinBool)
DAX_BUILTIN_TYPE_DEFINE(int64, int64 Value, Int, FDaxBuiltinInt64)
DAX_BUILTIN_TYPE_DEFINE(float, float Value, Float, FDaxBuiltinFloat)
DAX_BUILTIN_TYPE_DEFINE(FName, const FName& Value, Name, FDaxBuiltinName)
DAX_BUILTIN_TYPE_DEFINE(FString, const FString& Value, String, FDaxBuiltinString)
DAX_BUILTIN_TYPE_DEFINE(FVector, const FVector& Value, Vector, FDaxBuiltinVector)
DAX_BUILTIN_TYPE_DEFINE(FRotator, const FRotator& Value, Rotator, FDaxBuiltinRotator)

// ================= Array 操作 =================
FDaxVisitor FDaxVisitor::ArrayAdd() const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return {};

    // 确保容器是 Array（空节点则重置为 Array）
    FDaxVisitor Base = EnsureArray();
    if (!Base.IsValid()) return {};
    if (!Base.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (!Base.CachedNode || !Base.CachedNode->IsArray()) return {};

    auto* Arr = Base.CachedNode->GetArray();
    if (!Arr) return {};

    const FDaxNodeID ChildID = TargetSet->Allocator.Allocate();
    Arr->Add(ChildID);

    if (auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(ChildID); InfoRef.IsValid() && InfoRef.pParent) {
        *InfoRef.pParent = Base.CachedNodeID;
    }
    // 维护反向映射：数组下标
    TargetSet->Allocator.UpdateParentEdgeArray(ChildID, (uint16)(Arr->Num() - 1));

    TargetSet->BumpNodeDataVersionAndStruct(Base.CachedNodeID);
    TargetSet->BumpNodeDataVersionAndStruct(ChildID);

    // 返回新增元素的访问器
    FDaxVisitor ChildVisitor{};
    ChildVisitor.TargetSet = TargetSet;
    ChildVisitor.TargetLiveToken = TargetLiveToken;
    ChildVisitor.NodePath = NodePath;
    ChildVisitor.NodePath.Add(TVariant<FName, int32>(TInPlaceType<int32>(), Arr->Num() - 1));
    ChildVisitor.CachedNodeID = ChildID;
    ChildVisitor.CachedNode = TargetSet->TryGetNode(ChildID);
    return ChildVisitor;
}

FDaxResultDetail FDaxVisitor::ArrayEnsureMinNum(int32 Count) const {
    if (Count < 0) return EDaxResult::InvalidTargetValue;
    // 仅服务端允许修改容器结构
    if (!IsValid()) return EDaxResult::InvalidNode;
    if (!TargetSet->bRunningOnServer) return EDaxResult::PermissionDenied;

    // 确保存在/类型正确（空节点可转Array；非空非Array时失败）
    if (auto Res = ResolvePathInternal(EDaxPathResolveMode::EnsureCreate); !Res.IsOk()) return Res;
    if (!CachedNode) return EDaxResult::InvalidNode;

    if (!CachedNode->IsArray()) {
        if (CachedNode->IsEmpty()) {
            // 空 -> Array
            const FDaxResultDetail R = TargetSet->ResetToEmptyArray(CachedNodeID);
            if (!R.IsOk()) return R;
        }
        else {
            return EDaxResult::ValueTypeMismatch;
        }
    }

    auto* Arr = CachedNode->GetArray();
    if (!Arr) return EDaxResult::ResolveInternalNullArray;

    const int32 Curr = Arr->Num();

    // 不缩减：当 Count <= Curr 时视为无变化
    if (Count <= Curr) return EDaxResult::SameValueNotChange;

    // 扩充到 Count
    const int32 AddCount = Count - Curr;
    Arr->Reserve(Count);
    for (int32 i = 0; i < AddCount; ++i) {
        const FDaxNodeID ChildID = TargetSet->Allocator.Allocate();
        Arr->Add(ChildID);
        if (auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(ChildID); InfoRef.IsValid() && InfoRef.pParent) {
            *InfoRef.pParent = CachedNodeID;
        }
        // 新增子：设置反向映射（数组下标）
        TargetSet->Allocator.UpdateParentEdgeArray(ChildID, (uint16)(Curr + i));
        TargetSet->BumpNodeDataVersionAndStruct(ChildID);
    }
    TargetSet->BumpNodeDataVersionAndStruct(CachedNodeID);
    return EDaxResult::SuccessChangeValue;
}


bool FDaxVisitor::ArrayRemove() const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return false;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return false;
    if (!CachedNode || !CachedNode->IsArray()) return false;

    auto* Arr = CachedNode->GetArray();
    if (!Arr || Arr->Num() <= 0) return true; // 空数组，无操作

    const int32 LastIndex = Arr->Num() - 1;
    const FDaxNodeID ChildID = (*Arr)[LastIndex];
    Arr->RemoveAt(LastIndex, 1, EAllowShrinking::No);

    // 释放被移除子树并更新版本
    if (ChildID.IsValid()) {
        TargetSet->ReleaseRecursive(ChildID);
    }
    TargetSet->BumpNodeDataVersionAndStruct(CachedNodeID);

    return true;
}

FDaxVisitor FDaxVisitor::ArrayGet(int32 Index) const {
    return MakeVisitorByIndex(Index);
}

FDaxVisitor FDaxVisitor::ArrayInsert(int32 Index) const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return {};

    FDaxVisitor Base = EnsureArray();
    if (!Base.IsValid()) return {};
    if (!Base.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (!Base.CachedNode || !Base.CachedNode->IsArray()) return {};

    auto* Arr = Base.CachedNode->GetArray();
    if (!Arr) return {};

    // 插入位置修正到 [0, Num]
    const int32 N = Arr->Num();
    if (Index < 0) return {};
    int32 InsertAt = FMath::Clamp(Index, 0, N);

    const FDaxNodeID ChildID = TargetSet->Allocator.Allocate();
    Arr->Insert(ChildID, InsertAt);

    if (auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(ChildID); InfoRef.IsValid() && InfoRef.pParent) {
        *InfoRef.pParent = Base.CachedNodeID;
    }

    // 维护反向映射：插入点及其后的所有元素下标
    TargetSet->Allocator.UpdateParentEdgeArray(ChildID, (uint16)InsertAt);
    for (int32 i = InsertAt + 1; i < Arr->Num(); ++i) {
        const FDaxNodeID Sid = (*Arr)[i];
        if (Sid.IsValid()) TargetSet->Allocator.UpdateParentEdgeArray(Sid, (uint16)i);
    }

    TargetSet->BumpNodeDataVersionAndStruct(Base.CachedNodeID);
    TargetSet->BumpNodeDataVersionAndStruct(ChildID);

    // 返回插入位置的新元素访问器
    FDaxVisitor ChildVisitor{};
    ChildVisitor.TargetSet = TargetSet;
    ChildVisitor.TargetLiveToken = TargetLiveToken;
    ChildVisitor.NodePath = NodePath;
    ChildVisitor.NodePath.Add(TVariant<FName, int32>(TInPlaceType<int32>(), InsertAt));
    ChildVisitor.CachedNodeID = ChildID;
    ChildVisitor.CachedNode = TargetSet->TryGetNode(ChildID);
    return ChildVisitor;
}

int32 FDaxVisitor::ArrayNum() const {
    if (!IsValid()) return 0;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return 0;
    if (!CachedNode || !CachedNode->IsArray()) return 0;
    const auto* Arr = CachedNode->GetArray();
    return Arr ? Arr->Num() : 0;
}

bool FDaxVisitor::ArrayClear() const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return false;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return false;
    if (!CachedNode || !CachedNode->IsArray()) return false;

    return TargetSet->ReleaseChildren(CachedNodeID) > 0;
}

TArray<FDaxVisitor> FDaxVisitor::ArrayGetChildren() const {
    TArray<FDaxVisitor> Out;
    if (!IsValid()) return Out;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return Out;
    if (!CachedNode || !CachedNode->IsArray()) return Out;

    const auto* Arr = CachedNode->GetArray();
    if (!Arr) return Out;

    Out.Reserve(Arr->Num());
    for (int32 i = 0; i < Arr->Num(); ++i) {
        const FDaxNodeID ChildID = (*Arr)[i];
        FDaxVisitor V{};
        V.TargetSet = TargetSet;
        V.TargetLiveToken = TargetLiveToken;
        V.NodePath = NodePath;
        V.NodePath.Add(TVariant<FName, int32>(TInPlaceType<int32>(), i));
        V.CachedNodeID = ChildID;
        V.CachedNode = TargetSet->TryGetNode(ChildID);
        Out.Add(MoveTemp(V));
    }
    return Out;
}


FDaxVisitor FDaxVisitor::ArrayGetPrev() const {
    // 只在数组下有效：最后一段必须是索引
    if (!IsValid()) return {};
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (NodePath.IsEmpty()) return {};

    const TVariant<FName, int32>& Last = NodePath.Last();
    const int32* IndexPtr = Last.TryGet<int32>();
    if (!IndexPtr) return {}; // 尾段不是索引 => 不是数组子节点

    const int32 CurrIndex = *IndexPtr;
    if (CurrIndex <= 0) return {};

    // 父访问器
    FDaxVisitor Parent{};
    Parent.TargetSet = TargetSet;
    Parent.TargetLiveToken = TargetLiveToken;
    Parent.NodePath = NodePath;
    Parent.NodePath.Pop(); // 去掉最后一段

    if (!Parent.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (!Parent.CachedNode || !Parent.CachedNode->IsArray()) return {};

    auto* Arr = Parent.CachedNode->GetArray();
    if (!Arr) return {};
    const int32 PrevIndex = CurrIndex - 1;
    if (!Arr->IsValidIndex(PrevIndex)) return {};

    const FDaxNodeID SiblingID = (*Arr)[PrevIndex];

    FDaxVisitor Sibling{};
    Sibling.TargetSet = TargetSet;
    Sibling.TargetLiveToken = TargetLiveToken;
    Sibling.NodePath = Parent.NodePath;
    Sibling.NodePath.Add(TVariant<FName, int32>(TInPlaceType<int32>(), PrevIndex));
    Sibling.CachedNodeID = SiblingID;
    Sibling.CachedNode = TargetSet->TryGetNode(SiblingID);
    return Sibling;
}

FDaxVisitor FDaxVisitor::ArrayGetNext() const {
    // 只在数组下有效：最后一段必须是索引
    if (!IsValid()) return {};
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (NodePath.IsEmpty()) return {};

    const TVariant<FName, int32>& Last = NodePath.Last();
    const int32* IndexPtr = Last.TryGet<int32>();
    if (!IndexPtr) return {}; // 尾段不是索引 => 不是数组子节点

    const int32 CurrIndex = *IndexPtr;

    // 父访问器
    FDaxVisitor Parent{};
    Parent.TargetSet = TargetSet;
    Parent.TargetLiveToken = TargetLiveToken;
    Parent.NodePath = NodePath;
    Parent.NodePath.Pop(); // 去掉最后一段

    if (!Parent.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (!Parent.CachedNode || !Parent.CachedNode->IsArray()) return {};

    auto* Arr = Parent.CachedNode->GetArray();
    if (!Arr) return {};
    const int32 NextIndex = CurrIndex + 1;
    if (!Arr->IsValidIndex(NextIndex)) return {};

    const FDaxNodeID SiblingID = (*Arr)[NextIndex];

    FDaxVisitor Sibling{};
    Sibling.TargetSet = TargetSet;
    Sibling.TargetLiveToken = TargetLiveToken;
    Sibling.NodePath = Parent.NodePath;
    Sibling.NodePath.Add(TVariant<FName, int32>(TInPlaceType<int32>(), NextIndex));
    Sibling.CachedNodeID = SiblingID;
    Sibling.CachedNode = TargetSet->TryGetNode(SiblingID);
    return Sibling;
}

// ================= Map 操作 =================
FDaxVisitor FDaxVisitor::MapAdd(FName Key) const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return {};

    FDaxVisitor Base = EnsureMap();
    if (!Base.IsValid()) return {};
    if (!Base.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return {};
    if (!Base.CachedNode || !Base.CachedNode->IsMap()) return {};

    auto* Map = Base.CachedNode->GetMap();
    if (!Map) return {};

    FDaxNodeID ChildID{};
    if (auto It = Map->find(Key); It != Map->end() && TargetSet->IsNodeValid(It->second)) {
        ChildID = It->second; // 已存在则直接返回该子节点
        // 确保反向映射正确
        TargetSet->Allocator.UpdateParentEdgeMap(ChildID, Key);
    }
    else {
        ChildID = TargetSet->Allocator.Allocate();
        (*Map)[Key] = ChildID;
        if (auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(ChildID); InfoRef.IsValid() && InfoRef.pParent) {
            *InfoRef.pParent = Base.CachedNodeID;
        }
        // 维护反向映射：Map 的键
        TargetSet->Allocator.UpdateParentEdgeMap(ChildID, Key);
        TargetSet->BumpNodeDataVersionAndStruct(Base.CachedNodeID);
        TargetSet->BumpNodeDataVersionAndStruct(ChildID);
    }

    FDaxVisitor ChildVisitor{};
    ChildVisitor.TargetSet = TargetSet;
    ChildVisitor.TargetLiveToken = TargetLiveToken;
    ChildVisitor.NodePath = NodePath;
    ChildVisitor.NodePath.Add(TVariant<FName, int32>(TInPlaceType<FName>(), Key));
    ChildVisitor.CachedNodeID = ChildID;
    ChildVisitor.CachedNode = TargetSet->TryGetNode(ChildID);
    return ChildVisitor;
}

bool FDaxVisitor::MapRemove(FName Key) const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return false;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return false;
    if (!CachedNode || !CachedNode->IsMap()) return false;

    auto* Map = CachedNode->GetMap();
    if (!Map) return false;

    auto It = Map->find(Key);
    if (It != Map->end()) {
        const FDaxNodeID ChildID = It->second;
        Map->erase(It);
        if (ChildID.IsValid()) TargetSet->ReleaseRecursive(ChildID);
        TargetSet->BumpNodeDataVersionAndStruct(CachedNodeID);
    }

    return true;
}

FDaxVisitor FDaxVisitor::MapGet(FName Key) const {
    return MakeVisitorByName(Key);
}

int32 FDaxVisitor::MapNum() const {
    if (!IsValid()) return 0;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return 0;
    if (!CachedNode || !CachedNode->IsMap()) return 0;
    const auto* Map = CachedNode->GetMap();
    return Map ? static_cast<int32>(Map->size()) : 0;
}

bool FDaxVisitor::MapClear() const {
    // 仅服务端允许修改容器结构
    if (!IsValid() || !TargetSet->bRunningOnServer) return false;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return false;
    if (!CachedNode || !CachedNode->IsMap()) return false;
    return TargetSet->ReleaseChildren(CachedNodeID) > 0;
}

TArray<FDaxVisitor> FDaxVisitor::MapGetChildren() const {
    TArray<FDaxVisitor> Out;
    if (!IsValid()) return Out;
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return Out;
    if (!CachedNode || !CachedNode->IsMap()) return Out;

    const auto* Map = CachedNode->GetMap();
    if (!Map) return Out;

    Out.Reserve(static_cast<int32>(Map->size()));
    for (const auto& KV : *Map) {
        const FName K = KV.first;
        const FDaxNodeID ChildID = KV.second;
        FDaxVisitor V{};
        V.TargetSet = TargetSet;
        V.TargetLiveToken = TargetLiveToken;
        V.NodePath = NodePath;
        V.NodePath.Add(TVariant<FName, int32>(TInPlaceType<FName>(), K));
        V.CachedNodeID = ChildID;
        V.CachedNode = TargetSet->TryGetNode(ChildID);
        Out.Add(MoveTemp(V));
    }
    return Out;
}

// ==================== 路径位置查询 ====================
int32 FDaxVisitor::GetIndexInParentArray() const {
    // 尾段为 Index 才可能位于父数组中
    if (!IsValid()) return -1;
    if (NodePath.IsEmpty()) return -1;

    const TVariant<FName, int32>& Last = NodePath.Last();
    const int32* IndexPtr = Last.TryGet<int32>();
    if (!IndexPtr) return -1;

    // 解析当前与父容器，确保结构有效
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return -1;

    // 快路径：使用反向映射
    if (TargetSet) {
        const auto Kind = TargetSet->Allocator.GetParentEdgeKind(CachedNodeID);
        if (Kind == ArzDax::EDaxParentEdgeKind::Array) {
            const uint16 EdgeIndex = TargetSet->Allocator.GetParentEdgeIndex(CachedNodeID);
            // 校验：读取父数组，并确认匹配
            FDaxVisitor Parent{};
            Parent.TargetSet = TargetSet;
            Parent.TargetLiveToken = TargetLiveToken;
            Parent.NodePath = NodePath;
            Parent.NodePath.Pop();
            if (Parent.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && Parent.CachedNode && Parent.CachedNode->IsArray()) {
                const auto* Arr = Parent.CachedNode->GetArray();
                if (Arr && Arr->IsValidIndex((int32)EdgeIndex) && (*Arr)[EdgeIndex] == CachedNodeID) {
                    return (int32)EdgeIndex;
                }
            }
        }
    }

    FDaxVisitor Parent{};
    Parent.TargetSet = TargetSet;
    Parent.TargetLiveToken = TargetLiveToken;
    Parent.NodePath = NodePath;
    Parent.NodePath.Pop();

    if (!Parent.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return -1;
    if (!Parent.CachedNode || !Parent.CachedNode->IsArray()) return -1;

    const auto* Arr = Parent.CachedNode->GetArray();
    if (!Arr) return -1;

    const int32 Index = *IndexPtr;
    if (!Arr->IsValidIndex(Index)) return -1;

    return Index;
}

FName FDaxVisitor::GetKeyInParentMap() const {
    // 尾段为 Key 才可能位于父映射中
    if (!IsValid()) return NAME_None;
    if (NodePath.IsEmpty()) return NAME_None;

    const TVariant<FName, int32>& Last = NodePath.Last();
    const FName* KeyPtr = Last.TryGet<FName>();
    if (!KeyPtr) return NAME_None;

    // 解析当前与父容器，确保结构有效
    if (!ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return NAME_None;

    // 快路径：使用反向映射
    if (TargetSet) {
        const auto Kind = TargetSet->Allocator.GetParentEdgeKind(CachedNodeID);
        if (Kind == ArzDax::EDaxParentEdgeKind::Map) {
            const FName Label = TargetSet->Allocator.GetParentEdgeLabel(CachedNodeID);
            FDaxVisitor Parent{};
            Parent.TargetSet = TargetSet;
            Parent.TargetLiveToken = TargetLiveToken;
            Parent.NodePath = NodePath;
            Parent.NodePath.Pop();
            if (Parent.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk() && Parent.CachedNode && Parent.CachedNode->IsMap()) {
                const auto* Map = Parent.CachedNode->GetMap();
                if (Map) {
                    auto It = Map->find(Label);
                    if (It != Map->end() && It->second == CachedNodeID) {
                        return Label;
                    }
                }
            }
        }
    }

    FDaxVisitor Parent{};
    Parent.TargetSet = TargetSet;
    Parent.TargetLiveToken = TargetLiveToken;
    Parent.NodePath = NodePath;
    Parent.NodePath.Pop();

    if (!Parent.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return NAME_None;
    if (!Parent.CachedNode || !Parent.CachedNode->IsMap()) return NAME_None;

    const auto* Map = Parent.CachedNode->GetMap();
    if (!Map) return NAME_None;

    const FName Key = *KeyPtr;
    if (Map->find(Key) == Map->end()) return NAME_None;

    return Key;
}

int32 FDaxVisitor::GetIndexUnderAncestorArray(const FDaxVisitor& Ancestor) const {
    // Ancestor 必须为当前访问器的严格祖先
    if (!IsValid() || !Ancestor.IsValid()) return -1;
    if (TargetLiveToken.Pin().Get() != Ancestor.TargetLiveToken.Pin().Get()) return -1;

    // IsAncestor 会内部做只读解析与路径前缀判断
    if (!Ancestor.IsAncestor(*this)) return -1;

    const int32 Start = Ancestor.NodePath.Num();
    if (Start >= NodePath.Num()) return -1;

    const TVariant<FName, int32>& Seg = NodePath[Start];
    const int32* IndexPtr = Seg.TryGet<int32>();
    if (!IndexPtr) return -1;

    // 可选：校验祖先确实为数组并且索引合法
    if (!Ancestor.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return -1;
    if (!Ancestor.CachedNode || !Ancestor.CachedNode->IsArray()) return -1;
    const auto* Arr = Ancestor.CachedNode->GetArray();
    if (!Arr) return -1;
    if (!Arr->IsValidIndex(*IndexPtr)) return -1;

    return *IndexPtr;
}

FName FDaxVisitor::GetKeyUnderAncestorMap(const FDaxVisitor& Ancestor) const {
    // Ancestor 必须为当前访问器的严格祖先
    if (!IsValid() || !Ancestor.IsValid()) return NAME_None;
    if (TargetLiveToken.Pin().Get() != Ancestor.TargetLiveToken.Pin().Get()) return NAME_None;

    if (!Ancestor.IsAncestor(*this)) return NAME_None;

    const int32 Start = Ancestor.NodePath.Num();
    if (Start >= NodePath.Num()) return NAME_None;

    const TVariant<FName, int32>& Seg = NodePath[Start];
    const FName* KeyPtr = Seg.TryGet<FName>();
    if (!KeyPtr) return NAME_None;

    // 可选：校验祖先确实为映射并且存在该键
    if (!Ancestor.ResolvePathInternal(EDaxPathResolveMode::ReadOnly).IsOk()) return NAME_None;
    if (!Ancestor.CachedNode || !Ancestor.CachedNode->IsMap()) return NAME_None;
    const auto* Map = Ancestor.CachedNode->GetMap();
    if (!Map) return NAME_None;

    const FName Key = *KeyPtr;
    if (Map->find(Key) == Map->end()) return NAME_None;

    return Key;
}

bool FDaxVisitor::operator==(const FDaxVisitor& Other) const {
    if (NodePath.Num() != Other.NodePath.Num()) return false;
    if (TargetLiveToken.Pin().Get() != Other.TargetLiveToken.Pin().Get()) return false;

    for (int32 i = 0; i < NodePath.Num(); ++i) {
        const auto& A = NodePath[i];
        const auto& B = Other.NodePath[i];
        if (A.GetIndex() != B.GetIndex()) return false;
        if (A.GetIndex() == 0) {
            if (A.Get<FName>() != B.Get<FName>()) return false;
        }
        else {
            if (A.Get<int32>() != B.Get<int32>()) return false;
        }
    }

    return true;
}

FDaxResultDetail FDaxVisitor::ResolvePathInternal(EDaxPathResolveMode Mode) const {
    if (!IsValid()) return FDaxResultDetail(EDaxResult::InvalidVisitor, TEXT("Visitor invalid or Set destroyed"));
    const bool IsOnServer = TargetSet->bRunningOnServer;

    // 辅助：格式化段/路径/模式/节点类型
    auto SegmentToString = [](const TVariant<FName, int32>& Seg) -> FString {
        if (const FName* K = Seg.TryGet<FName>()) return K->ToString();
        if (const int32* I = Seg.TryGet<int32>()) return FString::Printf(TEXT("[%d]"), *I);
        return TEXT("<unknown>");
    };

    auto BuildPathString = [this, &SegmentToString](int32 StopExclusive) -> FString {
        FString Out;
        Out.Reserve(64);
        Out += TEXT("/");
        const int32 N = FMath::Clamp(StopExclusive, 0, NodePath.Num());
        for (int32 i = 0; i < N; ++i) {
            if (i > 0) Out += TEXT("/");
            Out += SegmentToString(NodePath[i]);
        }
        return Out;
    };

    auto ModeToString = [](EDaxPathResolveMode M) -> const TCHAR* {
        switch (M) {
        case EDaxPathResolveMode::ReadOnly: return TEXT("ReadOnly");
        case EDaxPathResolveMode::EnsureCreate: return TEXT("EnsureCreate");
        case EDaxPathResolveMode::ForceOverride: return TEXT("ForceOverride");
        default: return TEXT("Unknown");
        }
    };

    auto TypeToString = [](ArzDax::FDaxNode* Node) -> FString {
        return Node ? Node->GetTypeName().ToString() : FString(TEXT("<null>"));
    };

    // 快速路径：结构版本一致，尝试直接验证缓存节点仍有效
    if (CachedSetStructVersion == TargetSet->StructVersion) {
        // 统一用 TryGetNode 校验节点仍存在（避免使用已释放的旧指针导致误判）
        ArzDax::FDaxNode* NodePtr = TargetSet->TryGetNode(CachedNodeID);
        if (NodePtr != nullptr) {
            CachedNode = NodePtr; // 同步更新缓存指针（服务端不走 Overlay 也安全）
            return FDaxResultDetail(EDaxResult::Success);
        }
        // 若节点已无效，则继续走完整解析流程
    }
    else {
        // 结构版本变化后，检测缓存的节点是否仍然有效
        // 记录旧ID以用于客户端Remove/重定向场景下的OldValue定位
        OldNodeID = CachedNodeID;
        CachedSetStructVersion = TargetSet->StructVersion;
    }
    if (!IsOnServer) Mode = EDaxPathResolveMode::ReadOnly; //如果是客户端, 不允许修改结构, 对于预测, 只能进行值修改, 而不能修改结构

    // 重解析：从 Root 开始
    FDaxNodeID CurrentID = TargetSet->RootID;
    if (!CurrentID.IsValid()) {
        if (Mode == EDaxPathResolveMode::ReadOnly) {
            return FDaxResultDetail(EDaxResult::InvalidRootNode,
                                    FString::Printf(
                                        TEXT("Root missing and readonly mode, path=%s, mode=%s"), *BuildPathString(0),
                                        ModeToString(Mode)));
        }
        // 确保创建 Root
        CurrentID = TargetSet->Allocator.Allocate();
        TargetSet->RootID = CurrentID;
        if (auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(CurrentID); InfoRef.IsValid() && InfoRef.pParent) {
            *InfoRef.pParent = FDaxNodeID();
        }
        TargetSet->BumpNodeDataVersionAndStruct(CurrentID);
    }

    ArzDax::FDaxNode* CurrentNode = TargetSet->TryGetNode(CurrentID);
    if (!CurrentNode) return FDaxResultDetail(EDaxResult::InvalidRootNode, TEXT("Root node not accessible"));

    // 空路径：指向根
    if (NodePath.IsEmpty()) {
        CachedNodeID = CurrentID;
        CachedNode = CurrentNode;
        CachedSetStructVersion = TargetSet->StructVersion;
        return FDaxResultDetail(EDaxResult::Success);
    }

    // 防御性：路径深度限制
    static constexpr int32 MaxPathDepth = 64;
    if (NodePath.Num() > MaxPathDepth) {
        return FDaxResultDetail(EDaxResult::ResolvePathTooDeep,
                                FString::Printf(
                                    TEXT("Path depth exceeds MaxPathDepth(%d), fullPath=%s"), MaxPathDepth,
                                    *BuildPathString(NodePath.Num())));
    }

    for (int32 SegIndex = 0; SegIndex < NodePath.Num(); ++SegIndex) {
        const auto& Segment = NodePath[SegIndex];

        if (const FName* Key = Segment.TryGet<FName>()) {
            // 期望 Map
            if (!CurrentNode->IsMap()) {
                if (Mode == EDaxPathResolveMode::ReadOnly) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Node is not a map (readonly). nodeType=%s, at=%s, next=%s, mode=%s"),
                        *TypeToString(CurrentNode), *ResolvedPath, *NextSegStr, ModeToString(Mode));
                    return FDaxResultDetail(EDaxResult::SegmentNameButNodeNotMap, Msg);
                }
                // Ensure/Force：空或强制覆盖为 Map（释放旧子树，保持拓扑一致）
                if (CurrentNode->IsEmpty() || Mode == EDaxPathResolveMode::ForceOverride) {
                    auto R = TargetSet->ResetToEmptyMap(CurrentID);
                    if (R == EDaxResult::InvalidNode) {
                        const FString ResolvedPath = BuildPathString(SegIndex);
                        const FString NextSegStr = SegmentToString(Segment);
                        const FString Msg = FString::Printf(
                            TEXT("Failed resetting to empty map, node invalid. at=%s, next=%s"), *ResolvedPath,
                            *NextSegStr);
                        return FDaxResultDetail(EDaxResult::ResolveOperatorFailure, Msg);
                    }
                    // Node 实例可能改变内部存储，重新获取
                    CurrentNode = TargetSet->TryGetNode(CurrentID);
                    if (!CurrentNode) {
                        const FString ResolvedPath = BuildPathString(SegIndex);
                        const FString NextSegStr = SegmentToString(Segment);
                        const FString Msg = FString::Printf(
                            TEXT("Node missing after map reset. at=%s, next=%s"), *ResolvedPath, *NextSegStr);
                        return FDaxResultDetail(EDaxResult::UnknownFailure, Msg);
                    }
                }
                else {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Node is not a map (ensure). nodeType=%s, at=%s, next=%s, mode=%s"),
                        *TypeToString(CurrentNode), *ResolvedPath, *NextSegStr, ModeToString(Mode));
                    return FDaxResultDetail(EDaxResult::SegmentNameButNodeNotMap, Msg);
                }
            }

            auto* Map = CurrentNode->GetMap();
            if (!Map) {
                const FString ResolvedPath = BuildPathString(SegIndex);
                const FString NextSegStr = SegmentToString(Segment);
                const FString Msg = FString::Printf(
                    TEXT("Map pointer is null. at=%s, next=%s"), *ResolvedPath, *NextSegStr);
                return FDaxResultDetail(EDaxResult::ResolveInternalNullMap, Msg);
            }

            auto It = Map->find(*Key);
            bool bFound = (It != Map->end()) && TargetSet->IsNodeValid(It->second);
            if (!bFound) {
                if (Mode == EDaxPathResolveMode::ReadOnly) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Key not found (readonly). key=%s, at=%s, mode=%s"), *Key->ToString(), *ResolvedPath,
                        ModeToString(Mode));
                    return FDaxResultDetail(EDaxResult::ResolveMapKeyNotFound, Msg);
                }
                // 创建子节点并链接
                FDaxNodeID ChildID = TargetSet->Allocator.Allocate();
                auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(ChildID);
                if (!InfoRef.IsValid()) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Allocate child failed. key=%s, at=%s"), *Key->ToString(), *ResolvedPath);
                    return FDaxResultDetail(EDaxResult::ResolveAllocateFailed, Msg);
                }
                if (InfoRef.pParent) *InfoRef.pParent = CurrentID;

                (*Map)[*Key] = ChildID;

                TargetSet->BumpNodeDataVersionAndStruct(CurrentID);
                TargetSet->BumpNodeDataVersionAndStruct(ChildID);

                CurrentID = ChildID;
                CurrentNode = TargetSet->TryGetNode(CurrentID);
                if (!CurrentNode) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Child node missing after allocation. key=%s, at=%s"), *Key->ToString(), *ResolvedPath);
                    return FDaxResultDetail(EDaxResult::UnknownFailure, Msg);
                }
            }
            else {
                CurrentID = It->second;
                CurrentNode = TargetSet->TryGetNode(CurrentID);
                if (!CurrentNode) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Child node invalid. key=%s, at=%s"), *Key->ToString(), *ResolvedPath);
                    return FDaxResultDetail(EDaxResult::InvalidNode, Msg);
                }
            }
        }
        else if (const int32* IdxPtr = Segment.TryGet<int32>()) {
            const int32 Index = *IdxPtr;
            if (Index < 0) {
                const FString ResolvedPath = BuildPathString(SegIndex);
                const FString NextSegStr = SegmentToString(Segment);
                const FString Msg = FString::Printf(
                    TEXT("Negative index. index=%d, at=%s, mode=%s"), Index, *ResolvedPath, ModeToString(Mode));
                return FDaxResultDetail(EDaxResult::ResolveArrayIndexNegative, Msg);
            }

            // 期望 Array
            if (!CurrentNode->IsArray()) {
                if (Mode == EDaxPathResolveMode::ReadOnly) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Node is not an array (readonly). nodeType=%s, at=%s, next=%s, mode=%s"),
                        *TypeToString(CurrentNode), *ResolvedPath, *NextSegStr, ModeToString(Mode));
                    return FDaxResultDetail(EDaxResult::SegmentIndexButNodeNotArray, Msg);
                }
                if (CurrentNode->IsEmpty() || Mode == EDaxPathResolveMode::ForceOverride) {
                    auto R = TargetSet->ResetToEmptyArray(CurrentID);
                    if (R == EDaxResult::InvalidNode) {
                        const FString ResolvedPath = BuildPathString(SegIndex);
                        const FString NextSegStr = SegmentToString(Segment);
                        const FString Msg = FString::Printf(
                            TEXT("Failed resetting to empty array, node invalid. at=%s, next=%s"), *ResolvedPath,
                            *NextSegStr);
                        return FDaxResultDetail(EDaxResult::ResolveOperatorFailure, Msg);
                    }
                    CurrentNode = TargetSet->TryGetNode(CurrentID);
                    if (!CurrentNode) {
                        const FString ResolvedPath = BuildPathString(SegIndex);
                        const FString NextSegStr = SegmentToString(Segment);
                        const FString Msg = FString::Printf(
                            TEXT("Node missing after array reset. at=%s, next=%s"), *ResolvedPath, *NextSegStr);
                        return FDaxResultDetail(EDaxResult::UnknownFailure, Msg);
                    }
                }
                else {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Node is not an array (ensure). nodeType=%s, at=%s, next=%s, mode=%s"),
                        *TypeToString(CurrentNode), *ResolvedPath, *NextSegStr, ModeToString(Mode));
                    return FDaxResultDetail(EDaxResult::SegmentIndexButNodeNotArray, Msg);
                }
            }

            auto* Arr = CurrentNode->GetArray();
            if (!Arr) {
                const FString ResolvedPath = BuildPathString(SegIndex);
                const FString NextSegStr = SegmentToString(Segment);
                const FString Msg = FString::Printf(
                    TEXT("Array pointer is null. at=%s, next=%s"), *ResolvedPath, *NextSegStr);
                return FDaxResultDetail(EDaxResult::ResolveInternalNullArray, Msg);
            }

            // 不允许越界与稀疏创建
            if (!Arr->IsValidIndex(Index)) {
                const FString ResolvedPath = BuildPathString(SegIndex);
                const FString NextSegStr = SegmentToString(Segment);
                const FString Msg = FString::Printf(
                    TEXT("Index out of range. index=%d, size=%d, at=%s"), Index, Arr->Num(), *ResolvedPath);
                return FDaxResultDetail(EDaxResult::ResolveArrayIndexOutOfRange, Msg);
            }

            FDaxNodeID& Slot = (*Arr)[Index];
            if (!Slot.IsValid() || !TargetSet->IsNodeValid(Slot)) {
                if (Mode == EDaxPathResolveMode::ReadOnly) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Empty slot (readonly). index=%d, at=%s, mode=%s"), Index, *ResolvedPath,
                        ModeToString(Mode));
                    return FDaxResultDetail(EDaxResult::ResolveArrayIndexOutOfRange, Msg);
                }
                FDaxNodeID ChildID = TargetSet->Allocator.Allocate();
                Slot = ChildID;
                auto InfoRef = TargetSet->Allocator.GetCommonInfoRef(ChildID);
                if (!InfoRef.IsValid()) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Allocate child failed. index=%d, at=%s"), Index, *ResolvedPath);
                    return FDaxResultDetail(EDaxResult::ResolveAllocateFailed, Msg);
                }
                if (InfoRef.pParent) *InfoRef.pParent = CurrentID;

                TargetSet->BumpNodeDataVersionAndStruct(CurrentID);
                TargetSet->BumpNodeDataVersionAndStruct(ChildID);

                CurrentID = ChildID;
                CurrentNode = TargetSet->TryGetNode(CurrentID);
                if (!CurrentNode) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Child node missing after allocation. index=%d, at=%s"), Index, *ResolvedPath);
                    return FDaxResultDetail(EDaxResult::UnknownFailure, Msg);
                }
            }
            else {
                CurrentID = Slot;
                CurrentNode = TargetSet->TryGetNode(CurrentID);
                if (!CurrentNode) {
                    const FString ResolvedPath = BuildPathString(SegIndex);
                    const FString NextSegStr = SegmentToString(Segment);
                    const FString Msg = FString::Printf(
                        TEXT("Child node invalid. index=%d, at=%s"), Index, *ResolvedPath);
                    return FDaxResultDetail(EDaxResult::InvalidNode, Msg);
                }
            }
        }
        else {
            const FString ResolvedPath = BuildPathString(SegIndex);
            const FString NextSegStr = SegmentToString(Segment);
            const FString Msg = FString::Printf(TEXT("Unknown path segment variant. at=%s"), *ResolvedPath);
            return FDaxResultDetail(EDaxResult::InvalidVisitor, Msg);
        }
    }

    // 解析成功：更新缓存
    CachedNodeID = CurrentID;

    if (IsOnServer) {
        CachedNode = CurrentNode;
    }
    else {
        const auto TempCachedNode = TargetSet->TryGetNode(CachedNodeID);
        CachedNode = TempCachedNode != nullptr ? TempCachedNode : CurrentNode;
    }

    CachedSetStructVersion = TargetSet->StructVersion;
    return FDaxResultDetail(EDaxResult::Success);
}

bool FDaxVisitor::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) {
    bOutSuccess = false;

    uint8 bHasData = 0;

    if (Ar.IsSaving()) {
        bool bValid = IsValid() && TargetSet->ParentComponent.IsValid();

        bHasData = bValid ? 1 : 0;
        Ar << bHasData;

        if (!bValid) {
            bOutSuccess = true;
            return true;
        }

        UObject* Ptr = TargetSet->ParentComponent.Get();
        if (!Map->SerializeObject(Ar, UDaxComponent::StaticClass(), Ptr)) {
            UE_LOGFMT(DataXSystem, Warning, "FDaxVisitor::NetSerialize(send): SerializeObject failed");
            bOutSuccess = false;
            return true;
        }

        uint32 Count = static_cast<uint32>(NodePath.Num());
        Ar.SerializeIntPacked(Count);

        for (const TVariant<FName, int32>& V : NodePath) {
            const uint8 Tag = V.IsType<FName>() ? 0 : 1;
            Ar << const_cast<uint8&>(Tag);
            if (Tag == 0) {
                FName Name = V.Get<FName>();
                Ar << Name;
            }
            else {
                int32 I = V.Get<int32>();
                Ar << I;
            }
        }
        
        bOutSuccess = true;
        return true;
    }
    else {
        ResetAll();

        Ar << bHasData;

        if (!bHasData) {
            bOutSuccess = true;
            return true;
        }

        UObject* Ptr = nullptr;
        if (!Map->SerializeObject(Ar, UDaxComponent::StaticClass(), Ptr)) {
            UE_LOGFMT(DataXSystem, Warning, "FDaxVisitor::NetSerialize(recv): SerializeObject failed");
            bOutSuccess = false;
            return true;
        }
        
        UDaxComponent* Comp = Cast<UDaxComponent>(Ptr);
        if (!Comp) {
            UE_LOGFMT(DataXSystem, Warning, "FDaxVisitor::NetSerialize(recv): Invalid component pointer");
            bOutSuccess = false;
            return true;
        }
        
        TargetSet = &Comp->DataSet;
        TargetLiveToken = Comp->DataSet.LiveToken.ToWeakPtr();

        uint32 Count = 0;
        Ar.SerializeIntPacked(Count);

        constexpr uint32 kMaxPathEntries = 2048u;
        if (Count > kMaxPathEntries) {
            UE_LOG(DataXSystem, Warning, TEXT("FDaxVisitor::NetSerialize(recv): Path count %u exceeds limit %u"), Count,
                   kMaxPathEntries);
            return true;
        }

        NodePath.Empty(Count);

        for (uint32 i = 0; i < Count; ++i) {
            uint8 Tag = 0;
            Ar << Tag;

            if (Tag == 0) {
                FName N;
                Ar << N;
                NodePath.Emplace(TVariant<FName, int32>(TInPlaceType<FName>(), N));
            }
            else if (Tag == 1) {
                int32 I = 0;
                Ar << I;
                NodePath.Emplace(TVariant<FName, int32>(TInPlaceType<int32>(), I));
            }
            else {
                UE_LOG(DataXSystem, Warning, TEXT("FDaxVisitor::NetSerialize(recv): Invalid tag %u at index %u"), Tag,
                       i);
                return true;
            }
        }

        bOutSuccess = true;
        return true;
    }
}

FString FDaxVisitor::GetString() const {
    auto BuildPath = [this]() -> FString {
        FString Out = TEXT("/");
        for (int32 i = 0; i < NodePath.Num(); ++i) {
            if (i > 0) Out += TEXT("/");
            const auto& S = NodePath[i];
            if (const FName* K = S.TryGet<FName>()) Out += K->ToString();
            else if (const int32* I = S.TryGet<int32>()) Out += FString::Printf(TEXT("[%d]"), *I);
        }
        return Out;
    };

    const auto Res = ResolvePathInternal(EDaxPathResolveMode::ReadOnly);
    if (!Res.IsOk()) {
        return FString::Printf(TEXT("<ResolveFailed> %s : %s"), *Res.GetResultString(), *Res.ResultMessage);
    }

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

    auto TypeNameOf = [&](const FDaxNodeID ID) -> FString {
        const UScriptStruct* Type = TargetSet->GetNodeValueType(ID);
        if (Type == FDaxFakeTypeArray::StaticStruct()) return TEXT("Array");
        if (Type == FDaxFakeTypeMap::StaticStruct()) return TEXT("Map");
        if (Type == FDaxFakeTypeEmpty::StaticStruct()) return TEXT("Empty");
        if (Type == nullptr) return TEXT("NULL");
        return Type->GetName();
    };

    FString Out;
    Out += FString::Printf(TEXT("Visitor Path=%s\n"), *BuildPath());

    const FString Ind0 = IndentOf(0);
    const FString RootLabel = TEXT("Self");

    const FDaxNodeID SelfID = CachedNodeID;
    const ArzDax::FDaxNode* Self = CachedNode;
    const FString TypeName = TypeNameOf(SelfID);

    if (!Self) {
        Out += FString::Printf(TEXT("%s- %s %s <no-node>\n"), *Ind0, *RootLabel, *TypeName);
        return Out;
    }

    if (TargetSet->IsNodeArray(SelfID)) {
        const auto* Arr = Self->GetArray();
        const int32 Count = Arr ? Arr->Num() : 0;
        Out += FString::Printf(TEXT("%s- %s Array(count=%d)\n"), *Ind0, *RootLabel, Count);
        if (Arr) {
            for (int32 i = 0; i < Count; ++i) {
                const FDaxNodeID C = (*Arr)[i];
                const ArzDax::FDaxNode* N = TargetSet->TryGetNode(C);
                const FString TN = TypeNameOf(C);
                if (!N) {
                    Out += FString::Printf(TEXT("%s- [%d] %s <no-node>\n"), *IndentOf(1), i, *TN);
                }
                else if (TargetSet->IsNodeArray(C)) {
                    const auto* A2 = N->GetArray();
                    const int32 N2 = A2 ? A2->Num() : 0;
                    Out += FString::Printf(TEXT("%s- [%d] Array(count=%d)\n"), *IndentOf(1), i, N2);
                }
                else if (TargetSet->IsNodeMap(C)) {
                    const auto* M2 = N->GetMap();
                    const int32 N2 = M2 ? static_cast<int32>(M2->size()) : 0;
                    Out += FString::Printf(TEXT("%s- [%d] Map(count=%d)\n"), *IndentOf(1), i, N2);
                }
                else if (TargetSet->IsNodeEmpty(C) || !TargetSet->GetNodeValueType(C)) {
                    Out += FString::Printf(TEXT("%s- [%d] Empty\n"), *IndentOf(1), i);
                }
                else {
                    const FConstStructView SV = N->TryGetValueGeneric();
                    Out += FString::Printf(TEXT("%s- [%d] %s\t=%s\n"), *IndentOf(1), i, *TN, *ValueToText(SV));
                }
            }
        }
    }
    else if (TargetSet->IsNodeMap(SelfID)) {
        const auto* Map = Self->GetMap();
        const int32 Count = Map ? static_cast<int32>(Map->size()) : 0;
        Out += FString::Printf(TEXT("%s- %s Map(count=%d)\n"), *Ind0, *RootLabel, Count);
        if (Map) {
            for (const auto& KV : *Map) {
                const FName K = KV.first;
                const FDaxNodeID C = KV.second;
                const ArzDax::FDaxNode* N = TargetSet->TryGetNode(C);
                const FString TN = TypeNameOf(C);
                const FString Label = K.ToString();
                if (!N) {
                    Out += FString::Printf(TEXT("%s- %s %s <no-node>\n"), *IndentOf(1), *Label, *TN);
                }
                else if (TargetSet->IsNodeArray(C)) {
                    const auto* A2 = N->GetArray();
                    const int32 N2 = A2 ? A2->Num() : 0;
                    Out += FString::Printf(TEXT("%s- %s Array(count=%d)\n"), *IndentOf(1), *Label, N2);
                }
                else if (TargetSet->IsNodeMap(C)) {
                    const auto* M2 = N->GetMap();
                    const int32 N2 = M2 ? static_cast<int32>(M2->size()) : 0;
                    Out += FString::Printf(TEXT("%s- %s Map(count=%d)\n"), *IndentOf(1), *Label, N2);
                }
                else if (TargetSet->IsNodeEmpty(C) || !TargetSet->GetNodeValueType(C)) {
                    Out += FString::Printf(TEXT("%s- %s Empty\n"), *IndentOf(1), *Label);
                }
                else {
                    const FConstStructView SV = N->TryGetValueGeneric();
                    Out += FString::Printf(TEXT("%s- %s %s\t=%s\n"), *IndentOf(1), *Label, *TN, *ValueToText(SV));
                }
            }
        }
    }
    else if (TargetSet->IsNodeEmpty(SelfID) || !TargetSet->GetNodeValueType(SelfID)) {
        Out += FString::Printf(TEXT("%s- %s Empty\n"), *Ind0, *RootLabel);
    }
    else {
        const FConstStructView SV = Self->TryGetValueGeneric();
        Out += FString::Printf(TEXT("%s- %s %s\t=%s\n"), *Ind0, *RootLabel, *TypeName, *ValueToText(SV));
    }

    return Out;
}

FString FDaxVisitor::GetStringDebug() const {
    auto BuildPath = [this]() -> FString {
        FString Out = TEXT("/");
        for (int32 i = 0; i < NodePath.Num(); ++i) {
            if (i > 0) Out += TEXT("/");
            const auto& S = NodePath[i];
            if (const FName* K = S.TryGet<FName>()) Out += K->ToString();
            else if (const int32* I = S.TryGet<int32>()) Out += FString::Printf(TEXT("[%d]"), *I);
        }
        return Out;
    };

    const auto Res = ResolvePathInternal(EDaxPathResolveMode::ReadOnly);
    if (!Res.IsOk()) {
        return FString::Printf(TEXT("<ResolveFailed> %s : %s"), *Res.GetResultString(), *Res.ResultMessage);
    }

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

    auto TypeNameOf = [&](const FDaxNodeID ID) -> FString {
        const UScriptStruct* Type = TargetSet->GetNodeValueType(ID);
        if (Type == FDaxFakeTypeArray::StaticStruct()) return TEXT("Array");
        if (Type == FDaxFakeTypeMap::StaticStruct()) return TEXT("Map");
        if (Type == FDaxFakeTypeEmpty::StaticStruct()) return TEXT("Empty");
        if (Type == nullptr) return TEXT("NULL");
        return Type->GetName();
    };

    FString Out;
    Out += FString::Printf(TEXT("Visitor Path=%s\n"), *BuildPath());

    const FString Ind0 = IndentOf(0);
    const FString RootLabel = TEXT("Self");

    const FDaxNodeID SelfID = CachedNodeID;
    const ArzDax::FDaxNode* Self = CachedNode;
    const FString TypeName = TypeNameOf(SelfID);
    const auto CommonSelf = TargetSet->Allocator.GetCommonInfo(SelfID);

    if (!Self) {
        Out += FString::Printf(
            TEXT("%s- %s %s [ID=%s Ver=%u] <no-node>\n"), *Ind0, *RootLabel, *TypeName, *SelfID.ToString(),
            CommonSelf.Version);
        return Out;
    }

    if (TargetSet->IsNodeArray(SelfID)) {
        const auto* Arr = Self->GetArray();
        const int32 Count = Arr ? Arr->Num() : 0;
        Out += FString::Printf(
            TEXT("%s- %s Array(count=%d) [ID=%s Ver=%u]\n"), *Ind0, *RootLabel, Count, *SelfID.ToString(),
            CommonSelf.Version);
        if (Arr) {
            for (int32 i = 0; i < Count; ++i) {
                const FDaxNodeID C = (*Arr)[i];
                const ArzDax::FDaxNode* N = TargetSet->TryGetNode(C);
                const FString TN = TypeNameOf(C);
                const auto Common = TargetSet->Allocator.GetCommonInfo(C);
                if (!N) {
                    Out += FString::Printf(
                        TEXT("%s- [%d] %s [ID=%s Ver=%u] <no-node>\n"), *IndentOf(1), i, *TN, *C.ToString(),
                        Common.Version);
                }
                else if (TargetSet->IsNodeArray(C)) {
                    const auto* A2 = N->GetArray();
                    const int32 N2 = A2 ? A2->Num() : 0;
                    Out += FString::Printf(
                        TEXT("%s- [%d] Array(count=%d) [ID=%s Ver=%u]\n"), *IndentOf(1), i, N2, *C.ToString(),
                        Common.Version);
                }
                else if (TargetSet->IsNodeMap(C)) {
                    const auto* M2 = N->GetMap();
                    const int32 N2 = M2 ? static_cast<int32>(M2->size()) : 0;
                    Out += FString::Printf(
                        TEXT("%s- [%d] Map(count=%d) [ID=%s Ver=%u]\n"), *IndentOf(1), i, N2, *C.ToString(),
                        Common.Version);
                }
                else if (TargetSet->IsNodeEmpty(C) || !TargetSet->GetNodeValueType(C)) {
                    Out += FString::Printf(
                        TEXT("%s- [%d] Empty [ID=%s Ver=%u]\n"), *IndentOf(1), i, *C.ToString(), Common.Version);
                }
                else {
                    const FConstStructView SV = N->TryGetValueGeneric();
                    Out += FString::Printf(
                        TEXT("%s- [%d] %s\t=%s [ID=%s Ver=%u]\n"), *IndentOf(1), i, *TN, *ValueToText(SV),
                        *C.ToString(), Common.Version);
                }
            }
        }
    }
    else if (TargetSet->IsNodeMap(SelfID)) {
        const auto* Map = Self->GetMap();
        const int32 Count = Map ? static_cast<int32>(Map->size()) : 0;
        Out += FString::Printf(
            TEXT("%s- %s Map(count=%d) [ID=%s Ver=%u]\n"), *Ind0, *RootLabel, Count, *SelfID.ToString(),
            CommonSelf.Version);
        if (Map) {
            for (const auto& KV : *Map) {
                const FName K = KV.first;
                const FDaxNodeID C = KV.second;
                const ArzDax::FDaxNode* N = TargetSet->TryGetNode(C);
                const FString TN = TypeNameOf(C);
                const auto Common = TargetSet->Allocator.GetCommonInfo(C);
                const FString Label = K.ToString();
                if (!N) {
                    Out += FString::Printf(
                        TEXT("%s- %s %s [ID=%s Ver=%u] <no-node>\n"), *IndentOf(1), *Label, *TN, *C.ToString(),
                        Common.Version);
                }
                else if (TargetSet->IsNodeArray(C)) {
                    const auto* A2 = N->GetArray();
                    const int32 N2 = A2 ? A2->Num() : 0;
                    Out += FString::Printf(
                        TEXT("%s- %s Array(count=%d) [ID=%s Ver=%u]\n"), *IndentOf(1), *Label, N2, *C.ToString(),
                        Common.Version);
                }
                else if (TargetSet->IsNodeMap(C)) {
                    const auto* M2 = N->GetMap();
                    const int32 N2 = M2 ? static_cast<int32>(M2->size()) : 0;
                    Out += FString::Printf(
                        TEXT("%s- %s Map(count=%d) [ID=%s Ver=%u]\n"), *IndentOf(1), *Label, N2, *C.ToString(),
                        Common.Version);
                }
                else if (TargetSet->IsNodeEmpty(C) || !TargetSet->GetNodeValueType(C)) {
                    Out += FString::Printf(
                        TEXT("%s- %s Empty [ID=%s Ver=%u]\n"), *IndentOf(1), *Label, *C.ToString(), Common.Version);
                }
                else {
                    const FConstStructView SV = N->TryGetValueGeneric();
                    Out += FString::Printf(
                        TEXT("%s- %s %s\t=%s [ID=%s Ver=%u]\n"), *IndentOf(1), *Label, *TN, *ValueToText(SV),
                        *C.ToString(), Common.Version);
                }
            }
        }
    }
    else if (TargetSet->IsNodeEmpty(SelfID) || !TargetSet->GetNodeValueType(SelfID)) {
        Out += FString::Printf(TEXT("%s- %s Empty [ID=%s Ver=%u]\n"), *Ind0, *RootLabel, *SelfID.ToString(),
                               CommonSelf.Version);
    }
    else {
        const FConstStructView SV = Self->TryGetValueGeneric();
        Out += FString::Printf(TEXT("%s- %s %s\t=%s [ID=%s Ver=%u]\n"), *Ind0, *RootLabel, *TypeName, *ValueToText(SV),
                               *SelfID.ToString(), CommonSelf.Version);
    }

    return Out;
}

FString FDaxVisitor::GetPathString() const {
    if (!IsValid()) return "$Invalid Dax Visitor$";
    FString Path = "Root";
    for (const TVariant<FName, int32>& Seg : NodePath) {
        if (const FName* K = Seg.TryGet<FName>()) {
            Path += TEXT("/");
            Path += K->ToString();
        }
        else if (const int32* I = Seg.TryGet<int32>()) {
            Path += FString::Printf(TEXT("[%d]"), *I);
        }
        else {
            Path += TEXT("/$InvalidSegment$");
        }
    }

    return Path;
}

FString FDaxVisitor::GetStringDeep() const {
    auto BuildPath = [this]() -> FString {
        FString Out = TEXT("/");
        for (int32 i = 0; i < NodePath.Num(); ++i) {
            if (i > 0) Out += TEXT("/");
            const auto& S = NodePath[i];
            if (const FName* K = S.TryGet<FName>()) Out += K->ToString();
            else if (const int32* I = S.TryGet<int32>()) Out += FString::Printf(TEXT("[%d]"), *I);
        }
        return Out;
    };

    const auto Res = ResolvePathInternal(EDaxPathResolveMode::ReadOnly);
    if (!Res.IsOk()) {
        return FString::Printf(TEXT("<ResolveFailed> %s : %s"), *Res.GetResultString(), *Res.ResultMessage);
    }

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

    auto TypeNameOf = [&](const FDaxNodeID ID) -> FString {
        const UScriptStruct* Type = TargetSet->GetNodeValueType(ID);
        if (Type == FDaxFakeTypeArray::StaticStruct()) return TEXT("Array");
        if (Type == FDaxFakeTypeMap::StaticStruct()) return TEXT("Map");
        if (Type == FDaxFakeTypeEmpty::StaticStruct()) return TEXT("Empty");
        if (Type == nullptr) return TEXT("NULL");
        return Type->GetName();
    };

    FString Out;
    Out += FString::Printf(TEXT("Visitor Path=%s\n"), *BuildPath());

    const FDaxNodeID RootID = CachedNodeID;

    TFunction<void(const FDaxNodeID, const FString&, int32)> Dump = [&
        ](const FDaxNodeID ID, const FString& Label, int32 Depth) {
        const FString Ind = IndentOf(Depth);
        const FString TN = TypeNameOf(ID);
        const ArzDax::FDaxNode* Node = TargetSet->TryGetNode(ID);
        if (!Node) {
            Out += FString::Printf(TEXT("%s- %s %s <no-node>\n"), *Ind, *Label, *TN);
            return;
        }

        if (TargetSet->IsNodeArray(ID)) {
            const auto* Arr = Node->GetArray();
            const int32 Count = Arr ? Arr->Num() : 0;
            Out += FString::Printf(TEXT("%s- %s Array(count=%d)\n"), *Ind, *Label, Count);
            if (Arr) {
                for (int32 i = 0; i < Count; ++i) {
                    const FDaxNodeID C = (*Arr)[i];
                    Dump(C, FString::Printf(TEXT("[%d]"), i), Depth + 1);
                }
            }
        }
        else if (TargetSet->IsNodeMap(ID)) {
            const auto* Map = Node->GetMap();
            const int32 Count = Map ? static_cast<int32>(Map->size()) : 0;
            Out += FString::Printf(TEXT("%s- %s Map(count=%d)\n"), *Ind, *Label, Count);
            if (Map) {
                for (const auto& KV : *Map) {
                    const FName K = KV.first;
                    const FDaxNodeID C = KV.second;
                    Dump(C, K.ToString(), Depth + 1);
                }
            }
        }
        else if (TargetSet->IsNodeEmpty(ID) || !TargetSet->GetNodeValueType(ID)) {
            Out += FString::Printf(TEXT("%s- %s Empty\n"), *Ind, *Label);
        }
        else {
            const FConstStructView SV = Node->TryGetValueGeneric();
            Out += FString::Printf(TEXT("%s- %s %s\t=%s\n"), *Ind, *Label, *TN, *ValueToText(SV));
        }
    };

    Dump(RootID, TEXT("Self"), 0);
    return Out;
}

FString FDaxVisitor::GetStringDebugDeep() const {
    auto BuildPath = [this]() -> FString {
        FString Out = TEXT("/");
        for (int32 i = 0; i < NodePath.Num(); ++i) {
            if (i > 0) Out += TEXT("/");
            const auto& S = NodePath[i];
            if (const FName* K = S.TryGet<FName>()) Out += K->ToString();
            else if (const int32* I = S.TryGet<int32>()) Out += FString::Printf(TEXT("[%d]"), *I);
        }
        return Out;
    };

    const auto Res = ResolvePathInternal(EDaxPathResolveMode::ReadOnly);
    if (!Res.IsOk()) {
        return FString::Printf(TEXT("<ResolveFailed> %s : %s"), *Res.GetResultString(), *Res.ResultMessage);
    }

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

    auto TypeNameOf = [&](const FDaxNodeID ID) -> FString {
        const UScriptStruct* Type = TargetSet->GetNodeValueType(ID);
        if (Type == FDaxFakeTypeArray::StaticStruct()) return TEXT("Array");
        if (Type == FDaxFakeTypeMap::StaticStruct()) return TEXT("Map");
        if (Type == FDaxFakeTypeEmpty::StaticStruct()) return TEXT("Empty");
        if (Type == nullptr) return TEXT("NULL");
        return Type->GetName();
    };

    FString Out;
    Out += FString::Printf(TEXT("Visitor Path=%s\n"), *BuildPath());

    const FDaxNodeID RootID = CachedNodeID;

    TFunction<void(const FDaxNodeID, const FString&, int32)> Dump = [&
        ](const FDaxNodeID ID, const FString& Label, int32 Depth) {
        const FString Ind = IndentOf(Depth);
        const FString TN = TypeNameOf(ID);
        const auto Common = TargetSet->Allocator.GetCommonInfo(ID);
        const ArzDax::FDaxNode* Node = TargetSet->TryGetNode(ID);
        if (!Node) {
            Out += FString::Printf(
                TEXT("%s- %s %s [ID=%s Ver=%u] <no-node>\n"), *Ind, *Label, *TN, *ID.ToString(), Common.Version);
            return;
        }

        if (TargetSet->IsNodeArray(ID)) {
            const auto* Arr = Node->GetArray();
            const int32 Count = Arr ? Arr->Num() : 0;
            Out += FString::Printf(
                TEXT("%s- %s Array(count=%d) [ID=%s Ver=%u]\n"), *Ind, *Label, Count, *ID.ToString(), Common.Version);
            if (Arr) {
                for (int32 i = 0; i < Count; ++i) {
                    const FDaxNodeID C = (*Arr)[i];
                    Dump(C, FString::Printf(TEXT("[%d]"), i), Depth + 1);
                }
            }
        }
        else if (TargetSet->IsNodeMap(ID)) {
            const auto* Map = Node->GetMap();
            const int32 Count = Map ? static_cast<int32>(Map->size()) : 0;
            Out += FString::Printf(
                TEXT("%s- %s Map(count=%d) [ID=%s Ver=%u]\n"), *Ind, *Label, Count, *ID.ToString(), Common.Version);
            if (Map) {
                for (const auto& KV : *Map) {
                    const FName K = KV.first;
                    const FDaxNodeID C = KV.second;
                    Dump(C, K.ToString(), Depth + 1);
                }
            }
        }
        else if (TargetSet->IsNodeEmpty(ID) || !TargetSet->GetNodeValueType(ID)) {
            Out += FString::Printf(TEXT("%s- %s Empty [ID=%s Ver=%u]\n"), *Ind, *Label, *ID.ToString(), Common.Version);
        }
        else {
            const FConstStructView SV = Node->TryGetValueGeneric();
            Out += FString::Printf(
                TEXT("%s- %s %s\t=%s [ID=%s Ver=%u]\n"), *Ind, *Label, *TN, *ValueToText(SV), *ID.ToString(),
                Common.Version);
        }
    };

    Dump(RootID, TEXT("Self"), 0);
    return Out;
}
