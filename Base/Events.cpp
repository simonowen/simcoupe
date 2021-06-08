// Part of SimCoupe - A SAM Coupe emulator
// 
// Copyright 1999-2021 by Simon Owen <simon@simonowen.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SimCoupe.h"
#include "Events.h"

#include "BlueAlpha.h"
#include "CPU.h"
#include "Mouse.h"
#include "SAMIO.h"

CPU_EVENT events[MAX_EVENTS], * head_ptr, * free_head_ptr;

void InitEvents()
{
    for (int i = 0; i < MAX_EVENTS; ++i)
        events[i].next_ptr = &events[(i + 1) % MAX_EVENTS];

    free_head_ptr = events;
    head_ptr = nullptr;
}

void AddEvent(EventType type, uint32_t due_time)
{
    auto psNextFree = free_head_ptr->next_ptr;
    auto ppsEvent = &head_ptr;

    while (*ppsEvent && (*ppsEvent)->due_time <= due_time)
        ppsEvent = &((*ppsEvent)->next_ptr);

    free_head_ptr->type = type;
    free_head_ptr->due_time = due_time;

    free_head_ptr->next_ptr = *ppsEvent;
    *ppsEvent = free_head_ptr;
    free_head_ptr = psNextFree;
}

void CancelEvent(EventType type)
{
    auto event_ptr = &head_ptr;

    while (*event_ptr)
    {
        if ((*event_ptr)->type != type)
        {
            event_ptr = &((*event_ptr)->next_ptr);
        }
        else
        {
            auto next_ptr = (*event_ptr)->next_ptr;
            (*event_ptr)->next_ptr = free_head_ptr;
            free_head_ptr = *event_ptr;
            *event_ptr = next_ptr;
        }
    }
}

uint32_t GetEventTime(EventType type)
{
    for (auto event_ptr = head_ptr; event_ptr; event_ptr = event_ptr->next_ptr)
    {
        if (event_ptr->type == type)
            return event_ptr->due_time - CPU::frame_cycles;
    }

    return 0;
}

void EventFrameEnd(uint32_t elapsed_time)
{
    for (auto event_ptr = head_ptr; event_ptr; event_ptr = event_ptr->next_ptr)
        event_ptr->due_time -= elapsed_time;
}

void ExecuteEvent(const CPU_EVENT& event)
{
    switch (event.type)
    {
    case EventType::FrameInterrupt:
        IO::State().status &= ~STATUS_INT_FRAME;
        AddEvent(EventType::FrameInterruptEnd, event.due_time + CPU_CYCLES_INT_ACTIVE);
        AddEvent(EventType::FrameInterrupt, event.due_time + CPU_CYCLES_PER_FRAME);

        g_fBreak = true;
        break;

    case EventType::FrameInterruptEnd:
        IO::State().status |= STATUS_INT_FRAME;
        break;

    case EventType::LineInterrupt:
        IO::State().status &= ~STATUS_INT_LINE;
        AddEvent(EventType::LineInterruptEnd, event.due_time + CPU_CYCLES_INT_ACTIVE);
        AddEvent(EventType::LineInterrupt, event.due_time + CPU_CYCLES_PER_FRAME);
        break;

    case EventType::LineInterruptEnd:
        IO::State().status |= STATUS_INT_LINE;
        break;

    case EventType::MidiOutStart:
        IO::State().status &= ~STATUS_INT_MIDIOUT;
        AddEvent(EventType::MidiOutEnd, event.due_time + MIDI_INT_ACTIVE_TIME);
        AddEvent(EventType::MidiTxfmstEnd, event.due_time + MIDI_TXFMST_ACTIVE_TIME);
        break;

    case EventType::MidiOutEnd:
        IO::State().status |= STATUS_INT_MIDIOUT;
        break;

    case EventType::MidiTxfmstEnd:
        IO::State().lpen &= ~LPEN_TXFMST;
        break;

    case EventType::MouseReset:
        pMouse->Reset();
        break;

    case EventType::BlueAlphaClock:
        pSampler->Clock(event.due_time);
        break;

    case EventType::TapeEdge:
        Tape::NextEdge(event.due_time);
        break;

    case EventType::AsicReady:
        IO::State().asic_asleep = false;
        break;

    case EventType::InputUpdate:
        IO::UpdateInput();
        AddEvent(EventType::InputUpdate, event.due_time + CPU_CYCLES_PER_FRAME);
        break;

    case EventType::None:
        break;
    }
}
