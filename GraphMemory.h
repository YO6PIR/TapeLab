#pragma once

#include <Arduino.h>

constexpr uint8_t GRAPH_MEMORY_SLOT_COUNT = 8;
constexpr uint8_t GRAPH_MEMORY_EQ_POINT_COUNT = 16;
constexpr uint8_t GRAPH_MEMORY_DOLBY_POINT_COUNT = 9;

enum class GraphMemoryType : uint8_t
{
    Empty,
    TapeEq,
    Dolby
};

struct GraphMemorySlot
{
    GraphMemoryType type = GraphMemoryType::Empty;
    union
    {
        float tapeEq[3][GRAPH_MEMORY_EQ_POINT_COUNT];
        float dolby[3][GRAPH_MEMORY_DOLBY_POINT_COUNT];
    } points = {};
};

const GraphMemorySlot *graphMemoryGet(uint8_t index);
bool graphMemorySaveTapeEq(
    const float left[GRAPH_MEMORY_EQ_POINT_COUNT],
    const float right[GRAPH_MEMORY_EQ_POINT_COUNT],
    const float combined[GRAPH_MEMORY_EQ_POINT_COUNT]);
bool graphMemorySaveDolby(
    const float off[GRAPH_MEMORY_DOLBY_POINT_COUNT],
    const float dolbyB[GRAPH_MEMORY_DOLBY_POINT_COUNT],
    const float dolbyC[GRAPH_MEMORY_DOLBY_POINT_COUNT]);
