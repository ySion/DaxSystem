#pragma once

#include "CoreMinimal.h"
#include "DaxSystem/Private/DaxAllocator.h"
#include "DaxSystem/Public/DaxVisitor.h"
#include "DaxSet.generated.h"

DECLARE_STATS_GROUP(TEXT("DaxSystem"), STATGROUP_DaxSystem, STATCAT_DaxSystem)
DECLARE_CYCLE_STAT(TEXT("DaxSystem NetSyncTick"), STAT_NetSyncTick, STATGROUP_DaxSystem)
DECLARE_CYCLE_STAT(TEXT("DaxSystem NetDeltaSync"), STAT_NetDeltaSync, STATGROUP_DaxSystem)


class UDaxComponent;
class FDaxSetBaseState;

DECLARE_DYNAMIC_DELEGATE_OneParam(FDaxOnChangedDynamic, const FDaxVisitor&, ChangePosition);

struct FDaxOnChangedBinding {

    FDaxVisitor ListenPath {};
    
    bool bAggregate = false;
    
    int32 Depth = 0;
    
    FDaxOnChangedDynamic Delegate {}; // 动态委托（蓝图/反射安全）

    bool IsValid() const { return Delegate.IsBound(); }

    // Cache
    FDaxNodeID AnchorID{};
    
    uint32 AnchorStructVersion = 0;
};

USTRUCT(BlueprintType)
struct DAXSYSTEM_API FDaxSet {
    GENERATED_BODY()

    FDaxSet();
    ~FDaxSet() = default;
    FDaxSet(const FDaxSet& Other);
    FDaxSet& operator=(const FDaxSet& Other);
    FDaxSet(FDaxSet&& Other) = delete;
    FDaxSet& operator=(FDaxSet&& Other) = delete;

    FDaxVisitor GetVisitor() const { return FDaxVisitor(const_cast<FDaxSet*>(this), LiveToken); }
    FDaxVisitor GetVisitorFromPath(const FString& Path) const { return GetVisitor().MakeVisitorByParsePath(Path); }

    FString GetString() const;
    FString GetStringDebug() const;

    FORCEINLINE uint32 GetNodeNum() const { return Allocator.Stats.CurrentActive; }

private:
    void CopySet(const FDaxSet& Other);

    bool CopyNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID); // 拷贝覆盖到指定节点

    bool SwapNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID); // Swap内部走权威Copy, 禁止使用任何move

    bool MoveNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID); // 移动节点, 内部走权威拷贝

    FDaxResultDetail ResetToEmpty(const FDaxNodeID ID);

    FDaxResultDetail ResetToEmptyArray(const FDaxNodeID ID);

    FDaxResultDetail ResetToEmptyMap(const FDaxNodeID ID);

    FORCEINLINE ArzDax::FDaxNode* TryGetNode(const FDaxNodeID ID) {
        if (!bRunningOnServer)
            if (auto It = OverlayMap.find(ID); It != OverlayMap.end()) return It->second.Get();
        return Allocator.TryGetNode(ID);
    }

public:
    FORCEINLINE bool IsNodeValid(const FDaxNodeID ID) const { return Allocator.IsNodeValid(ID); }

    FORCEINLINE bool IsNodeArray(const FDaxNodeID ID) const { return GetNodeValueType(ID) == FDaxFakeTypeArray::StaticStruct(); }

    FORCEINLINE bool IsNodeMap(const FDaxNodeID ID) const { return GetNodeValueType(ID) == FDaxFakeTypeMap::StaticStruct(); }

    FORCEINLINE bool IsNodeValue(const FDaxNodeID ID) const {
        const auto Type = GetNodeValueType(ID);
        return Type != nullptr && Type != FDaxFakeTypeArray::StaticStruct() && Type != FDaxFakeTypeMap::StaticStruct() && Type != FDaxFakeTypeEmpty::StaticStruct();
    }

    FORCEINLINE bool IsNodeEmpty(const FDaxNodeID ID) const { return GetNodeValueType(ID) == FDaxFakeTypeEmpty::StaticStruct(); }

    FORCEINLINE bool IsNodeAncestor(const FDaxNodeID Parent, const FDaxNodeID Child) const { return Allocator.IsAncestor(Parent, Child); }

    FORCEINLINE bool IsNodeAncestorOrDescendant(const FDaxNodeID A, const FDaxNodeID B) const { return Allocator.IsAncestor(A, B) || Allocator.IsAncestor(B, A); }

    FORCEINLINE FDaxNodeID GetNodeParent(const FDaxNodeID Child) const { return Allocator.GetParent(Child); }

    FORCEINLINE const UScriptStruct* GetNodeValueType(FDaxNodeID ID) const { return Allocator.GetValueType(ID); }

    FORCEINLINE uint32 GetNodeNumRecursive(const FDaxNodeID ID) const;

private:
    friend struct FDaxVisitor;
    friend class UDaxComponent;

    FORCEINLINE void BumpOnlyNodeDataVersion(const FDaxNodeID ID) {
        if (!bRunningOnServer) return;
        Allocator.MarkDirty(ID, true);
        FrameChangedNodes.insert(ID);
    }

    FORCEINLINE void BumpNodeDataVersion(const FDaxNodeID ID) {
        if (!bRunningOnServer) return;
        Allocator.MarkDirty(ID, true);
        FrameChangedNodes.insert(ID);
        BumpDataVersion();
    }

    FORCEINLINE void BumpNodeType(const FDaxNodeID ID, const UScriptStruct* NewType) {
        if (!bRunningOnServer) return;
        Allocator.UpdateValueType(ID, NewType);
    }

    FORCEINLINE void BumpNodeDataVersionAndStruct(const FDaxNodeID ID) {
        if (!bRunningOnServer) return;
        Allocator.MarkDirty(ID, true);
        FrameChangedNodes.insert(ID);
        BumpStructVersion();
    }

    void BumpDataVersion();

    void BumpStructVersion();

