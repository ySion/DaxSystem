# DaxSystem 使用说明（基于 UDaxComponent + FDaxVisitor）

本说明书面向项目脚本（Angelscript）开发与引擎（C++/BluePrint）集成，围绕 Plugins/DaxSystem 的核心能力（FDaxSet/FDaxVisitor）与 Script/AScripts 的业务用法，给出系统概览、优缺点评估、标准使用流程，以及 Visitor 接口的详细说明与 Angelscript 短代码示例。

---

## 1. 系统概述

DaxSystem 是一个高性能、可复制（NetDelta）、支持层级（树形）结构的数据容器系统，适配 UE5：
- 数据以节点（Node）构成一棵树：空（Empty）/ 数组（Array）/ 映射（Map）/ 值（UScriptStruct）。
- 通过访问器（FDaxVisitor）以“路径”方式导航、读写与维护结构。
- 服务器负责结构（数组/映射）变更；客户端通过 Overlay 做值级覆盖并随网络同步更新。
- 提供 Angelscript 绑定，脚本侧可直接以 `UDaxComponent.Visitor` 驱动业务逻辑与 UI。

核心模块：
- `FDaxSet`：整棵数据树的拥有者，包含分配器、网络同步、变更监听等功能。
- `FDaxVisitor`：无侵入的“路径访问器”，提供导航、读写、容器操作以及调试输出。
- `UDaxComponent`：Actor 组件，内部持有 `FDaxSet`，对外暴露 `GetVisitor()` 与变更绑定接口，并支持复制。

---

## 2. 架构与运行时要点

- 结构类型
  - 空（Empty）/ 数组（Array）/ 映射（Map）/ 值（SmallValue/HeapValue）。
  - 值节点以 `UScriptStruct` 强类型存储；已内置基础类型包装（Bool/Int/Float/Name/String/Vector/Rotator）。
- 访问模型
  - `FDaxVisitor` 以路径段（FName 或 int32 索引）寻址：`g(Name)` / `g(Index)` / `gPath("Seg/$0/Key")` / `gParent()`。
  - 只读解析（ReadOnly）不会创建节点；`Ensure*` 解析则在需要时创建所需结构。
- 服务端/客户端职责
  - 结构变更（Array/Map 的新增、删除、插入、清空）仅允许在服务端进行。
  - 客户端值写入走 Overlay：仅当值实际变化时才创建 Overlay，避免无意义写入。
- 网络同步
  - 支持全量与增量（NetDelta）同步；维护 DataVersion/StructVersion 与分块镜像以提升同步效率。
- 变更监听
  - 基于 `BindOnChanged(Position, Depth, Delegate)` 按锚点与深度派发；支持 OldValue 快照查询。

---

## 3. 优缺点评估

优点：
- 统一、强类型的树形数据表示，易于组合 UI/玩法数据。
- 高性能：小型值内联、分块分配器、增量同步；仅差异传播。
- API 清晰：`g/gPath/Ensure*` 导航与构造语义直观；数组/映射操作对齐容器语义。
- 脚本友好：完整的 Angelscript 绑定；业务脚本可直接读写并绑定变更。
- 调试便捷：`GetString*()` 系列可快速输出结构/版本信息。

限制与注意：
- 结构操作仅服务端可写；客户端尝试修改结构将失败或被拒。
- 数组不支持稀疏创建与越界访问；`ArrayInsert/ArrayGet` 需合法索引。
- 路径深度上限（默认 64）；深层级频繁解析可能导致性能压力。
- Overlay 仅在“值变化”时创建；调试值一致时看不到 Overlay 节点。
- 自定义值类型必须有 `UScriptStruct` 且满足网络序列化（NetSerialize/SerializeBin）要求。

---

## 4. 标准使用流程（UDaxComponent 流）

以 Actor 上挂载的 `UDaxComponent` 为入口：

1) 获取访问器
```angelscript
auto DaxComp = UDaxComponent::Get(Owner); // Angelscript 侧常用获取方式
FDaxVisitor Root = DaxComp.Visitor;      // 根访问器
```

