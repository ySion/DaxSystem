#pragma once
#include "CoreMinimal.h"
#include "DaxAllocator.h"
#include "DaxVisitor.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Class.h"
#include "UObject/ScriptDelegates.h"
#include "DaxSet.generated.h"

class UDaxComponent;
DECLARE_STATS_GROUP(TEXT("DaxSystem"), STATGROUP_DaxSystem, STATCAT_DaxSystem)

DECLARE_CYCLE_STAT(TEXT("DaxSystem NetSyncTick"), STAT_NetSyncTick, STATGROUP_DaxSystem)
DECLARE_CYCLE_STAT(TEXT("DaxSystem NetDeltaSync"), STAT_NetDeltaSync, STATGROUP_DaxSystem)

// 前置声明，供同步实现函数签名使用
class FDaxSetBaseState;

namespace ArzDax {
    using OverlayMapType =
    ankerl::unordered_dense::map<FDaxNodeID, TUniquePtr<FDaxNode>, ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual, ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TUniquePtr<FDaxNode>>>>;

    struct FDaxGlobalSetManager;
}

// 动态委托：回调签名 void Func(const FDaxVisitor& ChangePosition)
DECLARE_DYNAMIC_DELEGATE_OneParam(FDaxOnChangedDynamic, const FDaxVisitor&, ChangePosition);

struct FDaxOnChangedBinding {
    FDaxVisitor Position;
    int32 Depth = 0; // 0=仅自身，1=自身+直子，>1按层数展开
    FDaxOnChangedDynamic Delegate; // 动态委托（蓝图/反射安全）

    bool IsValid() const { return Delegate.IsBound(); }

    // 缓存：绑定锚点（解析后的NodeID）及其对应的结构版本
    FDaxNodeID AnchorID{};
    uint32 AnchorStructVersion = 0;
};

USTRUCT(BlueprintType)
struct DAXSYSTEM_API FDaxSet {
    GENERATED_BODY()

    FDaxSet();
    FDaxSet(FDaxSet&& Other);
    FDaxSet& operator=(FDaxSet&& Other);
    FDaxSet& operator=(const FDaxSet& Other);

    FDaxSet(const FDaxSet& Other) {
        LiveToken = MakeShared<uint8>(0);
        CopySet(Other);
    }

    ~FDaxSet();

    FDaxVisitor GetVisitor() const { return FDaxVisitor(const_cast<FDaxSet*>(this), LiveToken); }
    
    FDaxVisitor GetVisitorFromPath(const FString& Path) const { return GetVisitor().MakeVisitorByParsePath(Path); }

private:
    void CopySet(const FDaxSet& Other);

    bool CopyNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID); // 拷贝覆盖到指定节点

    bool SwapNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID); // Swap内部走权威Copy, 禁止使用任何move

    bool MoveNode(const FDaxNodeID Target, const FDaxSet& SrcSet, const FDaxNodeID SrcSetNodeID); // 移动节点, 内部走权威拷贝

    FDaxResultDetail ResetToEmpty(const FDaxNodeID ID);

    FDaxResultDetail ResetToEmptyArray(const FDaxNodeID ID);

    FDaxResultDetail ResetToEmptyMap(const FDaxNodeID ID);

    ArzDax::FDaxNode* TryGetNode(const FDaxNodeID ID) {
        if (!bRunningOnServer)
            if (auto It = OverlayMap.find(ID); It != OverlayMap.end()) return It->second.Get();
        return Allocator.TryGetNode(ID);
    }

public:
    bool IsNetRegistered() const;

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

    FORCEINLINE uint32 GetNodeNum() const { return Allocator.Stats.CurrentActive; }

    FORCEINLINE uint32 GetNodeNumRecursive(const FDaxNodeID ID) const;

    FString GetString() const;

    FString GetStringDebug() const;

