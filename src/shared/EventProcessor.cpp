/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "EventProcessor.h"

bool EventsSortOrder(const BasicEventPtr event1, const BasicEventPtr event2)
{
    return event1->m_execTime < event2->m_execTime;
}

EventProcessor::EventProcessor()
{
    m_time = 0;
    m_aborting = false;
}

EventProcessor::~EventProcessor()
{
    KillAllEvents(true);
}

void EventProcessor::Update(uint32 p_time)
{
    if (m_aborting)
        return;

    // update time
    m_time += p_time;

    // main event loop
    while (!m_aborting && !m_events.empty() && m_events.back() && m_events.back()->m_execTime <= m_time)
    {
        BasicEventPtr Event = m_events.back();
        bool needDelete = true;
        if (Event)
        {
            if (!Event->to_Abort)
            {
                if (!Event->Execute(m_time, p_time))
                {
                    // re-adding not completed events to queue (changing time and sort)
                    // event execute time may be already changed in ::Execute() method
                    if (Event->m_execTime <= m_time)
                        Event->ModExecTime(m_time + 1);
                    if (m_events.size() > 1)
                        m_events.sort(EventsSortOrder);
                    needDelete = false;
                }
                else
                {
                    // if event not last in queue after operations, his be dropped (aborted) in next update cycle
                    Event->to_Abort = true;
                }
            }
            else
            {
                Event->Abort(m_time);
            }
        }

        if (needDelete)
        {
            if (!m_events.empty())
            {
                if (m_events.back() == Event)
                    m_events.pop_back();
                else
                    m_events.erase(Event);
            }
        }
    }
}

void EventProcessor::KillAllEvents(bool force)
{
    if (m_aborting)
        return;

    // prevent event insertions
    m_aborting = true;

    // first, abort all existing events
    while (!m_events.empty())
    {
        BasicEventPtr Event = m_events.back();
        m_events.pop_back();
        if (Event)
        {
            Event->to_Abort = true;
            Event->Abort(m_time);
        }
    }
}

void EventProcessor::AddEvent(BasicEvent* _event, uint64 e_time, bool set_addtime)
{
    if (m_aborting || !_event)
        return;

    BasicEventPtr Event = BasicEventPtr(_event);

    if (!Event)
    {
        sLog.outError("EventProcessor::AddEvent cannot add event to event processor by unknown reason.");
        return;
    }

    if (set_addtime)
        Event->m_addTime = m_time;

    Event->m_execTime = e_time;

    m_events.push_back(Event);

    // always sort events after adding new! even not be executed always.
    if (m_events.size() > 1)
        m_events.sort(EventsSortOrder);
}

uint64 EventProcessor::CalculateTime(uint64 t_offset)
{
    return m_time + t_offset;
}