2) 构建/确保结构并写入（服务端）
```angelscript
if (Owner.HasAuthority()) {
    // 确保 Map 与 Array 结构存在
    auto HandSlot   = Root.g(n"HandSlot").EnsureMap();
    auto Inventory  = Root.g(n"Inventory").EnsureArray();

    // 扩容并填充
    for (int i = 0; i < 5; i++) {
        auto item = Inventory.ArrayAdd().EnsureMap();
        item.g("Name").EnsureAndSetName(n"Apple").edv();
        item.g("Count").EnsureAndSetInt(Math::Rand() % 100).edv();
    }
    DaxComp.MarkDirty(); // 推送网络复制
}
```

3) 客户端读取并绑定变更
```angelscript
FDaxVisitor QuickBarRoot = Root.g(n"QuickBar");
DaxComp.BindOnChanged(QuickBarRoot, /*Depth=*/1, this, n"OnQuickBarChanged");

UFUNCTION()
void OnQuickBarChanged(FDaxVisitor ChangedVisitor) {
    int index = ChangedVisitor.GetIndexInParentArray();
    // 刷新第 index 项 UI...
}
```

示例可参考：
- `AScripts/GameObjects/SmallCargo/RoSmallCargo.as`
- `AScripts/CharacterCapability/PlayerHub/RoCapPlayerHubSetter.as`
- `AScripts/CharacterCapability/PlayerHub/RoCapPlayerHubQuickBarView.as`
---

## 5. Visitor 使用详解（函数与示例）

以下方法均有 C++ 与 Angelscript 侧绑定，其中 Angelscript 采用相同或等价签名。示例以 Angelscript 展示。

### 5.1 基础状态与调试
- `IsValid()`：访问器是否绑定到有效 Set（含 LiveToken）。
- `HasData()`：路径可解析且节点存在。
- `IsAncestor(Other)`：是否为另一个访问器的严格祖先。
- `GetPathSegmentCount()`：路径段数量。
- `GetDepthRelativeTo(Ancestor)`：相对深度（祖先到自身的层数）。
- `GetCachedNodeID()` / `GetPathString()`：定位与可读路径。
- `GetString()` / `GetStringDebug()`：当前节点扼要打印（含类型/值/版本）。
- `GetStringDeep()` / `GetStringDebugDeep()`：递归打印子树（调试深度结构）。

```angelscript
Print(Root.GetString());
Print(Root.g(n"Inventory").GetStringDeep());
```

### 5.2 路径导航/构造
- `g(const FName)` / `g(const FString&)`：下钻命名段。
- `g(int Index)`：下钻数组索引段。
- `gParent()`：上移到父节点。
- `gPath(const FString&)`：以 `/` 分段、`$N` 表示索引的路径字符串解析（例如：`"Inventory/$0/id"`）。

```angelscript
auto Item0ID = Root.g(n"Inventory").g(0).g(n"id");
auto Same   = Root.gPath("Inventory/$0/id");
```

### 5.3 值读写（内置类型 + 通用结构）
通用（Struct）API：
- `TryGetValue(const UScriptStruct)` / `TrySetValue(AnyStruct)` / `EnsureAndSetValue(AnyStruct)`
- `TryGetOldValue(const UScriptStruct)`：结构变化场景下的旧值快照。

内置类型（已包装）：
- Bool：`TryGetBool()` / `TryGetOldBool()` / `TrySetBool(bool)` / `EnsureAndSetBool(bool)`
- Int：`TryGetInt()` / `TryGetOldInt()` / `TrySetInt(int64)` / `EnsureAndSetInt(int64)`
- Float/Name/String/Vector/Rotator 同理。

差异：
- `TrySet*` 要求目标路径已存在（不会创建）；
- `EnsureAndSet*` 若为空节点会创建并写入（适用于初始化场景）。

