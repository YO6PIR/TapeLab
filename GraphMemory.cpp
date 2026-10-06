#include "GraphMemory.h"

#include <string.h>

namespace
{
    GraphMemorySlot slots[GRAPH_MEMORY_SLOT_COUNT] = {};

    GraphMemorySlot *firstFreeSlot()
    {
        for (uint8_t i = 0; i < GRAPH_MEMORY_SLOT_COUNT; ++i)
            if (slots[i].type == GraphMemoryType::Empty)
                return &slots[i];
        return nullptr;
    }
}

const GraphMemorySlot *graphMemoryGet(uint8_t index)
{
    return index < GRAPH_MEMORY_SLOT_COUNT ? &slots[index] : nullptr;
}

bool graphMemorySaveTapeEq(
    const float left[GRAPH_MEMORY_EQ_POINT_COUNT],
    const float right[GRAPH_MEMORY_EQ_POINT_COUNT],
    const float combined[GRAPH_MEMORY_EQ_POINT_COUNT])
{
    GraphMemorySlot *slot = firstFreeSlot();
    if (slot == nullptr || left == nullptr || right == nullptr || combined == nullptr)
        return false;

    memcpy(slot->points.tapeEq[0], left, sizeof(slot->points.tapeEq[0]));
    memcpy(slot->points.tapeEq[1], right, sizeof(slot->points.tapeEq[1]));
    memcpy(slot->points.tapeEq[2], combined, sizeof(slot->points.tapeEq[2]));
    slot->type = GraphMemoryType::TapeEq;
    return true;
}

bool graphMemorySaveDolby(
    const float off[GRAPH_MEMORY_DOLBY_POINT_COUNT],
    const float dolbyB[GRAPH_MEMORY_DOLBY_POINT_COUNT],
    const float dolbyC[GRAPH_MEMORY_DOLBY_POINT_COUNT])
{
    GraphMemorySlot *slot = firstFreeSlot();
    if (slot == nullptr || off == nullptr || dolbyB == nullptr || dolbyC == nullptr)
        return false;

    memcpy(slot->points.dolby[0], off, sizeof(slot->points.dolby[0]));
    memcpy(slot->points.dolby[1], dolbyB, sizeof(slot->points.dolby[1]));
    memcpy(slot->points.dolby[2], dolbyC, sizeof(slot->points.dolby[2]));
    slot->type = GraphMemoryType::Dolby;
    return true;
}
