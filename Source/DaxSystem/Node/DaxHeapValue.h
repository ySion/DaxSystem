#pragma once
#include "DaxSystem/Basic/DaxCommon.h"
#include "StructUtils/StructView.h"

namespace ArzDax {
    struct FDaxHeapValue {
        FInstancedStruct Value;

        FDaxHeapValue() = delete;
        FDaxHeapValue(FDaxHeapValue&& Other) = delete;
        FDaxHeapValue& operator=(FDaxHeapValue&& Other) = delete;

        ~FDaxHeapValue() { Value.Reset(); }

        FDaxHeapValue(const FDaxHeapValue& Other) : Value(Other.Value) { checkf(Value.IsValid(), TEXT("FDaxHeapValue copy: Value invalid")); }

        FDaxHeapValue& operator=(const FDaxHeapValue& Other) {
            if (this != &Other) {
                Value = Other.Value;
                checkf(Value.IsValid(), TEXT("FDaxHeapValue copy=: Value invalid"));
            }
            return *this;
        }

        explicit FDaxHeapValue(const UScriptStruct* InScriptStruct) {
            checkf(IsValid(InScriptStruct), TEXT("FDaxHeapValue(UScriptStruct*): ScriptStruct invalid"));
            Value.InitializeAs(InScriptStruct); // 默认构造 struct
            checkf(Value.IsValid(), TEXT("FDaxHeapValue(UScriptStruct*): InitializeAs failed"));
        }

        explicit FDaxHeapValue(const FConstStructView InStruct) : Value(InStruct) {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue(FConstStructView): InStruct invalid"));
        }

        const UScriptStruct* GetScriptStruct() const noexcept { return Value.GetScriptStruct(); }

        EDaxResult TrySet(const FConstStructView InStruct, EPropertyPortFlags Flags = PPF_None) {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::TrySet: self Value is invalid"));
            if (!InStruct.IsValid())
                return EDaxResult::InvalidTargetValue;

            if (InStruct.GetScriptStruct() != Value.GetScriptStruct())
                return EDaxResult::ValueTypeMismatch;

            if (Identical(InStruct, Flags))
                return EDaxResult::SameValueNotChange;

            Value = InStruct;
            return EDaxResult::SuccessChangeValue;
        }

        EDaxResult TrySet(const FDaxHeapValue& Param, EPropertyPortFlags Flags = PPF_None) {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::TrySet: self Value is invalid"));
            if (!Param.Value.IsValid())
                return EDaxResult::InvalidTargetValue;

            return TrySet(Param.Value, Flags);
        }

        FConstStructView TryGet(const UScriptStruct* ValueType) const {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::TryGet: self Value is invalid"));
            if (ValueType != Value.GetScriptStruct()) return {};
            return Value;
        }

        FConstStructView TryGetGeneric() const {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::TryGetGeneric: self Value is invalid"));
            return Value;
        }

        FStructView TryGetGenericMutable() {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::TryGetGenericMutable: self Value is invalid"));
            return Value;
        }

        bool Identical(const FDaxHeapValue* Other) const {
            if (!Other) return false;
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::Identical: self invalid"));
            if (this == Other) return true;
            return Value.IsValid() && Other->Value.IsValid()
                       ? (Value.GetScriptStruct() == Other->Value.GetScriptStruct())
                       : (!Value.IsValid() && !Other->Value.IsValid());
        }

        bool Identical(const FDaxHeapValue* Other, uint32 PortFlags) const {
            if (!Other) return false;
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::Identical: self invalid"));
            if (this == Other) return true;
            checkf(Other->Value.IsValid(), TEXT("FDaxHeapValue::Identical: other invalid"));
            return Value.Identical(&Other->Value, PortFlags);
        }

        bool Identical(const FConstStructView Other, uint32 PortFlags) const {
            if (!Other.IsValid()) return false;
            checkf(Value.IsValid(), TEXT("FDaxHeapValue::Identical: self invalid"));
            const FConstStructView This = Value;
            if (This == Other) return true;
            return This.GetScriptStruct()->CompareScriptStruct(This.GetMemory(), Other.GetMemory(), PortFlags);
        }
    };
}