```angelscript
// 读取
auto optCount = Root.g(n"Inventory").g(0).g(n"Count").TryGetInt();
if (optCount.IsSet()) Print("Count=" + optCount.Value);

// 写入（仅值）：客户端也可覆盖到 Overlay（实际变更才创建 Overlay）
Root.g(n"HandSlot").g(n"Count").EnsureAndSetInt(1).edv();

// 旧值（结构变化时有效）
auto oldName = Root.g(n"HandSlot").g(n"id").TryGetOldName();
```

### 5.4 容器保障
- `EnsureArray()` / `EnsureMap()`：将空节点重置为对应容器；若已是该容器直接返回自身。

```angelscript
auto Grid = Root.g(n"Inventory").EnsureArray();
auto Slot = Root.g(n"HandSlot").EnsureMap();
```

### 5.5 数组操作（仅服务端允许结构变更）
- `ArrayAdd()`：在末尾新增元素并返回子访问器。
- `ArrayInsert(Index)`：在 `Index` 位置插入新元素。
- `ArrayGet(Index)` / `ArrayNum()` / `ArrayClear()` / `ArrayRemove()`
- `ArrayEnsureMinNum(Count)`：至少保证 `Count` 个元素（不缩减）。
- `ArrayGetChildren()`：返回一组子访问器快照。
- `ArrayGetPrev()` / `ArrayGetNext()`：在数组内邻接导航。

```angelscript
if (Owner.HasAuthority()) {
    auto arr = Root.g(n"QuickBar").EnsureArray();
    auto child = arr.ArrayInsert(0).EnsureMap();
    child.g(n"id").EnsureAndSetName(n"SomeItem");
}
```

### 5.6 映射操作（仅服务端允许结构变更）
- `MapAdd(Key)` / `MapGet(Key)` / `MapRemove(Key)` / `MapClear()` / `MapNum()`
- `MapGetChildren()`：返回一组 `Key`→子访问器的快照。

```angelscript
if (Owner.HasAuthority()) {
    auto meta = Root.g(n"Meta").EnsureMap();
    meta.MapAdd(n"Title").EnsureAndSetString("Hello");
}
```

### 5.7 位置与关系查询
- `GetIndexInParentArray()`：若位于父数组中，返回索引；否则 -1。
- `GetKeyInParentMap()`：若位于父映射中，返回键；否则 `NAME_None`。
- `GetIndexUnderAncestorArray(Ancestor)` / `GetKeyUnderAncestorMap(Ancestor)`：在祖先之下的第一段位置。

```angelscript
UFUNCTION()
void OnQuickBarChanged(FDaxVisitor ChangedVisitor) {
    int idx = ChangedVisitor.GetIndexInParentArray(); // 用于刷新 UI 某一格
}
```

### 5.8 搜索与清空
- `SearchEmptyChildNode()` / `SearchEmptyChildArrayNode()` / `SearchEmptyChildMapNode()`：在当前容器下寻找第一个“空”子。
- `TrySetToEmpty()`：将当前节点重置为空（客户端写入会落到 Overlay）。

```angelscript
auto emptySlot = Root.g(n"Inventory").SearchEmptyChildMapNode();
if (emptySlot.IsValid()) emptySlot.TrySetToEmpty();
```
---

## 6. Angelscript 常用片段（汇总）

### 6.1 服务器初始化数据
```angelscript
UFUNCTION(BlueprintOverride)
void BeginPlay() {
    if (!HasAuthority()) return;
    auto V = DaxSet.Visitor; // UDaxComponent 成员

    V.g(n"CargoOpenState").EnsureAndSetBool(true).edv();

    auto Inventory = V.g(n"Inventory").EnsureArray();
    for (int i = 0; i < 5; i++) {
        auto item = Inventory.ArrayAdd().EnsureMap();
        item.g("Name").EnsureAndSetName(n"Apple").edv();
        item.g("Count").EnsureAndSetInt(Math::Rand() % 100).edv();
    }
    DaxSet.MarkDirty(); // 触发复制
}
```