private:
    FORCEINLINE FDaxNodeID NewNode() { return Allocator.Allocate(); }

    bool RedirectNode(const FDaxNodeID Old, const FDaxNodeID New);

    bool ReleaseNode(const FDaxNodeID ID);

    int32 ReleaseRecursive(const FDaxNodeID ID);

    int32 ReleaseChildren(const FDaxNodeID ID);

    void ReleaseSubtreeImp(const FDaxNodeID ID, uint32& ClearNum);

    void GetNodeNumRecursiveImp(const FDaxNodeID ID, int32& ClearNum) const;

    bool IsRemainingSpaceSupportCopy(const FDaxSet& Source, const FDaxNodeID SourceID) const;

    bool IsRemainingSpaceSupportInnerSwap(const FDaxNodeID A, const FDaxNodeID B) const;

    FDaxNodeID DeepCopyNode(FDaxSet& Source, const FDaxNodeID SourceID);

    FDaxNodeID DeepCopyNodeImpl(FDaxSet& Source, const FDaxNodeID SourceID, int& CopyCount);

    //Client:

    ArzDax::FDaxNode* GetOrCreateOverlayValueNode(const FDaxNodeID ID) {
        if (const auto It = OverlayMap.find(ID); It != OverlayMap.end()) {
            return It->second.Get();
        }
        else {
            auto Ref = OverlayMap.emplace(ID, MakeUnique<ArzDax::FDaxNode>());
            return Ref.first->second.Get();
        }
    }

    FORCEINLINE void ClearOverlayMap() { OverlayMap.clear(); }

private:
    ArzDax::FDaxAllocator Allocator{};

    TSharedPtr<uint8> LiveToken{};

    bool bRunningOnServer = true;

    FDaxNodeID RootID{};

    uint32 DataVersion = 0;

    uint32 StructVersion = 0;

    TWeakObjectPtr<UDaxComponent> ParentComponent{};

    TArray<FDaxOnChangedBinding> OnChangedBindings{};

    ankerl::unordered_dense::map<FDaxNodeID, TUniquePtr<ArzDax::FDaxNode>,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TUniquePtr<ArzDax::FDaxNode>>>> OverlayMap{};

    ankerl::unordered_dense::map<FDaxNodeID, FInstancedStruct,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<std::pair<FDaxNodeID, FInstancedStruct>>> OldValueMap{};

    ankerl::unordered_dense::set<FDaxNodeID,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<FDaxNodeID>> FrameChangedNodes{};

public:
    void Clear();

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

    bool operator==(const FDaxSet& Other) const {
        return Other.DataVersion == DataVersion && Other.StructVersion == StructVersion;
    }

    FConstStructView TryGetOldValueByNodeID(const FDaxNodeID NodeID) const;

    // 本帧变更节点集合：供 Subsystem 在分发后清理
    FORCEINLINE void ClearFrameChangedNodes() { FrameChangedNodes.clear(); }

public:
    bool BindOnChanged(const FDaxVisitor& Position, int32 Depth, const FDaxOnChangedDynamic& Delegate);
    void UnbindOnChanged(const FDaxVisitor& Position);
    void UnbindAllFor(UObject* TargetObject);

private:
    bool Sync_ServerFullWrite(FNetDeltaSerializeInfo& DeltaParms);
    bool Sync_ServerDeltaWrite(FNetDeltaSerializeInfo& DeltaParms, FDaxSetBaseState* OldState);

    bool Sync_ClientFullRead(FNetDeltaSerializeInfo& DeltaParms);
    bool Sync_ClientDeltaRead(FNetDeltaSerializeInfo& DeltaParms);
};

template <>
struct TStructOpsTypeTraits<FDaxSet> : public TStructOpsTypeTraitsBase2<FDaxSet> {
    enum {
        WithNetDeltaSerializer = true,
        WithIdenticalViaEquality = true,
    };
};

class FDaxSetBaseState : public INetDeltaBaseState {
    using FDaxArrayMirrorType = ankerl::unordered_dense::map<FDaxNodeID, TArray<FDaxNodeID>,
                                                             ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual, ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TArray<FDaxNodeID>>>>;

    using FDaxMapMirrorType = ankerl::unordered_dense::map<FDaxNodeID, ArzDax::FDaxMapType,
                                                           ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual, ArzDax::TDaxAllocator<std::pair<FDaxNodeID, ArzDax::FDaxMapType>>>;

public:
    uint32 ContainerVersion{};

    TArray<TUniquePtr<ArzDax::FDaxNodeChunkMeta>, TInlineAllocator<4>> ChildStates{};

    FDaxArrayMirrorType ArrayMirror{};

    FDaxMapMirrorType MapMirror{};

    virtual bool IsStateEqual(INetDeltaBaseState* OtherState) override {
        FDaxSetBaseState* Other = static_cast<FDaxSetBaseState*>(OtherState);
        if (!Other) return false;

        if (ContainerVersion == Other->ContainerVersion) return true;
        return false;
    }

    virtual void CountBytes(FArchive& Ar) const override {}
};
