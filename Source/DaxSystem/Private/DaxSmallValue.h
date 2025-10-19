#pragma once
#include "StructUtils/StructView.h"
#include "DaxSystem/Private/DaxCommon.h"
#include "AngelscriptCode/Public/AngelscriptManager.h"

namespace ArzDax {
    struct FDaxSmallValue {
        static constexpr int32 InlineSize = 32;
        static constexpr int32 InlineAlign = 16;

        TAlignedBytes<InlineSize, InlineAlign> Data{};
        const UScriptStruct* ScriptStruct = nullptr;

        FDaxSmallValue() = delete;

        FDaxSmallValue(FDaxSmallValue&& Other) = delete;
        FDaxSmallValue& operator=(FDaxSmallValue&& Other) = delete;

        FDaxSmallValue(const FDaxSmallValue& Other) : ScriptStruct(Other.ScriptStruct) {
            const UScriptStruct* S = ScriptStruct;
            checkf(S, TEXT("FDaxSmallValue copy: ScriptStruct invalid"));
            S->CopyScriptStruct(Data.Pad, Other.Data.Pad);
        }

        FDaxSmallValue& operator=(const FDaxSmallValue& Other) {
            if (this != &Other) {
                Reset();
                const UScriptStruct* S = Other.ScriptStruct;
                checkf(S, TEXT("FDaxSmallValue copy=: ScriptStruct invalid"));
                ScriptStruct = Other.ScriptStruct;
                S->CopyScriptStruct(Data.Pad, Other.Data.Pad);
            }
            return *this;
        }

        explicit FDaxSmallValue(const UScriptStruct* InScriptStruct) : ScriptStruct(InScriptStruct) {
            const UScriptStruct* S = ScriptStruct;
            checkf(S, TEXT("FDaxSmallValue(UScriptStruct*): ScriptStruct invalid"));
            checkf(CanInline(S), TEXT("FDaxSmallValue: size %d align %d exceed inline limit (size %d, align %d)"),
                   S->GetStructureSize(), S->GetMinAlignment(),
                   InlineSize, InlineAlign);

            S->InitializeStruct(Data.Pad);
        }

        explicit FDaxSmallValue(const FConstStructView InStruct) {
            checkf(InStruct.IsValid(), TEXT("FDaxSmallValue(FConstStructView): InStruct invalid"));
            ScriptStruct = InStruct.GetScriptStruct();
            const UScriptStruct* S = ScriptStruct;

            checkf(S, TEXT("FDaxSmallValue(FConstStructView): ScriptStruct invalid"));
            checkf(CanInline(S), TEXT("FDaxSmallValue: size %d align %d exceed inline limit (size %d, align %d)"),
                   S->GetStructureSize(), S->GetMinAlignment(),
                   InlineSize, InlineAlign);

            S->InitializeStruct(Data.Pad);
            S->CopyScriptStruct(Data.Pad, InStruct.GetMemory());
        }

        ~FDaxSmallValue() {
            Reset();
        }

        const UScriptStruct* GetScriptStruct() const noexcept { return ScriptStruct; }

        EDaxResult TrySet(const FConstStructView InStruct, EPropertyPortFlags Flags = PPF_None) {
            const UScriptStruct* S = ScriptStruct;

            if (!S) {
                UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
                const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
                for (const auto& Frame : Stack) {
                    UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
                }
                checkf(false, TEXT("FDaxSmallValue::TrySet(self): invalid"));
            }

            if (!InStruct.IsValid())
                return EDaxResult::InvalidTargetValue;

            if (InStruct.GetScriptStruct() != S)
                return EDaxResult::ValueTypeMismatch;

            if (S->CompareScriptStruct(Data.Pad, InStruct.GetMemory(), Flags))
                return EDaxResult::SameValueNotChange;

            S->CopyScriptStruct(Data.Pad, InStruct.GetMemory());
            return EDaxResult::SuccessChangeValue;
        }

