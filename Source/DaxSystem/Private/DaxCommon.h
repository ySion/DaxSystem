#pragma once

#include "CoreMinimal.h"
#include "DaxCommon.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(DataXSystem, Log, All);

// ================== 可控调试日志开关（网络增量同步） ==================
#ifndef DAX_ENABLE_NET_SYNC_LOG
#define DAX_ENABLE_NET_SYNC_LOG 0
#endif

#if DAX_ENABLE_NET_SYNC_LOG
#define DAX_NET_SYNC_LOG(Verbosity, Fmt, ...) UE_LOGFMT(DataXSystem, Verbosity, Fmt, ##__VA_ARGS__)
#else
#define DAX_NET_SYNC_LOG(Verbosity, Fmt, ...) do { } while (0)
#endif

namespace ArzDax {
    // 父边类型（用于子节点到父容器的反向映射）
    enum class EDaxParentEdgeKind : uint8 { None = 0, Array = 1, Map = 2 };

    struct DaxFNameHash {
        using is_avalanching = void;

        auto operator()(const FName& ID) const noexcept {
            return GetTypeHash(ID);
        }
    };

    struct DaxFNameEqual {
        auto operator()(const FName& A, const FName& B) const noexcept {
            return A == B;
        }
    };

    enum class FDaxAllocateResult : uint8 {
        Failed, // 分配失败
        Exist, // 返回已经存在的
        Replaced, // 返回已经替换的
        NewOne, // 新对象
    };

    // ================= 增量网络同步操作与字段标识（压缩至1字节） =================
    // bit7..6: 操作类型（00=Remove, 01=Add, 10=Update, 11=保留）
    // bit5: 是否携带 Parent（HasParent）
    // bit4: 是否携带 Type（HasType）
    // bit3: 值类型是否包含有效负载（HasValuePayload）
    // bit2: 容器是否以增量方式编码（HasContainerDelta）
    // bit1: 容器是否以全量方式编码（IsFullContainer）
    // bit0: 保留
    enum class EDaxDeltaOp : uint8 { Remove = 0, Add = 1, Update = 2 };

    enum : uint8 {
        DAX_DELTA_OP_SHIFT = 6,
        DAX_DELTA_OP_MASK = 0xC0,
        DAX_DELTA_FLAG_PARENT = 0x20,
        DAX_DELTA_FLAG_TYPE = 0x10,
        DAX_DELTA_FLAG_VALUE = 0x08,
        DAX_DELTA_FLAG_CDELTA = 0x04,
        DAX_DELTA_FLAG_CFULL = 0x02,
    };

    static inline uint8 DaxMakeDeltaFlags(EDaxDeltaOp Op, bool bHasParent, bool bHasType, bool bHasValuePayload, bool bHasContainerDelta, bool bIsFullContainer) {
        uint8 Flags = (static_cast<uint8>(Op) << DAX_DELTA_OP_SHIFT) & DAX_DELTA_OP_MASK;
        if (bHasParent) Flags |= DAX_DELTA_FLAG_PARENT;
        if (bHasType) Flags |= DAX_DELTA_FLAG_TYPE;
        if (bHasValuePayload) Flags |= DAX_DELTA_FLAG_VALUE;
        if (bHasContainerDelta) Flags |= DAX_DELTA_FLAG_CDELTA;
        if (bIsFullContainer) Flags |= DAX_DELTA_FLAG_CFULL;
        return Flags;
    }

    static inline EDaxDeltaOp DaxGetDeltaOp(uint8 Flags) {
        return static_cast<EDaxDeltaOp>((Flags & DAX_DELTA_OP_MASK) >> DAX_DELTA_OP_SHIFT);
    }

    static inline bool DaxFlagHasParent(uint8 Flags) { return (Flags & DAX_DELTA_FLAG_PARENT) != 0; }
    static inline bool DaxFlagHasType(uint8 Flags) { return (Flags & DAX_DELTA_FLAG_TYPE) != 0; }
    static inline bool DaxFlagHasValue(uint8 Flags) { return (Flags & DAX_DELTA_FLAG_VALUE) != 0; }
    static inline bool DaxFlagHasCDelta(uint8 Flags) { return (Flags & DAX_DELTA_FLAG_CDELTA) != 0; }
    static inline bool DaxFlagIsCFull(uint8 Flags) { return (Flags & DAX_DELTA_FLAG_CFULL) != 0; }
}


USTRUCT()
struct FDaxFakeTypeEmpty {
    GENERATED_BODY()
    UPROPERTY()
    uint8 SomeThing = 0;
};

USTRUCT()
struct FDaxFakeTypeMap {
    GENERATED_BODY()
    UPROPERTY()
    uint8 SomeThing = 0;
};

USTRUCT()
struct FDaxFakeTypeArray {
    GENERATED_BODY()
    UPROPERTY()
    uint8 SomeThing = 0;
};

UENUM(BlueprintType)
enum class EDaxResult : uint8 {
    // 成功
    Success,

    // ================= 改值 =================
    SuccessOverrideEmpty,
    SuccessChangeValue,
    SuccessChangeValueAndType,
    SameValueNotChange,
    ValueTypeMismatch,
    InvalidTargetValue,
    InvalidSourceValue,

    // ================= Resolve相关 =================

    //客户端无权限
    PermissionDenied,
    // 访问器或容器无效
    InvalidVisitor,
    InvalidNode,
    InvalidRootNode,
    // 路径与深度
    PathEmptyResolvedToRoot,
    ResolvePathTooDeep,
    // 段类型/节点类型不匹配
    SegmentNameButNodeNotMap,
    SegmentIndexButNodeNotArray,
    // Map/Array 访问问题
    ResolveMapKeyNotFound,
    ResolveArrayIndexNegative,
    ResolveArrayIndexOutOfRange, // 不允许以跳跃式方式创建索引
    // 操作无效
    ResolveOperatorFailure,
    UnknownFailure,
    // 内部防御（理论上不应发生）
    ResolveInternalNullMap,
    ResolveInternalNullArray,
    // 资源/分配问题
    ResolveAllocateFailed
};

USTRUCT(BlueprintType)
struct DAXSYSTEM_API FDaxResultDetail {
    GENERATED_BODY()

    UPROPERTY()
    EDaxResult Result = EDaxResult::Success;

    UPROPERTY()
    FString ResultMessage{};

    FDaxResultDetail() = default;
    FDaxResultDetail(const FDaxResultDetail&) = default;
    FDaxResultDetail& operator=(const FDaxResultDetail&) = default;
    FDaxResultDetail(EDaxResult In) : Result(In) {}
    FDaxResultDetail(EDaxResult In, const FString& Msg) : Result(In), ResultMessage(Msg) {}
    FORCEINLINE bool operator==(const EDaxResult& Res) const { return Result == Res; }

    operator EDaxResult() const { return Result; }

    FString GetResultString() const;

    FORCEINLINE bool IsSuccess() const { return Result == EDaxResult::Success; }
    FORCEINLINE bool IsOk() const { return Result <= EDaxResult::SameValueNotChange; }
    FORCEINLINE void ed() const {
        if (!IsOk())
            UE_LOGFMT(DataXSystem, Error, "Dax Resolve Failed: [{0}] : {1}", GetResultString(), ResultMessage);
    }

    FORCEINLINE void ed(const FString& Context) const {
        if (!IsOk())
            UE_LOGFMT(DataXSystem, Error, "[{0}] Dax Resolve Failed: [{1}] : {2}", Context, GetResultString(), ResultMessage);
    }
    
    void edv() const;
    void edvas(const FString& Context) const;
};