#pragma once
#include "DaxSystem/Private/DaxNodeChunk.h"

#define DAX_NODE_POOR_CHUNK_SHIFT (5) // log2(32)
#define DAX_NODE_POOR_CHUNK_MASK (0x1F) // 31
#define DAX_NODE_POOR_MAX_CHUNKS (2048) // 32 * 2048 = 65536 总节点数

namespace ArzDax {
    struct FDaxAllocatorStats {
        uint32 TotalAllocated = 0;              // 用分配的累计次数
        uint32 TotalDeallocated = 0;            //调用释放的累计次数
        uint32 PeakActive = 0;                  //历史有效节点
        uint16 CurrentActive = 0;               //当前有效节点
        uint16 FirstNotFullHintChunk = 0xFFFF;
    };

    struct FDaxAllocator {
        static constexpr uint32 TotalCapacity = DAX_NODE_POOR_MAX_CHUNKS * (1u << DAX_NODE_POOR_CHUNK_SHIFT);

        TArray<TUniquePtr<FDaxNodeChunk>, TInlineAllocator<4>> Chunks{};
        FDaxAllocatorStats Stats{};

        FDaxAllocator(const FDaxAllocator&) = delete;
        FDaxAllocator& operator=(const FDaxAllocator&) = delete;

        FDaxAllocator(FDaxAllocator&& Other) {
            Chunks = MoveTemp(Other.Chunks);
            Stats = Other.Stats;
            Other.Stats = {};
        }

        FDaxAllocator& operator=(FDaxAllocator&& Other) {
            if (this == &Other) return *this;
            Chunks = MoveTemp(Other.Chunks);
            Stats = Other.Stats;
            Other.Stats = {};
            return *this;
        }

        FDaxAllocator() { AllocateNewChunk(); }
        ~FDaxAllocator() = default;

        FDaxNodeID Allocate();

        FDaxAllocateResult AllocateSlotAt(const FDaxNodeID SpecificNodeID);

        bool Deallocate(FDaxNodeID ID);

        FORCEINLINE void Reset() {
            Chunks.Empty();
            Stats = {};
        }

        FORCEINLINE const FDaxNodeChunkMeta* GetChunkMetadata(const uint16 ChunkIndex) const {
            if (Chunks.IsValidIndex(ChunkIndex)) {
                return &Chunks[ChunkIndex]->Meta;
            }
            return nullptr;
        }

        FORCEINLINE const FDaxNodeChunk* GetChunk(const uint16 ChunkIndex) const {
            if (Chunks.IsValidIndex(ChunkIndex)) {
                return Chunks[ChunkIndex].Get();
            }
            return nullptr;
        }

        FORCEINLINE const UScriptStruct* GetValueType(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return {};
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return {};
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return {};
            return Chunks[ChunkIndex]->UnsafeGetValueType(LocalIndex);
        }

        FORCEINLINE const UScriptStruct** GetValueTypeRef(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return {};
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return {};
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return {};
            return Chunks[ChunkIndex]->UnsafeGetValueTypeRef(LocalIndex);
        }

        FORCEINLINE bool IsNodeValid(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return false;
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return false;
            const uint16 LocalIndex = ToLocal(ID.Index);
            return Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation);
        }

        FORCEINLINE FDaxNode* TryGetNode(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return nullptr;
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return nullptr;
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return nullptr;
            return Chunks[ChunkIndex]->UnsafeGetNode(LocalIndex);
        }

        FORCEINLINE FDaxNodeCommonInfo GetCommonInfo(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return {};
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return {};
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return {};
            return Chunks[ChunkIndex]->UnsafeGetCommonInfo(LocalIndex);
        }

        FORCEINLINE FDaxNodeCommonInfoRef GetCommonInfoRef(const FDaxNodeID ID) {
            if (!ID.IsValid()) return {};
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return {};
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return {};
            return Chunks[ChunkIndex]->UnsafeGetCommonInfoRef(LocalIndex);
        }

        FORCEINLINE bool IsAncestor(const FDaxNodeID Ancestor, const FDaxNodeID Current) const {
            if (Ancestor == Current) return false;
            if (!IsNodeValid(Ancestor) || !IsNodeValid(Current)) return false;

            FDaxNodeID Temp = Current;
            do {
                Temp = GetParent(Temp);
                if (Temp == Ancestor) return true;
            } while (Temp.IsValid());

            return false;
        }

        FORCEINLINE FDaxNodeID GetParent(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return {};
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return {};
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return {};
            return Chunks[ChunkIndex]->UnsafeGetParent(LocalIndex);
        }

        FORCEINLINE FDaxNodeID* GetParentRef(const FDaxNodeID ID) const {
            if (!ID.IsValid()) return {};
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return {};
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return {};
            return Chunks[ChunkIndex]->UnsafeGetParentRef(LocalIndex);
        }

        // ===================== 反向映射（子->父容器边） =====================
        FORCEINLINE bool UpdateParentEdgeArray(const FDaxNodeID Child, uint16 Index) {
            if (!Child.IsValid()) return false;
            const uint16 ChunkIndex = ToChunk(Child.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return false;
            const uint16 LocalIndex = ToLocal(Child.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, Child.Generation)) return false;
            Chunks[ChunkIndex]->UnsafeSetParentEdgeArray(LocalIndex, Index);
            return true;
        }

        FORCEINLINE bool UpdateParentEdgeMap(const FDaxNodeID Child, FName Label) {
            if (!Child.IsValid()) return false;
            const uint16 ChunkIndex = ToChunk(Child.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return false;
            const uint16 LocalIndex = ToLocal(Child.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, Child.Generation)) return false;
            Chunks[ChunkIndex]->UnsafeSetParentEdgeMap(LocalIndex, Label);
            return true;
        }

