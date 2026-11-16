/*
 * (C) 2026 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <string>

// The trim interval [start, end) on the playback timeline, in 100 ns units.
// A source frame belongs to the clip when its display interval overlaps
// [start, end): the frame shown at `start` is the first one and the frame
// shown just before `end` is the last one.
class CTrimSelection
{
public:
    using Time = int64_t;

    enum class Part {
        None,
        Start,
        End,
        Playhead,
    };

    void Reset(Time duration)
    {
        m_duration = std::max<Time>(duration, 0);
        m_start = 0;
        m_end = m_duration;
        m_focus = Part::Playhead;
    }

    Time Start() const { return m_start; }
    Time End() const { return m_end; }
    Time Duration() const { return m_duration; }
    Time Length() const { return m_end - m_start; }
    bool IsExportable() const { return m_duration > 0 && m_start < m_end; }

    Time Boundary(Part part) const { return part == Part::End ? m_end : m_start; }

    // Boundaries never cross: each one is clamped by the other.
    bool SetStart(Time value)
    {
        const Time next = std::clamp<Time>(value, 0, m_end);
        const bool changed = next != m_start;
        m_start = next;
        return changed;
    }

    bool SetEnd(Time value)
    {
        const Time next = std::clamp<Time>(value, m_start, m_duration);
        const bool changed = next != m_end;
        m_end = next;
        return changed;
    }

    bool SetBoundary(Part part, Time value)
    {
        return part == Part::Start ? SetStart(value) : part == Part::End ? SetEnd(value) : false;
    }

    Part Focus() const { return m_focus; }
    void SetFocus(Part part) { m_focus = part == Part::None ? Part::Playhead : part; }

    Part CycleFocus(bool backwards)
    {
        static constexpr Part order[] = { Part::Playhead, Part::Start, Part::End };
        int index = 0;
        while (order[index] != m_focus) {
            ++index;
        }
        m_focus = order[(index + (backwards ? 2 : 1)) % 3];
        return m_focus;
    }

    // Boundary pixels are edges: 0 maps to `left` and the duration maps to `left + width`.
    static int TimeToPixel(Time value, Time duration, int left, int width)
    {
        if (duration <= 0 || width <= 0) {
            return left;
        }
        value = std::clamp<Time>(value, 0, duration);
        return left + static_cast<int>((value * width + duration / 2) / duration);
    }

    static Time PixelToTime(int x, Time duration, int left, int width)
    {
        if (duration <= 0 || width <= 0) {
            return 0;
        }
        x = std::clamp(x, left, left + width);
        return static_cast<Time>(x - left) * duration / width;
    }

    // The Start grip sits left of its edge and the End grip right of its edge, so when
    // the zones overlap the side of the pointer decides; a tie goes to the grip that
    // can still move away from the channel limit, then to the focused one.
    static Part HitTest(int x, int startX, int endX, int radius, int channelLeft, int channelRight, Part focus)
    {
        const bool hitsStart = std::abs(x - startX) <= radius;
        const bool hitsEnd = std::abs(x - endX) <= radius;
        if (hitsStart && hitsEnd) {
            const int middle2 = startX + endX;
            if (2 * x < middle2) {
                return Part::Start;
            }
            if (2 * x > middle2) {
                return Part::End;
            }
            if (endX <= channelLeft) {
                return Part::End;
            }
            if (startX >= channelRight) {
                return Part::Start;
            }
            return focus == Part::End ? Part::End : Part::Start;
        }
        return hitsStart ? Part::Start : hitsEnd ? Part::End : Part::None;
    }

    // "m:ss.mmm", or "h:mm:ss.mmm" from one hour on; truncated to the millisecond.
    static std::wstring FormatTime(Time value)
    {
        const bool negative = value < 0;
        const uint64_t ms = static_cast<uint64_t>(negative ? -value : value) / 10000;
        const unsigned hours = static_cast<unsigned>(ms / 3600000);
        const unsigned minutes = static_cast<unsigned>(ms / 60000 % 60);
        const unsigned seconds = static_cast<unsigned>(ms / 1000 % 60);
        const unsigned millis = static_cast<unsigned>(ms % 1000);
        wchar_t buffer[32];
        if (hours) {
            swprintf_s(buffer, L"%s%u:%02u:%02u.%03u", negative ? L"-" : L"", hours, minutes, seconds, millis);
        } else {
            swprintf_s(buffer, L"%s%u:%02u.%03u", negative ? L"-" : L"", minutes, seconds, millis);
        }
        return buffer;
    }

    static std::wstring FormatTimeSeconds(Time value)
    {
        const bool negative = value < 0;
        const uint64_t totalSeconds = static_cast<uint64_t>(negative ? -value : value) / 10000000;
        const unsigned hours = static_cast<unsigned>(totalSeconds / 3600);
        const unsigned minutes = static_cast<unsigned>(totalSeconds / 60 % 60);
        const unsigned seconds = static_cast<unsigned>(totalSeconds % 60);
        wchar_t buffer[32];
        if (hours) {
            swprintf_s(buffer, L"%s%u:%02u:%02u", negative ? L"-" : L"", hours, minutes, seconds);
        } else {
            swprintf_s(buffer, L"%s%u:%02u", negative ? L"-" : L"", minutes, seconds);
        }
        return buffer;
    }

    // A compact whole-second length for the timeline badge.
    static std::wstring FormatLength(Time value)
    {
        const uint64_t length = static_cast<uint64_t>(std::max<Time>(value, 0));
        const uint64_t seconds = length / 10000000;
        wchar_t buffer[32];
        if (seconds < 60) {
            swprintf_s(buffer, L"%llu s", seconds);
        } else if (seconds < 3600) {
            swprintf_s(buffer, L"%llu min %02llu s", seconds / 60, seconds % 60);
        } else {
            swprintf_s(buffer, L"%llu h %02llu min", seconds / 3600, seconds / 60 % 60);
        }
        return buffer;
    }

private:
    Time m_duration = 0;
    Time m_start = 0;
    Time m_end = 0;
    Part m_focus = Part::Playhead;
};