        EDaxResult TrySet(const FDaxSmallValue& InStruct, EPropertyPortFlags Flags = PPF_None) {
            const UScriptStruct* S = ScriptStruct;
            if (!S) {
                UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
                const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
                for (const auto& Frame : Stack) {
                    UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
                }
                checkf(false, TEXT("FDaxSmallValue::TrySet(self): invalid"));
            }

            if (!InStruct.ScriptStruct) {
                UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
                const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
                for (const auto& Frame : Stack) {
                    UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
                }
                checkf(InStruct.ScriptStruct, TEXT("FDaxSmallValue::TrySet(param): invalid"));
            }

            if (InStruct.ScriptStruct != ScriptStruct)
                return EDaxResult::ValueTypeMismatch;

            if (S->CompareScriptStruct(Data.Pad, InStruct.Data.Pad, Flags))
                return EDaxResult::SameValueNotChange;

            S->CopyScriptStruct(Data.Pad, InStruct.Data.Pad);
            return EDaxResult::SuccessChangeValue;
        }

        FConstStructView TryGet(const UScriptStruct* ValueType) const {
            const UScriptStruct* S = ScriptStruct;

            if (!S) {
                UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
                const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
                for (const auto& Frame : Stack) {
                    UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
                }
                checkf(S, TEXT("FDaxSmallValue::TryGet: self Value is invalid"));
            }
            if (ValueType != S) return {};
            return FConstStructView(S, Data.Pad);
        }

        FConstStructView TryGetGeneric() const {
            const UScriptStruct* S = ScriptStruct;
            if (!S) {
                UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
                const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
                for (const auto& Frame : Stack) {
                    UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
                }
                checkf(S, TEXT("FDaxSmallValue::TryGetGeneric: self Value is invalid"));
            }
            return FConstStructView(S, Data.Pad);
        }

        FStructView TryGetGenericMutable() {
            const UScriptStruct* S = ScriptStruct;
            if (!S) {
                UE_LOG(DataXSystem, Error, TEXT("==== Stack Trace:"));
                const auto Stack = FAngelscriptManager::Get().GetAngelscriptCallstack();
                for (const auto& Frame : Stack) {
                    UE_LOGFMT(DataXSystem, Error, "==== {0}", Frame);
                }
                checkf(S, TEXT("FDaxSmallValue::TryGetGenericMutable: self Value is invalid"));
            }
            return FStructView(S, Data.Pad);
        }

        bool Identical(const FDaxSmallValue* Other, uint32 PortFlags) const {
            if (this == Other) return true;
            if (!Other) return false;
            const UScriptStruct* S = ScriptStruct;
            const UScriptStruct* OS = Other->ScriptStruct;
            checkf(S && OS, TEXT("FDaxSmallValue::Identical: invalid"));
            if (S != OS) return false;
            return S->CompareScriptStruct(Data.Pad, Other->Data.Pad, (EPropertyPortFlags)PortFlags);
        }

        bool IdenticalTo(const FConstStructView Other, EPropertyPortFlags Flags = PPF_None) const {
            const UScriptStruct* S = ScriptStruct;
            if (!Other.IsValid() || Other.GetScriptStruct() != S) return false;
            return S->CompareScriptStruct(Data.Pad, Other.GetMemory(), Flags);
        }

        static bool CanInline(const UScriptStruct* S) {
            checkf(S != nullptr, TEXT("FDaxSmallValue::CanInline: ScriptStruct invalid"));
            const int32 Size = S->GetStructureSize();
            const int32 Align = S->GetMinAlignment();
            return Size <= InlineSize && Align <= InlineAlign;
        }

        bool CanInline() const { return ScriptStruct != nullptr && CanInline(ScriptStruct); }

    private:
        void Reset() {
            if (const UScriptStruct* S = ScriptStruct) {
                S->DestroyStruct(Data.Pad);
                ScriptStruct = nullptr;
            }
        }
    };

    static_assert(alignof(FDaxSmallValue) >= FDaxSmallValue::InlineAlign, "FDaxSmallValue must keep 16B alignment");
}