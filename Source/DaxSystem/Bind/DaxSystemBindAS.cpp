#include "DaxSystemBindAS.h"
#include "DaxSystem/Core/DaxSet.h"

#define DAX_WITH_ANGELSCRIPT
#ifdef  DAX_WITH_ANGELSCRIPT

#include "AngelscriptAnyStructParameter.h"
#include "AngelscriptBinds.h"

struct FAngelscriptAnyStructParameter;
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FDaxStructHandle(FAngelscriptBinds::EOrder::Late, [] {
    {
        auto FDaxResultDetail_ = FAngelscriptBinds::ExistingClass("FDaxResultDetail");
        
        FBindFlags FDaxStructHandleFlags;
        FDaxStructHandleFlags.bTemplate = true;
        FDaxStructHandleFlags.TemplateType = "<T>";
        FDaxStructHandleFlags.ExtraFlags |= asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
        FDaxStructHandleFlags.ExtraFlags |= asOBJ_TEMPLATE_INHERIT_SPECIALIZATIONS;

        auto FDaxStructHandle_ = FAngelscriptBinds::ValueClass("FDaxStructView<T>", sizeof(FDaxStructView), FDaxStructHandleFlags);

        FDaxStructHandle_.Constructor("void f()", &FDaxStructView::ConstructHandle);
        FDaxStructHandle_.Constructor("void f(const FDaxStructView<T>& Other)", &FDaxStructView::CopyConstructHandle);
        FDaxStructHandle_.Method("FDaxStructView<T>& opAssign(const FDaxStructView<T>& Other)", &FDaxStructView::AssignHandle);

        FDaxStructHandle_.Method("bool GetIsValid() const", [](const FDaxStructView* Handle)-> bool {
            return Handle->Data.IsValid();
        });

        FDaxStructHandle_.Method("const T& GetV() const", [](const FDaxStructView* Handle) {
            return Handle->Data.GetMemory();
        });
    }
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FDaxResultDetail(FAngelscriptBinds::EOrder::Late, [] {
    {
        auto FDaxResultDetail_ = FAngelscriptBinds::ExistingClass("FDaxResultDetail");

        FDaxResultDetail_.Method("FString GetResultString() const", &FDaxResultDetail::GetResultString);
        FDaxResultDetail_.Method("bool IsSuccess() const", &FDaxResultDetail::IsSuccess);
        FDaxResultDetail_.Method("bool IsOk() const", &FDaxResultDetail::IsOk);
        FDaxResultDetail_.Method("void ed() const", static_cast<void(FDaxResultDetail::*)() const>(&FDaxResultDetail::ed));
        FDaxResultDetail_.Method("void ed(const FString& Content) const", static_cast<void(FDaxResultDetail::*)(const FString&) const>(&FDaxResultDetail::ed));
        FDaxResultDetail_.Method("void edv() const", &FDaxResultDetail::edvas);
    }
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FDaxSet(
    static_cast<FAngelscriptBinds::EOrder>(static_cast<uint32>(FAngelscriptBinds::EOrder::Late) + 1), [] {
    {
        auto FDaxSet_ = FAngelscriptBinds::ExistingClass("FDaxSet");
        FDaxSet_.Method("FDaxVisitor GetVisitor() const", &FDaxSet::GetVisitor);
        FDaxSet_.Method("FDaxVisitor GetVisitorFromPath(const FString& str) const", &FDaxSet::GetVisitorFromPath);
        FDaxSet_.Method("int32 GetNodeNum() const", &FDaxSet::GetNodeNum);
        FDaxSet_.Method("FString GetString() const", &FDaxSet::GetString);
        FDaxSet_.Method("FString GetStringDebug() const", &FDaxSet::GetStringDebug);
        // 下线：FDaxSet 的监听相关 API 由 Subsystem 统一管理，不再在 AS 暴露
    }
});

#define DAX_BUILTIN_TYPE_BIND_AS(Ret, Param, Func) \
        FDaxVisitor_.Method("TOptional<"#Ret"> TryGet"#Func"() const",&FDaxVisitor::TryGet##Func); \
        FDaxVisitor_.Method("TOptional<"#Ret"> TryGetOld"#Func"() const",&FDaxVisitor::TryGetOld##Func); \
        FDaxVisitor_.Method("FDaxResultDetail TrySet"#Func"("#Param") const allow_discard",&FDaxVisitor::TrySet##Func); \
        FDaxVisitor_.Method("FDaxResultDetail EnsureAndSet"#Func"("#Param") const allow_discard",&FDaxVisitor::EnsureAndSet##Func); \
        

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FDaxVisitor(
    static_cast<FAngelscriptBinds::EOrder>(static_cast<uint32>(FAngelscriptBinds::EOrder::Late) + 2), [] {
    {
        auto FDaxVisitor_ = FAngelscriptBinds::ExistingClass("FDaxVisitor");
        FDaxVisitor_.Method("FDaxVisitor g(const FName Name) const", &FDaxVisitor::MakeVisitorByName);
        FDaxVisitor_.Method("FDaxVisitor g(const FString& sName) const", &FDaxVisitor::MakeVisitorByString);
        FDaxVisitor_.Method("FDaxVisitor gPath(const FString& Path) const", &FDaxVisitor::MakeVisitorByParsePath);
        FDaxVisitor_.Method("FDaxVisitor g(int __any_implicit_integer Index) const", &FDaxVisitor::MakeVisitorByIndex);
        FDaxVisitor_.Method("FDaxVisitor gParent() const", &FDaxVisitor::MakeVisitorToParent);
        
        FDaxVisitor_.Method("FDaxVisitor SearchEmptyChildNode() const", &FDaxVisitor::SearchEmptyChildNode);
        FDaxVisitor_.Method("FDaxVisitor SearchEmptyChildArrayNode() const", &FDaxVisitor::SearchEmptyChildArrayNode);
        FDaxVisitor_.Method("FDaxVisitor SearchEmptyChildMapNode() const", &FDaxVisitor::SearchEmptyChildMapNode);
        
        FDaxVisitor_.Method("FDaxResultDetail TrySetToEmpty() const", &FDaxVisitor::TrySetToEmpty);

        FDaxVisitor_.Method("FDaxResultDetail EnsureAndCopyFrom(const FDaxVisitor& Src) const", &FDaxVisitor::EnsureAndCopyFrom);
        FDaxVisitor_.Method("FDaxResultDetail EnsureAndSwapWith(const FDaxVisitor& Src) const", &FDaxVisitor::EnsureAndSwapWith);
        

        DAX_BUILTIN_TYPE_BIND_AS(bool, bool Value, Bool);
        DAX_BUILTIN_TYPE_BIND_AS(int64, int64 __any_implicit_integer Value, Int);
        DAX_BUILTIN_TYPE_BIND_AS(float32, float32 Value, Float);
        DAX_BUILTIN_TYPE_BIND_AS(FName, const FName& Value, Name);
        DAX_BUILTIN_TYPE_BIND_AS(FString, const FString& Value, String);
        DAX_BUILTIN_TYPE_BIND_AS(FVector, const FVector& Value, Vector);
        DAX_BUILTIN_TYPE_BIND_AS(FRotator, const FRotator& Value, Rotator);
        
        FDaxVisitor_.Method("FDaxStructView<FScriptStructWildcard> TryGetValue(const UScriptStruct ValueType) const",
                            [](const FDaxVisitor& Visitor, const UScriptStruct* TargetStruct) {
                                return FDaxStructView{Visitor.TryGetValue(TargetStruct)};
                            });
        FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

        FDaxVisitor_.Method("FDaxStructView<FScriptStructWildcard> TryGetOldValue(const UScriptStruct ValueType) const",
                            [](const FDaxVisitor& Visitor, const UScriptStruct* TargetStruct) {
                                return FDaxStructView{Visitor.TryGetOldValue(TargetStruct)};
                            });
        FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

        FDaxVisitor_.Method("FDaxResultDetail TrySetValue(const FAngelscriptAnyStructParameter& Value) const allow_discard",
                            [](const FDaxVisitor& Visitor, const FAngelscriptAnyStructParameter& TargetStruct) {
                                return Visitor.TrySetValue(FConstStructView(TargetStruct.InstancedStruct));
                            });
        
        FDaxVisitor_.Method("FDaxResultDetail EnsureAndSetValue(const FAngelscriptAnyStructParameter& Value) const allow_discard",
                            [](const FDaxVisitor& Visitor, const FAngelscriptAnyStructParameter& TargetStruct) {
                                return Visitor.EnsureAndSetValue(FConstStructView(TargetStruct.InstancedStruct));
                            });
        
        FDaxVisitor_.Method("FDaxVisitor EnsureArray() const allow_discard", &FDaxVisitor::EnsureArray);
        FDaxVisitor_.Method("FDaxVisitor EnsureMap() const allow_discard", &FDaxVisitor::EnsureMap);
        
        FDaxVisitor_.Method("FDaxVisitor ArrayAdd() const", &FDaxVisitor::ArrayAdd);
        FDaxVisitor_.Method("bool ArrayRemove() const allow_discard", &FDaxVisitor::ArrayRemove);
        FDaxVisitor_.Method("FDaxVisitor ArrayGet(int32 Index) const", &FDaxVisitor::ArrayGet);
        FDaxVisitor_.Method("FDaxVisitor ArrayInsert(int32 Index) const", &FDaxVisitor::ArrayInsert);
        FDaxVisitor_.Method("FDaxResultDetail ArrayEnsureMinNum(int32 Count) const allow_discard", &FDaxVisitor::ArrayEnsureMinNum);
        FDaxVisitor_.Method("int32 ArrayNum() const", &FDaxVisitor::ArrayNum);
        FDaxVisitor_.Method("bool ArrayClear() const allow_discard", &FDaxVisitor::ArrayClear);
        FDaxVisitor_.Method("TArray<FDaxVisitor> ArrayGetChildren() const", &FDaxVisitor::ArrayGetChildren);
        
        FDaxVisitor_.Method("FDaxVisitor MapAdd(FName Key) const", &FDaxVisitor::MapAdd);
        FDaxVisitor_.Method("bool MapRemove(FName Key) const allow_discard", &FDaxVisitor::MapRemove);
        FDaxVisitor_.Method("FDaxVisitor MapGet(FName Key) const", &FDaxVisitor::MapGet);
        FDaxVisitor_.Method("int32 MapNum() const", &FDaxVisitor::MapNum);
        FDaxVisitor_.Method("bool MapClear() const allow_discard", &FDaxVisitor::MapClear);
        FDaxVisitor_.Method("TArray<FDaxVisitor> MapGetChildren() const", &FDaxVisitor::MapGetChildren);

        // 额外：基础状态与关系
        FDaxVisitor_.Method("bool IsValid() const", &FDaxVisitor::IsValid);
        FDaxVisitor_.Method("bool HasData() const", &FDaxVisitor::HasData);
        FDaxVisitor_.Method("bool IsAncestor(const FDaxVisitor& Other) const", &FDaxVisitor::IsAncestor);

        FDaxVisitor_.Method("bool IsAEmptyMap() const", &FDaxVisitor::IsAEmptyMap); 
        FDaxVisitor_.Method("bool IsAEmptyArray() const", &FDaxVisitor::IsAEmptyArray); 
        FDaxVisitor_.Method("bool IsAEmptyNode() const", &FDaxVisitor::IsAEmptyNode); 
        FDaxVisitor_.Method("bool IsANoEmptyMap() const", &FDaxVisitor::IsANoEmptyMap); 
        FDaxVisitor_.Method("bool IsANoEmptyArray() const", &FDaxVisitor::IsANoEmptyArray); 

        // 额外：数组兄弟导航
        FDaxVisitor_.Method("FDaxVisitor ArrayGetPrev() const", &FDaxVisitor::ArrayGetPrev);
        FDaxVisitor_.Method("FDaxVisitor ArrayGetNext() const", &FDaxVisitor::ArrayGetNext);

        // 额外：容器位置查询（无效返回 -1/NAME_None）
        FDaxVisitor_.Method("int32 GetIndexInParentArray() const", &FDaxVisitor::GetIndexInParentArray);
        FDaxVisitor_.Method("FName GetKeyInParentMap() const", &FDaxVisitor::GetKeyInParentMap);
        FDaxVisitor_.Method("int32 GetIndexUnderAncestorArray(const FDaxVisitor& Ancestor) const", &FDaxVisitor::GetIndexUnderAncestorArray);
        FDaxVisitor_.Method("FName GetKeyUnderAncestorMap(const FDaxVisitor& Ancestor) const", &FDaxVisitor::GetKeyUnderAncestorMap);
        
        FDaxVisitor_.Method("FString GetString() const", &FDaxVisitor::GetString);
        FDaxVisitor_.Method("FString GetStringDebug() const", &FDaxVisitor::GetStringDebug);
        // 深度打印
        FDaxVisitor_.Method("FString GetStringDeep() const", &FDaxVisitor::GetStringDeep);
        FDaxVisitor_.Method("FString GetStringDebugDeep() const", &FDaxVisitor::GetStringDebugDeep);
        FDaxVisitor_.Method("FString GetPathString() const", &FDaxVisitor::GetPathString);
    }
});

#endif
