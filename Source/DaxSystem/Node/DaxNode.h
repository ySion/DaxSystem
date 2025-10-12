#pragma once

#include "CoreMinimal.h"
#include "DaxSmallValue.h"
#include "DaxHeapValue.h"
#include "DaxArray.h"
#include "DaxMap.h"

namespace ArzDax {
    struct FDaxEmpty {};

    struct FDaxNode {
        TVariant<FDaxEmpty, FDaxSmallValue, FDaxHeapValue, FDaxArray, FDaxMap> Value{};

        FDaxNode() { Value.Emplace<FDaxEmpty>(); }
        ~FDaxNode() = default;
        FDaxNode(FDaxNode&& Other) = delete;
        FDaxNode& operator=(FDaxNode&& Other) = delete;
        FDaxNode(const FDaxNode& Other) : Value(Other.Value) {};

        FDaxNode& operator=(const FDaxNode& Other) {
            if (this != &Other) Value = Other.Value;
            return *this;
        }

        FORCEINLINE bool IsEmpty() const { return Value.IsType<FDaxEmpty>(); }
        FORCEINLINE bool IsValue() const { return Value.IsType<FDaxSmallValue>() || Value.IsType<FDaxHeapValue>(); }
        FORCEINLINE bool IsArray() const { return Value.IsType<FDaxArray>(); }
        FORCEINLINE bool IsMap() const { return Value.IsType<FDaxMap>(); }
        FORCEINLINE bool IsCompound() const { return IsArray() || IsMap(); }

        FORCEINLINE bool IsEmptyArray() const { return Value.IsType<FDaxArray>() && Value.Get<FDaxArray>().Children.IsEmpty(); }
        FORCEINLINE bool IsEmptyMap() const { return Value.IsType<FDaxMap>() && Value.Get<FDaxMap>().Children().empty(); }
        FORCEINLINE bool IsEmptyCompound() const { return IsEmptyArray() || IsEmptyMap(); }

        FName GetTypeName();

        EDaxResult ResetToEmpty() {
            if (!IsEmpty()) {
                if (IsEmptyArray() || IsEmptyMap()) {
                    Value.Emplace<FDaxEmpty>();
                    return EDaxResult::SuccessOverrideEmpty;
                }
                else {
                    Value.Emplace<FDaxEmpty>();
                    return EDaxResult::SuccessChangeValueAndType;
                }
            }
            return EDaxResult::SameValueNotChange;
        }

        EDaxResult ResetToEmptyArray() {
            if (IsArray()) {
                auto& array = Value.Get<FDaxArray>().Children;
                if (array.Num() > 0) {
                    array.Empty();
                    return EDaxResult::SuccessChangeValue;
                }
                else {
                    return EDaxResult::SameValueNotChange;
                }
            }

            if (IsEmpty()) {
                Value.Emplace<FDaxArray>();
                return EDaxResult::SuccessOverrideEmpty;
            }
            else {
                Value.Emplace<FDaxArray>();
                return EDaxResult::SuccessChangeValueAndType;
            }
        }

        EDaxResult ResetToEmptyMap() {
            if (IsMap()) {
                auto& Map = Value.Get<FDaxMap>().Children();
                if (Map.size() > 0) {
                    Map.clear();
                    return EDaxResult::SuccessChangeValue;
                }
                else {
                    return EDaxResult::SameValueNotChange;
                }
            }

            if (IsEmpty()) {
                Value.Emplace<FDaxMap>();
                return EDaxResult::SuccessOverrideEmpty;
            }
            else {
                Value.Emplace<FDaxMap>();
                return EDaxResult::SuccessChangeValueAndType;
            }
        }

        FORCEINLINE FDaxHeapValue* GetValueWrapper() { return Value.TryGet<FDaxHeapValue>(); }

        FORCEINLINE FDaxArray* GetArrayWrapper() { return Value.TryGet<FDaxArray>(); }

        FORCEINLINE const FDaxArray* GetArrayWrapper() const { return Value.TryGet<FDaxArray>(); }

