#pragma once

#include "CoreMinimal.h"
#include "DaxSystem/Private/DaxCommon.h"
#include "DaxSystem/Private/DaxNodeID.h"
#include "DaxSystem/Public/DaxBuiltinTypes.h"
#include "StructUtils/StructView.h"

#include "DaxVisitor.generated.h"

#define DAX_BUILTIN_TYPE_DECLARE(Ret, Param, Func) \
    TOptional<Ret> TryGet##Func() const; \
    TOptional<Ret> TryGetOld##Func() const; \
    FDaxResultDetail TrySet##Func(Param) const; \
    FDaxResultDetail EnsureAndSet##Func(Param) const;
#define DAX_BUILTIN_TYPE_DEFINE(Ret, Param, Func, BuiltInType) \
    TOptional<Ret> FDaxVisitor::TryGet##Func() const { \
        if (const auto view = TryGetValue<BuiltInType>(); view) return view->Value; \
        return {}; \
    } \
    TOptional<Ret> FDaxVisitor::TryGetOld##Func() const { \
        if (auto view = TryGetOldValue<BuiltInType>(); view)  return view->Value; \
        return {}; \
    } \
    FDaxResultDetail FDaxVisitor::TrySet##Func(Param) const { \
        return TrySetValue(FConstStructView::Make(BuiltInType{Value})); \
    } \
    FDaxResultDetail FDaxVisitor::EnsureAndSet##Func(Param) const { \
        return EnsureAndSetValue(FConstStructView::Make(BuiltInType{Value})); \
    }

namespace ArzDax {
    struct FDaxNode;

    template <typename T>
    concept IsValidNodeValueType = !std::is_base_of_v<FInstancedStruct, T> && requires(T t) { T::StaticStruct(); };
}

struct FDaxSet;

USTRUCT(BlueprintType)
struct DAXSYSTEM_API FDaxVisitor {
    GENERATED_BODY()

private:
    enum class EDaxPathResolveMode : uint8 {
        ReadOnly, // 只读访问
        EnsureCreate, // 确保路径存在（必要时创建）
        ForceOverride // 强制覆盖（可改变类型）
    };

public:
    FDaxVisitor() = default;
    FDaxVisitor(FDaxSet* Set, const TWeakPtr<uint8>& LiveToken) : TargetSet(Set), TargetLiveToken(LiveToken) {}
    FDaxVisitor(FDaxSet* Set, const TWeakPtr<uint8>& LiveToken, const TArray<TVariant<FName, int32>>& Path) : TargetSet(Set), TargetLiveToken(LiveToken), NodePath(Path) {}

    FDaxVisitor(const FDaxVisitor& Other)
        : TargetSet(Other.TargetSet), TargetLiveToken(Other.TargetLiveToken),
          NodePath(Other.NodePath), CachedNodeID(Other.CachedNodeID) {}

    FDaxVisitor& operator=(const FDaxVisitor& Other) {
        if (this != &Other) {
            TargetSet = Other.TargetSet;
            TargetLiveToken = Other.TargetLiveToken;
            NodePath = Other.NodePath;
            CachedNodeID = Other.CachedNodeID;
        }
        return *this;
    }


    
    // 基本访问器信息
    bool IsValid() const { return TargetSet != nullptr && TargetLiveToken.IsValid(); }
    bool HasData() const; // 访问器数据是否有效（即Set未销毁且路径可解析）
    bool IsAncestor(const FDaxVisitor& Other) const;


    
    // 基本数据情况
    bool IsAEmptyMap() const;
    bool IsAEmptyArray() const;
    bool IsAEmptyNode() const;
    bool IsANoEmptyMap() const;
    bool IsANoEmptyArray() const;


    
    // 路径/缓存辅助（用于监听深度匹配与OldValue关联）
    // 返回：当前访问器路径段数量
    int32 GetPathSegmentCount() const { return NodePath.Num(); }
    // 返回：若 Ancestor 为自身祖先，返回深度差（OtherDepth - AncestorDepth），否则返回 -1
    int32 GetDepthRelativeTo(const FDaxVisitor& Ancestor) const;
    // 返回：当前缓存的节点ID（即最近一次成功Resolve后的ID，或构造时直接指定的ID）
    FDaxNodeID GetCachedNodeID() const { return CachedNodeID; }
    // 返回：当结构版本变化导致重新解析前记录的旧节点ID
    FDaxNodeID GetOldNodeID() const { return OldNodeID; }


    
    // 寻址
    FDaxVisitor MakeVisitorToParent() const;

    FDaxVisitor MakeVisitorByName(FName Key) const;
    FDaxVisitor MakeVisitorByIndex(int32 Index) const;
    FDaxVisitor MakeVisitorByString(const FString& sName) const { return MakeVisitorByName(FName(sName)); }

    FDaxVisitor MakeVisitorByFullPath(const TArray<TVariant<FName, int32>>& Path);
    FDaxVisitor MakeVisitorByParsePath(const FString& FullPath);


    
    // 拷贝, Move, Swap
    FDaxResultDetail EnsureAndCopyFrom(const FDaxVisitor& Src) const;
    FDaxResultDetail EnsureAndSwapWith(const FDaxVisitor& Src) const;


    
    // 搜索
    template <typename T>
    FDaxVisitor SearchChildBy(T PredictFunc) const;

