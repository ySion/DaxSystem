#pragma once

#include "CoreMinimal.h"
#include "udm.h"
#include "DaxStdAllocatorGlue.h"
#include "DaxCommon.h"
#include "DaxNodeID.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

namespace ArzDax {
    struct FDaxEmpty {};

    struct FDaxSmallValue {
        static constexpr int32 InlineSize = 32;
        static constexpr int32 InlineAlign = 16;

        TAlignedBytes<InlineSize, InlineAlign> Data{};
        TWeakObjectPtr<const UScriptStruct> ScriptStruct = nullptr;

        FDaxSmallValue() = delete;

        FDaxSmallValue(FDaxSmallValue&& Other) {
            const UScriptStruct* S = Other.ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue move: ScriptStruct invalid"));
            ScriptStruct = Other.ScriptStruct;
            S->CopyScriptStruct(Data.Pad, Other.Data.Pad);
            Other.Reset();
        }

        FDaxSmallValue& operator=(FDaxSmallValue&& Other) {
            if (this != &Other) {
                Reset();
                const UScriptStruct* S = Other.ScriptStruct.Get();
                checkf(S, TEXT("FDaxSmallValue move: ScriptStruct invalid"));
                ScriptStruct = Other.ScriptStruct;
                S->CopyScriptStruct(Data.Pad, Other.Data.Pad);
                Other.Reset();
            }
            return *this;
        }

        FDaxSmallValue(const FDaxSmallValue& Other) : ScriptStruct(Other.ScriptStruct) {
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue copy: ScriptStruct invalid"));
            S->CopyScriptStruct(Data.Pad, Other.Data.Pad);
        }

        FDaxSmallValue& operator=(const FDaxSmallValue& Other) {
            if (this != &Other) {
                Reset();
                const UScriptStruct* S = Other.ScriptStruct.Get();
                checkf(S, TEXT("FDaxSmallValue copy=: ScriptStruct invalid"));
                ScriptStruct = Other.ScriptStruct;
                S->CopyScriptStruct(Data.Pad, Other.Data.Pad);
            }
            return *this;
        }

        explicit FDaxSmallValue(const UScriptStruct* InScriptStruct) : ScriptStruct(InScriptStruct) {
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue(UScriptStruct*): ScriptStruct invalid"));
            checkf(CanInline(S), TEXT("FDaxSmallValue: size %d align %d exceed inline limit (size %d, align %d)"),
                   S->GetStructureSize(), S->GetMinAlignment(),
                   InlineSize, InlineAlign);

            S->InitializeStruct(Data.Pad);
        }