        FORCEINLINE FDaxMap* GetMapWrapper() { return Value.TryGet<FDaxMap>(); }

        FORCEINLINE const FDaxMap* GetMapWrapper() const { return Value.TryGet<FDaxMap>(); }

        FORCEINLINE FDaxArray::ChildrenType* GetArray() {
            if (auto* Ptr = GetArrayWrapper()) return &Ptr->Children;
            return nullptr;
        }

        FORCEINLINE const FDaxArray::ChildrenType* GetArray() const {
            if (const auto* Ptr = GetArrayWrapper()) return &Ptr->Children;
            return nullptr;
        }

        FORCEINLINE FDaxMapType* GetMap() {
            if (auto* Ptr = GetMapWrapper()) return &Ptr->Children();
            return nullptr;
        }

        FORCEINLINE const FDaxMapType* GetMap() const {
            if (const auto* Ptr = GetMapWrapper()) return &Ptr->Children();
            return nullptr;
        }

        EDaxResult TrySetValue(const FConstStructView Target) {
            if (!Target.IsValid()) return EDaxResult::InvalidTargetValue;
            if (IsEmpty()) {
                const UScriptStruct* S = Target.GetScriptStruct();
                const bool canInline = (S->GetStructureSize() <= FDaxSmallValue::InlineSize) &&
                    (S->GetMinAlignment() <= FDaxSmallValue::InlineAlign);
                if (canInline) {
                    Value.Emplace<FDaxSmallValue>(Target);
                }
                else {
                    Value.Emplace<FDaxHeapValue>(Target);
                }
                return EDaxResult::SuccessChangeValueAndType;
            }
            else {
                if (auto* H = Value.TryGet<FDaxHeapValue>()) return H->TrySet(Target);
                if (auto* S = Value.TryGet<FDaxSmallValue>()) return S->TrySet(Target);
                return EDaxResult::ValueTypeMismatch;
            }
        }

        FORCEINLINE FConstStructView TryGetValue(const UScriptStruct* ValueType) const {
            if (auto* H = Value.TryGet<FDaxHeapValue>()) return H->TryGet(ValueType);
            if (auto* S = Value.TryGet<FDaxSmallValue>()) return S->TryGet(ValueType);
            return {};
        }

        FORCEINLINE FConstStructView TryGetValueGeneric() const {
            if (auto* H = Value.TryGet<FDaxHeapValue>()) return H->TryGetGeneric();
            if (auto* S = Value.TryGet<FDaxSmallValue>()) return S->TryGetGeneric();
            return {};
        }

        FORCEINLINE FStructView TryGetValueGenericMutable() {
            if (auto* H = Value.TryGet<FDaxHeapValue>()) return H->TryGetGenericMutable();
            if (auto* S = Value.TryGet<FDaxSmallValue>()) return S->TryGetGenericMutable();
            return {};
        }

        bool Identical(const FDaxNode* Other, uint32 PortFlags) const;

        bool SerializeValueData(FArchive& Ar, UPackageMap* Map, const UScriptStruct* Type) {
            if (Ar.IsLoading()) {
                ResetToEmpty();
                auto Temp = FInstancedStruct();
                Temp.InitializeAs(Type);
                if (Type->GetCppStructOps()->HasNetSerializer()) {
                    bool Success = true;
                    Type->GetCppStructOps()->NetSerialize(Ar, Map, Success, Temp.GetMutableMemory());
                }
                else {
                    Type->SerializeBin(Ar, Temp.GetMutableMemory());
                }
                TrySetValue(Temp);
            }
            else if (Ar.IsSaving()) {
                if (!IsValue()) return false;
                auto SV = TryGetValueGenericMutable();
                auto SS = SV.GetScriptStruct();
                if (SS != Type) return false;
                if (SS->GetCppStructOps()->HasNetSerializer()) {
                    bool Success = true;
                    SS->GetCppStructOps()->NetSerialize(Ar, Map, Success, SV.GetMemory());
                }
                else {
                    SS->SerializeBin(Ar, SV.GetMemory());
                }
            }

            return true;
        }
    };
}