### 6.2 UI 侧读取与刷新
```angelscript
auto DaxComp = UDaxComponent::Get(Owner);
auto QuickBar = DaxComp.Visitor.g(n"QuickBar");
auto Items    = QuickBar.ArrayGetChildren();

for (int i = 0; i < Items.Num(); i++) {
    const auto& Slot = Items[i];
    if (Slot.IsANoEmptyMap()) {
        auto ItemID = Slot.g("id").TryGetName();
        if (ItemID.IsSet()) {
            // 刷 UI
        }
    }
}
```

### 6.4 路径字符串导航（`gPath`）
```angelscript
// `$N` 为数组索引片段标记
auto Name0 = DaxComp.Visitor.gPath("Inventory/$0/Name").TryGetName();
```

---

## 7. 常见问题与最佳实践

- 区分 `IsValid` 与 `HasData`
  - `IsValid()` 仅表示访问器绑定的 Set/LiveToken 有效；
  - `HasData()` 进一步保证路径可被解析（只读）且节点存在。
- `TrySet*` vs `EnsureAndSet*`
  - 初始化与“需要创建路径时”使用 `EnsureAndSet*`；
  - 已确定路径存在、仅改值时使用 `TrySet*`。
- 客户端写值与 Overlay
  - 客户端对值的写入会与底层权威值比较，若相同则不创建 Overlay；只有实际差异才创建 Overlay 记录。
- 结构更改权限
  - `ArrayAdd/Insert/Remove/Clear`、`MapAdd/Remove/Clear` 等仅在服务端有效；脚本中请先判断 `HasAuthority()`。
- 索引与路径边界
  - 数组不支持稀疏创建与越界访问；`g(Index)` 要求 Index 在合法范围。
  - 路径深度默认上限 64；尽量缓存中间访问器以减少重复解析。
- OldValue 的可用性
  - 旧值来自结构版本变化（如增量同步中的重定向/删除）；只有在发生结构调整后，`TryGetOld*` 才可能返回有效快照。
- 调试输出
  - `GetString*()` 系列可打印容器概览、节点类型、ID 与版本号，便于排查同步/结构问题。

---

## 8. 参考 API（按功能分组）

- 路径/导航
  - `g(Name) / g(String) / g(Index) / gParent() / gPath(String)`
- 状态/元信息
  - `IsValid / HasData / IsAncestor / GetPathSegmentCount / GetDepthRelativeTo / GetCachedNodeID / GetPathString`
- 值读写
  - 通用：`TryGetValue(UScriptStruct) / TrySetValue(AnyStruct) / EnsureAndSetValue(AnyStruct) / TryGetOldValue(UScriptStruct)`
  - 内置：`TryGetBool/Int/Float/Name/String/Vector/Rotator`、对应 `TryGetOld*`、`TrySet*`、`EnsureAndSet*`
- 容器保障
  - `EnsureArray / EnsureMap`
- 数组
  - `ArrayAdd / ArrayInsert / ArrayGet / ArrayNum / ArrayClear / ArrayRemove / ArrayEnsureMinNum / ArrayGetChildren / ArrayGetPrev / ArrayGetNext`
- 映射
  - `MapAdd / MapGet / MapRemove / MapNum / MapClear / MapGetChildren`
- 位置查询
  - `GetIndexInParentArray / GetKeyInParentMap / GetIndexUnderAncestorArray / GetKeyUnderAncestorMap`
- 搜索/清空
  - `SearchEmptyChildNode / SearchEmptyChildArrayNode / SearchEmptyChildMapNode / TrySetToEmpty`
- 调试
  - `GetString / GetStringDebug / GetStringDeep / GetStringDebugDeep`

---

## 9. 设计小结

- 以 `FDaxSet` 为中心、`FDaxVisitor` 为访问核心，统一承载网络玩法与 UI 绑定数据。
- 严格区分服务端与客户端职责，结构变更与值覆盖路径清晰；
- 通过 Angelscript 绑定直接驱动业务，示例清晰可复用；
- 建议在复杂 UI/系统中复用访问器与结果快照，降低路径解析与同步压力。
