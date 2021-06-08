// Part of SimCoupe - A SAM Coupe emulator
//
// Copyright 1999-2021 by Simon Owen <simon@simonowen.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum class EventType
{
    None,
    FrameInterrupt, FrameInterruptEnd,
    LineInterrupt, LineInterruptEnd,
    MidiOutStart, MidiOutEnd, MidiTxfmstEnd,
    MouseReset, BlueAlphaClock, TapeEdge,
    AsicReady, InputUpdate
};

struct CPU_EVENT
{
    EventType type{ EventType::None };
    uint32_t due_time{ 0 };
    CPU_EVENT* next_ptr{ nullptr };
};

constexpr auto MAX_EVENTS = 16;
extern CPU_EVENT events[MAX_EVENTS], * head_ptr, * free_head_ptr;

void InitEvents();
void AddEvent(EventType type, uint32_t due_time);
void CancelEvent(EventType type);
uint32_t GetEventTime(EventType type);
void EventFrameEnd(uint32_t elapsed_time);
void ExecuteEvent(const CPU_EVENT& event);

inline void CheckEvents(uint32_t frame_cycles)
{
    while (frame_cycles >= head_ptr->due_time)
    {
        auto event = *head_ptr;
        head_ptr->next_ptr = free_head_ptr;
        free_head_ptr = head_ptr;
        head_ptr = event.next_ptr;
        ExecuteEvent(event);
    }
}
