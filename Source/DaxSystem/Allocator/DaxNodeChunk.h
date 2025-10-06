#pragma once
#include "DaxSystem/Node/DaxNode.h"
#include "DaxSystem/Basic/DaxCommon.h"
#include "DaxSystem/Basic/DaxNodeID.h"

#define DAX_NODE_POOR_CHUNK_SIZE 32
#define DAX_NODE_POOR_BASE_NUMBER ((uint32)1)

namespace ArzDax {

    enum class EDaxNodeKind : uint8 { Empty=0, Value=1, Array=2, Map=3 };
    
    struct FDaxNodeChunkMeta {
        uint16 Generations[DAX_NODE_POOR_CHUNK_SIZE] {};
        uint32 Versions[DAX_NODE_POOR_CHUNK_SIZE] {};
        FDaxNodeID Parent[DAX_NODE_POOR_CHUNK_SIZE] {};
        const UScriptStruct* ValueType[DAX_NODE_POOR_CHUNK_SIZE] {};
        // 反向映射：子 -> 父容器中的边（数组下标或Map键）
        ArzDax::EDaxParentEdgeKind ParentEdgeKind[DAX_NODE_POOR_CHUNK_SIZE] {};
        uint16 ParentEdgeIndex[DAX_NODE_POOR_CHUNK_SIZE] {};
        FName ParentEdgeLabel[DAX_NODE_POOR_CHUNK_SIZE] {};
        uint32 UsedMask {};
        uint16 ChunkIndex {};
        uint8 UsedCount {};
    };

    struct FDaxNodeCommonInfo {
        uint32 Version {};
        FDaxNodeID Parent {};
        const UScriptStruct* ValueType = nullptr;
        
        bool IsValid() {
            return ValueType != nullptr;
        }
    };

    struct FDaxNodeCommonInfoRef {
        uint32* pVersion = nullptr;
        FDaxNodeID* pParent = nullptr;
        const UScriptStruct** pValueType = nullptr;
        
        bool IsValid() {
            return pValueType != nullptr;
        }
    };

    struct FDaxNodeChunk {
        FDaxNodeChunkMeta Meta {};
        TAlignedBytes<sizeof(FDaxNode) * DAX_NODE_POOR_CHUNK_SIZE, alignof(FDaxNode)> NodeBuffer {};
        
        FDaxNodeChunk() = delete; //必须要确定Index
        FDaxNodeChunk(const FDaxNodeChunk&) = delete;
        FDaxNodeChunk& operator=(const FDaxNodeChunk&) = delete;
        FDaxNodeChunk(FDaxNodeChunk&&) = delete;
        FDaxNodeChunk& operator=(FDaxNodeChunk&&) = delete;

        FDaxNodeChunk(uint16 Index) {
            Meta.ChunkIndex = Index;
        }

        ~FDaxNodeChunk() {
            if (Meta.UsedMask != 0) {
                FDaxNode* Node = reinterpret_cast<FDaxNode*>(NodeBuffer.Pad);
                for (uint32 i = 0; i < DAX_NODE_POOR_CHUNK_SIZE; ++i) {
                    if (Meta.UsedMask & (DAX_NODE_POOR_BASE_NUMBER << i)) {
                        Node[i].~FDaxNode();
                    }
                }
            }
        }

        FORCEINLINE bool HasFreeSlot() const {
            if (Meta.ChunkIndex == 2047) return Meta.UsedCount < DAX_NODE_POOR_CHUNK_SIZE - 1;
            return Meta.UsedCount < DAX_NODE_POOR_CHUNK_SIZE;
        }

        FORCEINLINE uint8 GetUsedCount() const {
            return Meta.UsedCount;
        }

        static FORCEINLINE bool IsInRange(uint16 LocalIndex) {
            return LocalIndex < DAX_NODE_POOR_CHUNK_SIZE;
        }
    
        FORCEINLINE bool IsUsed(uint16 LocalIndex) const {
            return Meta.UsedMask & (DAX_NODE_POOR_BASE_NUMBER << LocalIndex);
        }
    
        FORCEINLINE bool IsIndexValid(uint16 LocalIndex) const {
            return IsInRange(LocalIndex) && IsUsed(LocalIndex);
        }
        
        FORCEINLINE bool IsNodeValid(uint16 LocalIndex, uint16 Generation) const {
            return IsIndexValid(LocalIndex) && Generation == Meta.Generations[LocalIndex];
        }
        
        FORCEINLINE FDaxNode* UnsafeGetNode(uint16 LocalIndex) {
            return reinterpret_cast<FDaxNode*>(NodeBuffer.Pad) + LocalIndex;
        }
        
        FORCEINLINE FDaxNodeID UnsafeGetParent(uint16 LocalIndex) {
            return Meta.Parent[LocalIndex];
        }

        FORCEINLINE FDaxNodeID* UnsafeGetParentRef(uint16 LocalIndex) {
            return &Meta.Parent[LocalIndex];
        }

        FORCEINLINE const UScriptStruct* UnsafeGetValueType(uint16 LocalIndex) {
            return Meta.ValueType[LocalIndex];
        }

        FORCEINLINE const UScriptStruct** UnsafeGetValueTypeRef(uint16 LocalIndex) {
            return &Meta.ValueType[LocalIndex];
        }

        FORCEINLINE void UnsafeUpdateValueType(uint16 LocalIndex, const UScriptStruct* NewType) {
            Meta.ValueType[LocalIndex] = NewType;
        }

