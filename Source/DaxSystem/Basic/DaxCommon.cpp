#include "DaxCommon.h"
#include "AngelscriptManager.h"

DEFINE_LOG_CATEGORY(DataXSystem);

FString FDaxResultDetail::GetResultString() const {
    switch (Result) {
    case EDaxResult::Success: return TEXT("Success");

    case EDaxResult::SuccessChangeValue: return TEXT("SuccessChangeValue");
    case EDaxResult::SuccessChangeValueAndType: return TEXT("SuccessChangeValueAndType");
    case EDaxResult::SameValueNotChange: return TEXT("SameValueNotChange");
    case EDaxResult::ValueTypeMismatch: return TEXT("ValueTypeMismatch");
    case EDaxResult::InvalidTargetValue: return TEXT("InvalidTargetValue");
    case EDaxResult::InvalidSourceValue: return TEXT("InvalidSourceValue");
    case EDaxResult::PermissionDenied: return TEXT("PermissionDenied");
            
    case EDaxResult::InvalidVisitor: return TEXT("InvalidVisitor");
    case EDaxResult::InvalidNode: return TEXT("InvalidNode");
    case EDaxResult::InvalidRootNode: return TEXT("InvalidRootNode");

    case EDaxResult::PathEmptyResolvedToRoot: return TEXT("PathEmptyResolvedToRoot");
    case EDaxResult::ResolvePathTooDeep: return TEXT("ResolvePathTooDeep");

    case EDaxResult::SegmentNameButNodeNotMap: return TEXT("SegmentNameButNodeNotMap");
    case EDaxResult::SegmentIndexButNodeNotArray: return TEXT("SegmentIndexButNodeNotArray");

    case EDaxResult::ResolveMapKeyNotFound: return TEXT("ResolveMapKeyNotFound");
    case EDaxResult::ResolveArrayIndexNegative: return TEXT("ResolveArrayIndexNegative");
    case EDaxResult::ResolveArrayIndexOutOfRange: return TEXT("ResolveArrayIndexOutOfRange");

    case EDaxResult::ResolveOperatorFailure: return TEXT("ResolveOperatorFailure");
    case EDaxResult::UnknownFailure: return TEXT("UnknownFailure");

    case EDaxResult::ResolveInternalNullMap: return TEXT("ResolveInternalNullMap");
    case EDaxResult::ResolveInternalNullArray: return TEXT("ResolveInternalNullArray");

    case EDaxResult::ResolveAllocateFailed: return TEXT("ResolveAllocateFailed");

    default: return TEXT("Unknown error");
    }
}

void FDaxResultDetail::edv() const {
    if (!IsOk()) {
        UE_LOGFMT(DataXSystem, Error, "==== DaxSystem Op Failed: ====");
        UE_LOGFMT(DataXSystem, Error, "==== Result: [{0}]", *GetResultString());
        UE_LOGFMT(DataXSystem, Error, "==== Message: {0}", *ResultMessage);
        UE_LOGFMT(DataXSystem, Error, "==== Stack Trace:");
            
        TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 5);
        for (const auto& Frame : Stack) {
            UE_LOGFMT(DataXSystem, Error, "==== {0} [{1}:{2}]",
                      Frame.FunctionName, Frame.Filename, Frame.LineNumber);
        }
    }
}

void FDaxResultDetail::edvas(const FString& Context) const {
    if (!IsOk()) {
        UE_LOGFMT(DataXSystem, Error, "==== DaxSystem Op Failed: ====");
        UE_LOGFMT(DataXSystem, Error, "==== Result: {0}", *GetResultString());
        UE_LOGFMT(DataXSystem, Error, "==== Message: {0}", *ResultMessage);
        
        UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
        const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
        for (const auto& Frame : Stack) {
            UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
        }
    }
}