        explicit FDaxSmallValue(const FConstStructView InStruct) {
            checkf(InStruct.IsValid(), TEXT("FDaxSmallValue(FConstStructView): InStruct invalid"));
            ScriptStruct = InStruct.GetScriptStruct();
            const UScriptStruct* S = ScriptStruct.Get();

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

        const UScriptStruct* GetScriptStruct() const noexcept { return ScriptStruct.Get(); }

        EDaxResult TrySet(const FConstStructView InStruct, EPropertyPortFlags Flags = PPF_None) {
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue::TrySet(self): invalid"));
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
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue::TrySet(self): invalid"));
            checkf(InStruct.ScriptStruct.Get(), TEXT("FDaxSmallValue::TrySet(param): invalid"));

            if (InStruct.ScriptStruct != ScriptStruct)
                return EDaxResult::ValueTypeMismatch;

            if (S->CompareScriptStruct(Data.Pad, InStruct.Data.Pad, Flags))
                return EDaxResult::SameValueNotChange;

            S->CopyScriptStruct(Data.Pad, InStruct.Data.Pad);
            return EDaxResult::SuccessChangeValue;
        }

        FConstStructView TryGet(const UScriptStruct* ValueType) const {
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue::TryGet: self Value is invalid"));
            if (ValueType != S) return {};
            return FConstStructView(S, Data.Pad);
        }

        FConstStructView TryGetGeneric() const {
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue::TryGetGeneric: self Value is invalid"));
            return FConstStructView(S, Data.Pad);
        }

        FStructView TryGetGenericMutable() {
            const UScriptStruct* S = ScriptStruct.Get();
            checkf(S, TEXT("FDaxSmallValue::TryGetGenericMutable: self Value is invalid"));
            return FStructView(S, Data.Pad);
        }

        bool Identical(const FDaxSmallValue* Other, uint32 PortFlags) const {
            if (this == Other) return true;
            if (!Other) return false;
            const UScriptStruct* S = ScriptStruct.Get();
            const UScriptStruct* OS = Other->ScriptStruct.Get();
            checkf(S && OS, TEXT("FDaxSmallValue::Identical: invalid"));
            if (S != OS) return false;
            return S->CompareScriptStruct(Data.Pad, Other->Data.Pad, (EPropertyPortFlags)PortFlags);
        }

        bool IdenticalTo(const FConstStructView Other, EPropertyPortFlags Flags = PPF_None) const {
            const UScriptStruct* S = ScriptStruct.Get();
            if (!Other.IsValid() || Other.GetScriptStruct() != S) return false;
            return S->CompareScriptStruct(Data.Pad, Other.GetMemory(), Flags);
        }

        static bool CanInline(const UScriptStruct* S) {
            checkf(IsValid(S), TEXT("FDaxSmallValue::CanInline: ScriptStruct invalid"));
            const int32 Size = S->GetStructureSize();
            const int32 Align = S->GetMinAlignment();
            return Size <= InlineSize && Align <= InlineAlign;
        }

        bool CanInline() const { return ScriptStruct.IsValid() && CanInline(ScriptStruct.Get()); }

    private:
        void Reset() {
            if (const UScriptStruct* S = ScriptStruct.Get()) {
                S->DestroyStruct(Data.Pad);
                ScriptStruct.Reset();
            }
        }
    };

    static_assert(alignof(FDaxSmallValue) >= FDaxSmallValue::InlineAlign, "FDaxSmallValue must keep 16B alignment");

    struct FDaxHeapValue {
        FInstancedStruct Value;

        FDaxHeapValue() = delete;

        FDaxHeapValue(FDaxHeapValue&& Other) : Value(MoveTemp(Other.Value)) {
            checkf(Value.IsValid(), TEXT("FDaxHeapValue move=: Value invalid"));
        }

        FDaxHeapValue& operator=(FDaxHeapValue&& Other) {
            if (this != &Other) {
                Value = MoveTemp(Other.Value);
                checkf(Value.IsValid(), TEXT("FDaxHeapValue move=: Value invalid"));
            }
            return *this;
        }

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

    struct FDaxArray { // 48B
        using ChildrenType = TArray<FDaxNodeID, TInlineAllocator<8>>;
        ChildrenType Children;
        FDaxArray() = default;
        FDaxArray(const FDaxArray& other) : Children(other.Children) {};
        FDaxArray(FDaxArray&& other) : Children(MoveTemp(other.Children)) {};

        FDaxArray& operator=(const FDaxArray& other) {
            if (this != &other) Children = other.Children;
            return *this;
        }

        FDaxArray& operator=(FDaxArray&& other) {
            if (this != &other) Children = MoveTemp(other.Children);
            return *this;
        }

        bool Identical(const FDaxArray* Other, uint32 PortFlags) const {
            if (this == Other) return true;
            if (!Other) return false;
            return Children == Other->Children;
        }
    };

    using FDaxMapType = ankerl::unordered_dense::map<FName, FDaxNodeID, ArzDax::DaxFNameHash, ArzDax::DaxFNameEqual, ArzDax::TDaxAllocator<std::pair<FName, FDaxNodeID>>>;

    struct FDaxMap {
        TUniquePtr<FDaxMapType> MapPtr = nullptr;