        FORCEINLINE bool ClearParentEdge(const FDaxNodeID Child) {
            if (!Child.IsValid()) return false;
            const uint16 ChunkIndex = ToChunk(Child.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return false;
            const uint16 LocalIndex = ToLocal(Child.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, Child.Generation)) return false;
            Chunks[ChunkIndex]->UnsafeClearParentEdge(LocalIndex);
            return true;
        }

        FORCEINLINE ArzDax::EDaxParentEdgeKind GetParentEdgeKind(const FDaxNodeID Child) const {
            if (!Child.IsValid()) return ArzDax::EDaxParentEdgeKind::None;
            const uint16 ChunkIndex = ToChunk(Child.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return ArzDax::EDaxParentEdgeKind::None;
            const uint16 LocalIndex = ToLocal(Child.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, Child.Generation)) return ArzDax::EDaxParentEdgeKind::None;
            return Chunks[ChunkIndex]->UnsafeGetParentEdgeKind(LocalIndex);
        }

        FORCEINLINE uint16 GetParentEdgeIndex(const FDaxNodeID Child) const {
            if (!Child.IsValid()) return 0xFFFF;
            const uint16 ChunkIndex = ToChunk(Child.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return 0xFFFF;
            const uint16 LocalIndex = ToLocal(Child.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, Child.Generation)) return 0xFFFF;
            return Chunks[ChunkIndex]->UnsafeGetParentEdgeIndex(LocalIndex);
        }

        FORCEINLINE FName GetParentEdgeLabel(const FDaxNodeID Child) const {
            if (!Child.IsValid()) return NAME_None;
            const uint16 ChunkIndex = ToChunk(Child.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return NAME_None;
            const uint16 LocalIndex = ToLocal(Child.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, Child.Generation)) return NAME_None;
            return Chunks[ChunkIndex]->UnsafeGetParentEdgeLabel(LocalIndex);
        }

        FORCEINLINE bool MarkDirty(const FDaxNodeID ID, bool bBumpVersion) {
            if (!ID.IsValid()) return false;
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return false;
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return false;
            Chunks[ChunkIndex]->UnsafeMarkDirtyAndBump(LocalIndex, bBumpVersion);
            return true;
        }

        FORCEINLINE bool UpdateValueType(const FDaxNodeID ID, const UScriptStruct* NewType) {
            if (!IsValid(NewType)) return false;
            if (!ID.IsValid()) return false;
            const uint16 ChunkIndex = ToChunk(ID.Index);
            if (!Chunks.IsValidIndex(ChunkIndex)) return false;
            const uint16 LocalIndex = ToLocal(ID.Index);
            if (!Chunks[ChunkIndex]->IsNodeValid(LocalIndex, ID.Generation)) return false;
            Chunks[ChunkIndex]->UnsafeUpdateValueType(LocalIndex, NewType);
            return true;
        }

        FORCEINLINE uint32 GetTotalAllocated() const { return Stats.TotalAllocated; } //总共分配过多少次
        FORCEINLINE uint32 GetTotalDeallocated() const { return Stats.TotalDeallocated; } //总共删除过多少次
        FORCEINLINE uint32 GetCurrentActive() const { return Stats.CurrentActive; } // 当前数量
        FORCEINLINE uint32 GetPeakActive() const { return Stats.PeakActive; } // 最高使用
        FORCEINLINE uint32 GetChunkCount() const { return Chunks.Num(); } //当前块数量
        FORCEINLINE uint32 GetFreeRemaining() const { return (TotalCapacity - 1) - GetCurrentActive(); }

        template <typename Func>
        void ForEachNode(Func&& Function) {
            for (const auto& Chunk : Chunks) {
                if (Chunk->Meta.UsedCount == 0) continue;

                FDaxNode* Attributes = reinterpret_cast<FDaxNode*>(Chunk->NodeBuffer.Pad);
                uint64 Mask = Chunk->Meta.UsedMask;

                while (Mask) {
                    const uint16 LocalIndex = static_cast<uint16>(FMath::CountTrailingZeros(Mask));
                    const uint16 GlobalIndex = static_cast<uint16>((Chunk->Meta.ChunkIndex << DAX_NODE_POOR_CHUNK_SHIFT) | LocalIndex);
                    const FDaxNodeID ID(GlobalIndex, Chunk->Meta.Generations[LocalIndex]);

                    Function(ID, Attributes[LocalIndex], Chunk->Meta.Versions[LocalIndex], Chunk->Meta.Parent[LocalIndex], Chunk->Meta.ValueType[LocalIndex]);

                    Mask &= Mask - 1;
                }
            }
        }

    private:
        uint16 AllocateNewChunk();

        uint16 SelectOrCreateChunkForBestAllocation();

        static FORCEINLINE uint16 ToChunk(const uint16 GlobalIdx) {
            return GlobalIdx >> DAX_NODE_POOR_CHUNK_SHIFT;
        }

        static FORCEINLINE uint16 ToLocal(const uint16 GlobalIdx) {
            return GlobalIdx & DAX_NODE_POOR_CHUNK_MASK;
        }

        FORCEINLINE bool EnsureChunkExists(const uint16 ChunkIndex) {
            if (ChunkIndex >= DAX_NODE_POOR_MAX_CHUNKS) return false;
            while (Chunks.Num() <= ChunkIndex) {
                if (AllocateNewChunk() == 0xFFFF) return false;
            }
            return true;
        }
    };
}