        // ---------- 反向映射：Unsafe 辅助 ----------
        FORCEINLINE void UnsafeClearParentEdge(uint16 LocalIndex) {
            Meta.ParentEdgeKind[LocalIndex] = ArzDax::EDaxParentEdgeKind::None;
            Meta.ParentEdgeIndex[LocalIndex] = 0xFFFF;
            Meta.ParentEdgeLabel[LocalIndex] = NAME_None;
        }

        FORCEINLINE void UnsafeSetParentEdgeArray(uint16 LocalIndex, uint16 Index) {
            Meta.ParentEdgeKind[LocalIndex] = ArzDax::EDaxParentEdgeKind::Array;
            Meta.ParentEdgeIndex[LocalIndex] = Index;
            Meta.ParentEdgeLabel[LocalIndex] = NAME_None;
        }

        FORCEINLINE void UnsafeSetParentEdgeMap(uint16 LocalIndex, FName Label) {
            Meta.ParentEdgeKind[LocalIndex] = ArzDax::EDaxParentEdgeKind::Map;
            Meta.ParentEdgeIndex[LocalIndex] = 0xFFFF;
            Meta.ParentEdgeLabel[LocalIndex] = Label;
        }

        FORCEINLINE ArzDax::EDaxParentEdgeKind UnsafeGetParentEdgeKind(uint16 LocalIndex) const {
            return Meta.ParentEdgeKind[LocalIndex];
        }

        FORCEINLINE uint16 UnsafeGetParentEdgeIndex(uint16 LocalIndex) const {
            return Meta.ParentEdgeIndex[LocalIndex];
        }

        FORCEINLINE FName UnsafeGetParentEdgeLabel(uint16 LocalIndex) const {
            return Meta.ParentEdgeLabel[LocalIndex];
        }

        FORCEINLINE FDaxNodeCommonInfo UnsafeGetCommonInfo(uint16 LocalIndex) {
            return {Meta.Versions[LocalIndex], Meta.Parent[LocalIndex], Meta.ValueType[LocalIndex] };
        }

        FORCEINLINE FDaxNodeCommonInfoRef UnsafeGetCommonInfoRef(uint16 LocalIndex) {
            return {&Meta.Versions[LocalIndex], &Meta.Parent[LocalIndex], &Meta.ValueType[LocalIndex] };
        }
        
        FORCEINLINE void UnsafeMarkDirtyAndBump(uint16 LocalIndex, bool bBumpVersion) {
            const uint32 Bit = DAX_NODE_POOR_BASE_NUMBER << LocalIndex;
            if (bBumpVersion) {
                ++Meta.Versions[LocalIndex];
                //Meta.DirtyMask |= Bit;
            }
            
            // if (bBumpSubtree) {
            //     ++Meta.SubtreeRev[LocalIndex];
            //     Meta.DirtySubtreeMask |= Bit;
            // }
        }

        TOptional<uint16> AllocateSlot() {
            uint32 FreeMask = ~Meta.UsedMask;
            if (FreeMask == 0) return {};
            
            uint16 LocalIndex = FMath::CountTrailingZeros(FreeMask);
            if (Meta.ChunkIndex == 2047 && LocalIndex == 31) return {};
            
            Meta.UsedMask |= (DAX_NODE_POOR_BASE_NUMBER << LocalIndex);
            Meta.UsedCount++;
            Meta.Generations[LocalIndex]++;
            Meta.Versions[LocalIndex]++;
            FDaxNode* Nodes = reinterpret_cast<FDaxNode*>(NodeBuffer.Pad);
            new(&Nodes[LocalIndex]) FDaxNode();
            // 清空反向映射
            UnsafeClearParentEdge(LocalIndex);
        
            return LocalIndex;
        }

        // 确定性分配, 指定index和generation, 给网络同步使用
        FDaxAllocateResult AllocateSlotAt(uint16 LocalIndex, uint16 ExpectedGeneration) {
            if (!IsInRange(LocalIndex)) return FDaxAllocateResult::Failed;
            if (IsUsed(LocalIndex)) {
                if (Meta.Generations[LocalIndex] == ExpectedGeneration) return FDaxAllocateResult::Exist;
                FDaxNode* Nodes = reinterpret_cast<FDaxNode*>(NodeBuffer.Pad);
                Nodes[LocalIndex].~FDaxNode();
                Meta.Generations[LocalIndex] = ExpectedGeneration;
                new(&Nodes[LocalIndex]) FDaxNode();
                UnsafeClearParentEdge(LocalIndex);
                return FDaxAllocateResult::Replaced;
            } else {
                Meta.UsedMask |= (DAX_NODE_POOR_BASE_NUMBER << LocalIndex);
                Meta.UsedCount++;
                Meta.Generations[LocalIndex] = ExpectedGeneration;
                FDaxNode* Nodes = reinterpret_cast<FDaxNode*>(NodeBuffer.Pad);
                new(&Nodes[LocalIndex]) FDaxNode();
                UnsafeClearParentEdge(LocalIndex);
                return FDaxAllocateResult::NewOne;
            }
        }

        bool DeallocateSlot(uint16 LocalIndex, uint16 ExpectedGeneration) {
            if (!IsIndexValid(LocalIndex)) return false;
            if (Meta.Generations[LocalIndex] != ExpectedGeneration) return false;

            FDaxNode* Nodes = reinterpret_cast<FDaxNode*>(NodeBuffer.Pad);
            Nodes[LocalIndex].~FDaxNode();

            Meta.Parent[LocalIndex] = FDaxNodeID();
            Meta.ValueType[LocalIndex] = nullptr;
            UnsafeClearParentEdge(LocalIndex);
            
            Meta.UsedMask &= ~(DAX_NODE_POOR_BASE_NUMBER << LocalIndex);
            Meta.UsedCount--;
            return true;
        }
    };
}