private:
    FORCEINLINE void BumpOnlyNodeDataVersion(const FDaxNodeID ID) {
        if (!bRunningOnServer) return;
        Allocator.MarkDirty(ID, true);;
    }

    FORCEINLINE void BumpNodeDataVersion(const FDaxNodeID ID) {
        if (!bRunningOnServer) return;
        Allocator.MarkDirty(ID, true);
        BumpDataVersion();
    }

    FORCEINLINE void BumpNodeType(const FDaxNodeID ID, const UScriptStruct* NewType) {
        if (!bRunningOnServer) return;
        Allocator.UpdateValueType(ID, NewType);
    }

    FORCEINLINE void BumpNodeDataVersionAndStruct(const FDaxNodeID ID) {
        if (!bRunningOnServer) return;
        Allocator.MarkDirty(ID, true);
        BumpStructVersion();
    }

    void BumpDataVersion();

    void BumpStructVersion();

private:
    friend struct FDaxVisitor;
    friend struct ArzDax::FDaxGlobalSetManager;
    friend class UDaxComponent;

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

    uint32 GlobalSetID = 0;

    TSharedPtr<uint8> LiveToken{};

    FDaxNodeID RootID{};

    uint32 DataVersion = 0;

    uint32 StructVersion = 0;

    bool bRunningOnServer = true;

    TWeakObjectPtr<UDaxComponent> ParentComponent{}; //if exist;

    TArray<FDaxOnChangedBinding> OnChangedBindings{};

    ankerl::unordered_dense::map<FDaxNodeID, TUniquePtr<ArzDax::FDaxNode>,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TUniquePtr<ArzDax::FDaxNode>>>> OverlayMap{};

    ankerl::unordered_dense::map<FDaxNodeID, FInstancedStruct,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<std::pair<FDaxNodeID, FInstancedStruct>>> OldValueMap{};

    // 仅客户端：锚点索引（AnchorID -> 绑定索引列表），以及索引对应的Set结构版本
    ankerl::unordered_dense::map<FDaxNodeID, TArray<int32>,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TArray<int32>>>> OnChangedIndex{};

    // 仅客户端：路径缓存（NodeID -> 路径），用于监听回调时快速构建访问器
    ankerl::unordered_dense::map<FDaxNodeID, TArray<TVariant<FName, int32>>,
                                 ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual,
                                 ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TArray<TVariant<FName, int32>>>>> PathCache{};

    uint32 OnChangedIndexStructVersion = 0;

public:
    void Clear();

    void RegisterDaxSetOnNetWork(FArchive& Ar);

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

    //friend FArchive& operator<<(FArchive& Ar, FDaxSet& S);

    bool operator==(const FDaxSet& Other) const {
        return Other.DataVersion == DataVersion && Other.StructVersion == StructVersion;
    }

    FConstStructView TryGetOldValueByNodeID(const FDaxNodeID NodeID) const;

public:
    bool BindOnChanged(const FDaxVisitor& Position, int32 Depth, const FDaxOnChangedDynamic& Delegate);
    void UnbindOnChanged(const FDaxVisitor& Position);
    void UnbindAllFor(UObject* TargetObject);

private:
    void RebuildOnChangedIndex();

private:
    bool Sync_ServerFullSyncImpl(FNetDeltaSerializeInfo& DeltaParms);
    bool Sync_ServerDeltaSyncImpl(FNetDeltaSerializeInfo& DeltaParms, FDaxSetBaseState* OldState);

    bool Sync_ClientFullSyncImpl(FNetDeltaSerializeInfo& DeltaParms);
    bool Sync_ClientDeltaSyncImpl(FNetDeltaSerializeInfo& DeltaParms);
};

template <>
struct TStructOpsTypeTraits<FDaxSet> : public TStructOpsTypeTraitsBase2<FDaxSet> {
    enum {
        WithNetDeltaSerializer = true,
        WithIdenticalViaEquality = true,
    };
};

