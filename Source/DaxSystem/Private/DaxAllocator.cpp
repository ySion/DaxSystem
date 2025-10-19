#include "DaxSystem/Private/DaxAllocator.h"

using namespace ArzDax;

FDaxNodeID FDaxAllocator::Allocate() {
    //分配满了就不分配了, 在正常使用中, 这个几乎是不可能的, 不存在这么大的NBT, 如果存在, 那么整个程序会陷入大麻烦.
    if (Stats.CurrentActive >= TotalCapacity - 1) {
        UE_LOGFMT(DataXSystem, Error, "DataXSystem Allocator pool is full, Stats.CurrentActive >= {0}, support range (0 ~ {1})", TotalCapacity - 1, TotalCapacity - 2);
        return FDaxNodeID();
    }
        
    uint16 ChunkIndex = SelectOrCreateChunkForBestAllocation();
    if (!Chunks.IsValidIndex(ChunkIndex)) {
        UE_LOGFMT(DataXSystem, Error, "DataXSystem Allocator AllocateSlot Failed to Find Free Chunk, Maybe Used in multi thread? This Container don't support thread");
        return FDaxNodeID();
    }
    FDaxNodeChunk* Chunk = Chunks[ChunkIndex].Get();
        
    auto AllocateResult = Chunks[ChunkIndex]->AllocateSlot();
    if (!AllocateResult.IsSet()) {
        UE_LOGFMT(DataXSystem, Error, "DataXSystem Allocator AllocateSlot Failed at Index Chunk {0}, Maybe Used in multi thread? This Container don't support thread", ChunkIndex);
        return FDaxNodeID();
    }
        
    uint16 LocalIndex = AllocateResult.GetValue();
    uint16 GlobalIndex = (ChunkIndex << DAX_NODE_POOR_CHUNK_SHIFT) | LocalIndex;

    Stats.TotalAllocated++;
    Stats.CurrentActive++;
    Stats.PeakActive = FMath::Max(Stats.PeakActive, static_cast<uint32>(Stats.CurrentActive));

    uint16 Generation = Chunk->Meta.Generations[LocalIndex];
    return FDaxNodeID(GlobalIndex, Generation);
}

FDaxAllocateResult FDaxAllocator::AllocateSlotAt(const FDaxNodeID SpecificNodeID) {
    if (!SpecificNodeID.IsValid()) return FDaxAllocateResult::Failed;

    const uint16 ChunkIndex = ToChunk(SpecificNodeID.Index);
    const uint16 LocalIndex = ToLocal(SpecificNodeID.Index);

    if (!EnsureChunkExists(ChunkIndex)) {
        UE_LOGFMT(DataXSystem, Error,
            "DataXSystem Allocator AllocateSlotAt EnsureChunkExists failed: ChunkIndex = {0}",
            ChunkIndex);
        return FDaxAllocateResult::Failed;
    }

    switch (Chunks[ChunkIndex]->AllocateSlotAt(LocalIndex, SpecificNodeID.Generation)) {
        case FDaxAllocateResult::NewOne:
            Stats.TotalAllocated++;
            Stats.CurrentActive++;
            Stats.PeakActive = FMath::Max(Stats.PeakActive, static_cast<uint32>(Stats.CurrentActive));
            return FDaxAllocateResult::NewOne;
        case FDaxAllocateResult::Replaced:
            Stats.TotalAllocated++;
            return FDaxAllocateResult::Replaced;
        case FDaxAllocateResult::Exist:
            return FDaxAllocateResult::Exist;
        case FDaxAllocateResult::Failed:
            return FDaxAllocateResult::Failed;
        default:
            return FDaxAllocateResult::Failed;
    }
}

bool FDaxAllocator::Deallocate(const FDaxNodeID ID) {
    if (!ID.IsValid()) return false;

    uint16 ChunkIndex = ID.Index >> DAX_NODE_POOR_CHUNK_SHIFT;
    uint16 LocalIndex = ID.Index & DAX_NODE_POOR_CHUNK_MASK;
        
    if (!Chunks.IsValidIndex(ChunkIndex)) return false;

    if (Chunks[ChunkIndex]->DeallocateSlot(LocalIndex, ID.Generation)) {
        Stats.TotalDeallocated++;
        Stats.CurrentActive--;
        return true;
    }
            
    return false;
}

uint16 FDaxAllocator::AllocateNewChunk() {
    uint16 NewIndex = Chunks.Num();
    if (NewIndex >= DAX_NODE_POOR_MAX_CHUNKS) {
        return 0xFFFF;
    }
    Chunks.Emplace(MakeUnique<FDaxNodeChunk>(NewIndex));
    return NewIndex;
}

uint16 FDaxAllocator::SelectOrCreateChunkForBestAllocation() { //直接找第一个有空位的块, 这里不做freelist, 对于低数量级的线性的遍历并不慢, 做了候选下标已经足够
    if (Chunks.IsValidIndex(Stats.FirstNotFullHintChunk)) {
        if (Chunks[Stats.FirstNotFullHintChunk]->HasFreeSlot()) {
            return Stats.FirstNotFullHintChunk;
        }
    }
            
    for (int32 i = 0; i < Chunks.Num(); ++i) { //如果预存没用了, 就再扫一波.
        FDaxNodeChunk* Chunk = Chunks[i].Get();
        if (Chunk && Chunk->HasFreeSlot()) {
            Stats.FirstNotFullHintChunk = i;
            return i;
        }
    }
            
    Stats.FirstNotFullHintChunk = AllocateNewChunk();
    return Stats.FirstNotFullHintChunk;
}