        FDaxMapType& Children() {
            checkf(MapPtr.IsValid(), TEXT("FDaxMap::Children: MapPtr is nullptr, WTF?"));
            return *MapPtr;
        }

        const FDaxMapType& Children() const {
            checkf(MapPtr.IsValid(), TEXT("FDaxMap::Children const: MapPtr is nullptr, WTF?"));
            return *MapPtr;
        }

        FDaxMap() {
            MapPtr = TUniquePtr<FDaxMapType>(new FDaxMapType());
        }

        FDaxMap(const FDaxMap& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap: copy: Other.MapPtr is nullptr, WTF?"));
            MapPtr = TUniquePtr<FDaxMapType>(new FDaxMapType(*Other.MapPtr));
        };

        FDaxMap(FDaxMap&& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap: move: Other.MapPtr is nullptr, WTF?"));
            MapPtr = MoveTemp(Other.MapPtr);
        };

        FDaxMap& operator=(const FDaxMap& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap::operator= copy: Other.MapPtr is nullptr, WTF?"));
            if (this != &Other) MapPtr = TUniquePtr<FDaxMapType>(new FDaxMapType(*Other.MapPtr));
            return *this;
        }

        FDaxMap& operator=(FDaxMap&& Other) {
            checkf(Other.MapPtr.IsValid(), TEXT("FDaxMap::operator= move: Other.MapPtr is nullptr, WTF?"));
            if (this != &Other) MapPtr = MoveTemp(Other.MapPtr);
            return *this;
        }

        bool Identical(const FDaxMap* Other, uint32 /*PortFlags*/) const {
            if (this == Other) return true;
            if (!Other) return false;
            checkf(MapPtr.IsValid() && Other->MapPtr.IsValid(), TEXT("FDaxMap::Identical: MapPtr null"));
            return Children() == Other->Children();
        }
    };

    struct FDaxNode {
        TVariant<FDaxEmpty, FDaxSmallValue, FDaxHeapValue, FDaxArray, FDaxMap> Value{};

        FDaxNode() { Value.Emplace<FDaxEmpty>(); }
        ~FDaxNode() = default;

        FDaxNode(FDaxNode&& Other) : Value(MoveTemp(Other.Value)) {
            Other.ResetToEmpty();
        }

        FDaxNode& operator=(FDaxNode&& Other) {
            if (this != &Other) {
                Value = MoveTemp(Other.Value);
                Other.ResetToEmpty();
            }
            return *this;
        }

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
                } else {
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
                } else {
                    return EDaxResult::SameValueNotChange;
                }
            }

            if (IsEmpty()) {
                Value.Emplace<FDaxArray>();
                return EDaxResult::SuccessOverrideEmpty;
            } else {
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
                } else {
                    return EDaxResult::SameValueNotChange;
                }
            }

            if (IsEmpty()) {
                Value.Emplace<FDaxMap>();
                return EDaxResult::SuccessOverrideEmpty;
            } else {
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

        FORCEINLINE const FDaxMapType* GetMap() const{
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
                } else {
                    Value.Emplace<FDaxHeapValue>(Target);
                }
                return EDaxResult::SuccessChangeValueAndType;
            } else {
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
                } else {
                    Type->SerializeBin(Ar, Temp.GetMutableMemory());
                }
                TrySetValue(Temp);
            } else if (Ar.IsSaving()) {
                if (!IsValue()) return false;
                auto SV = TryGetValueGenericMutable();
                auto SS = SV.GetScriptStruct();
                if (SS != Type) return false;
                if (SS->GetCppStructOps()->HasNetSerializer()) {
                    bool Success = true;
                    SS->GetCppStructOps()->NetSerialize(Ar, Map, Success, SV.GetMemory());
                } else {
                    SS->SerializeBin(Ar, SV.GetMemory());
                }
            }

            return true;
        }
    };
}