class FDaxSetBaseState : public INetDeltaBaseState {
public:
    using FDaxArrayMirrorType = ankerl::unordered_dense::map<FDaxNodeID, TArray<FDaxNodeID>,
                                                             ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual, ArzDax::TDaxAllocator<std::pair<FDaxNodeID, TArray<FDaxNodeID>>>>;

    using FDaxMapMirrorType = ankerl::unordered_dense::map<FDaxNodeID, ArzDax::FDaxMapType,
                                                           ArzDax::FDaxNodeIDHash, ArzDax::FDaxNodeIDEqual, ArzDax::TDaxAllocator<std::pair<FDaxNodeID, ArzDax::FDaxMapType>>>;

    uint32 ContainerVersion{};

    TArray<TUniquePtr<ArzDax::FDaxNodeChunkMeta>, TInlineAllocator<4>> ChildStates{};

    FDaxArrayMirrorType ArrayMirror{};

    FDaxMapMirrorType MapMirror{};

    uint32 GlobalSetID = 0;

    virtual bool IsStateEqual(INetDeltaBaseState* OtherState) override {
        FDaxSetBaseState* Other = static_cast<FDaxSetBaseState*>(OtherState);
        if (!Other) return false;

        if (ContainerVersion == Other->ContainerVersion) return true;
        return false;
    }

    virtual void CountBytes(FArchive& Ar) const override {
        //Ar.CountBytes(sizeof(FArzNBTContainerBaseState), sizeof(FArzNBTContainerBaseState));
        //VersionChunks.CountBytes(Ar);
    }
};

namespace ArzDax {
    struct FDaxSetInfo {
        TWeakPtr<uint8> LiveToken{};
        FDaxSet* Ptr{};

        bool operator==(const FDaxSetInfo& Other) const {
            return LiveToken.Pin().Get() == Other.LiveToken.Pin().Get() && Ptr == Other.Ptr;
        }

        bool IsValid() const { return Ptr != nullptr && LiveToken.Pin().IsValid(); }
    };

    struct FDaxGlobalSetIDHash {
        using is_avalanching = void;

        auto operator()(const uint32 ID) const noexcept {
            return ID;
        }
    };

    struct FDaxGlobalSetEqual {
        auto operator()(const uint32 A, const uint32 B) const noexcept {
            return A == B;
        }
    };

    using GlobalDaxSetMap =
    ankerl::unordered_dense::map<uint32, FDaxSetInfo, FDaxGlobalSetIDHash, FDaxGlobalSetEqual, ArzDax::TDaxAllocator<std::pair<uint32, FDaxSetInfo>>>;

    struct FDaxGlobalSetManager {
        inline static GlobalDaxSetMap Map{};
        inline static uint32 NextID = 1;

        static uint32 Register(FDaxSet* Set) {
            if (Set->GlobalSetID != 0) return 0;
            if (NextID % 200 == 0) Gc(); //Gc频率

            NextID++;
            Set->GlobalSetID = NextID;
            Map.emplace(NextID, FDaxSetInfo{Set->LiveToken, Set});
            return NextID;
        }

        static void RegisterOnClient(uint32 ID, FDaxSet* Set) {
            Set->GlobalSetID = ID;
            Map.emplace(ID, FDaxSetInfo{Set->LiveToken, Set});
        }

        static void Gc() {
            erase_if(Map, [](const std::pair<uint32, FDaxSetInfo>& KV) {
                return !KV.second.IsValid();
            });
        }

        static void UnregisterOnClient(const FDaxSet* Set) {
            auto it = Map.find(Set->GlobalSetID);
            if (it != Map.end() && it->second.Ptr == Set) {
                Map.erase(it);
            }
        }

        static FDaxSetInfo TryGetDaxSetInfo(const uint32 GlobalSetID) {
            const auto It = Map.find(GlobalSetID);
            if (It == Map.end()) return {};
            return It->second;
        }
    };
}