    FDaxVisitor SearchEmptyChildNode() const;
    FDaxVisitor SearchEmptyChildMapNode() const;
    FDaxVisitor SearchEmptyChildArrayNode() const;


    // C++ API
    FDaxVisitor operator[](FName Key) const { return MakeVisitorByName(Key); }
    FDaxVisitor operator[](int32 Index) const { return MakeVisitorByIndex(Index); }
    FDaxVisitor operator[](const FString& Key) const { return MakeVisitorByName(FName(Key)); }
    FDaxVisitor operator[](const char* Key) const { return MakeVisitorByName(FName(Key)); }


    // 修改器
    FDaxResultDetail TrySetToEmpty() const; // 清空当前节点（变成空节点）
    FConstStructView TryGetValue(const UScriptStruct* ValueType) const;
    FConstStructView TryGetValueGeneric() const;
    FDaxResultDetail TrySetValue(const FConstStructView Value) const;
    FDaxResultDetail EnsureAndSetValue(const FConstStructView Value) const;

    template <typename T> requires ArzDax::IsValidNodeValueType<T>
    const T* TryGetValue() const { return static_cast<const T*>(reinterpret_cast<const void*>(TryGetValue(T::StaticStruct()).GetMemory())); }

    template <typename T> requires ArzDax::IsValidNodeValueType<T>
    EDaxResult TrySetValue(const T& Value) const { return TrySetValue(FConstStructView::Make(Value)); }


    // 便捷特化：内置类型写入（避免上层自建USTRUCT）=
    DAX_BUILTIN_TYPE_DECLARE(bool, bool Value, Bool)
    DAX_BUILTIN_TYPE_DECLARE(int64, int64 Value, Int)
    DAX_BUILTIN_TYPE_DECLARE(float, float Value, Float)
    DAX_BUILTIN_TYPE_DECLARE(FName, const FName& Value, Name)
    DAX_BUILTIN_TYPE_DECLARE(FString, const FString& Value, String)
    DAX_BUILTIN_TYPE_DECLARE(FVector, const FVector& Value, Vector)
    DAX_BUILTIN_TYPE_DECLARE(FRotator, const FRotator& Value, Rotator)


    //Old Value
    FConstStructView TryGetOldValue(const UScriptStruct* ValueType) const;

    template <typename T> requires ArzDax::IsValidNodeValueType<T>
    const T* TryGetOldValue() const { return static_cast<const T*>(reinterpret_cast<const void*>(TryGetOldValue(T::StaticStruct()).GetMemory())); }


    // 核心结构函数
    FDaxVisitor EnsureArray() const; // 保证当前节点是Array, 如果是空节点, 那么就设置成Array
    FDaxVisitor EnsureMap() const; // 保证当前节点是Map, 如果是空节点, 那么就设置成Map


    // Array
    FDaxVisitor ArrayAdd() const;
    FDaxResultDetail ArrayEnsureMinNum(int32 Count) const; // 语义：确保至少Count个元素（不缩减）
    bool ArrayRemove() const;
    FDaxVisitor ArrayGet(int32 Index) const;
    FDaxVisitor ArrayInsert(int32 Index) const;
    int32 ArrayNum() const;
    bool ArrayClear() const;
    TArray<FDaxVisitor> ArrayGetChildren() const;
    FDaxVisitor ArrayGetPrev() const;
    FDaxVisitor ArrayGetNext() const;


    // Map
    FDaxVisitor MapAdd(FName Key) const;
    bool MapRemove(FName Key) const;
    FDaxVisitor MapGet(FName Key) const;
    int32 MapNum() const;
    bool MapClear() const;
    TArray<FDaxVisitor> MapGetChildren() const;


    // 层级关系查询
    int32 GetIndexInParentArray() const;
    FName GetKeyInParentMap() const;
    int32 GetIndexUnderAncestorArray(const FDaxVisitor& Ancestor) const;
    FName GetKeyUnderAncestorMap(const FDaxVisitor& Ancestor) const;


    // 信息打印系列
    FString GetString() const;
    FString GetStringDebug() const;
    FString GetStringDeep() const;
    FString GetStringDebugDeep() const;
    FString GetPathString() const;

    bool operator==(const FDaxVisitor& Other) const;
    
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

private:
    FDaxResultDetail ResolvePathInternal(EDaxPathResolveMode Mode) const;

    void ResetAll() {
        TargetSet = nullptr;
        TargetLiveToken = nullptr;
        NodePath.Reset();
        CachedNodeID = {};
        CachedSetStructVersion = 0;
        CachedNode = nullptr;
    }

private:
    FDaxSet* TargetSet{};

    TWeakPtr<uint8> TargetLiveToken{};

    TArray<TVariant<FName, int32>> NodePath{};

    mutable FDaxNodeID CachedNodeID{};

    mutable uint32 CachedSetStructVersion{};

    mutable ArzDax::FDaxNode* CachedNode{};

    mutable FDaxNodeID OldNodeID{};
};

template <>
struct TStructOpsTypeTraits<FDaxVisitor> : public TStructOpsTypeTraitsBase2<FDaxVisitor> {
    enum {
        WithNetSerializer = true,
        WithIdenticalViaEquality = true,
    };
};