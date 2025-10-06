#include "DaxCommon.h"
#include "AngelscriptManager.h"

DEFINE_LOG_CATEGORY(DataXSystem);

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