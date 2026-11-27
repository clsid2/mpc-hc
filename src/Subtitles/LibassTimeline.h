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

#include <cstddef>
#include <cstdint>
#include <vector>

struct ass_track;

// One event as the timeline needs it: the ASS_Event fields it reads, and
// nothing from libass, so the builder can be tested without an ASS_Track.
struct LibassTimelineEvent {
    long long start;     // ms, ASS_Event::Start
    long long duration;  // ms, ASS_Event::Duration
    const char* text;    // ASS_Event::Text, may be null
    const char* effect;  // ASS_Event::Effect, may be null
    int readOrder = 0;   // ASS_Event::ReadOrder
};

// The segments of a libass track: the maximal time ranges over which the set
// of visible events does not change. Built from event start and end times in
// ms, which is what libass compares against (Start <= now < Start + Duration),
// so a segment boundary in ms times 10000 is exact in reference time.
//
// Only visible segments are kept. A gap between two lines is the space
// between two segments, and a lookup there answers the next segment, so the
// subpic queue can render it ahead or wait.
class LibassTimeline
{
public:
    static const size_t npos = (size_t)-1;

    struct Segment {
        long long start;  // ms, inclusive
        long long stop;   // ms, exclusive
        bool animated;    // some visible event animates over time
    };

    // True when the event's text has an override tag that changes the picture
    // over time, or its effect is a banner or a scroll. The tag set and the
    // way a tag name is read match the internal renderer's parse (RTS.cpp
    // ParseSSATag and CreateSubFromSSATag), which is what sets
    // CSubtitle::m_bIsAnimated there.
    static bool IsAnimatedEvent(const char* text, const char* effect);

    void Build(const LibassTimelineEvent* events, size_t count, int builtEvents, uint32_t generation);
    void Build(const ass_track* track, uint32_t generation);
    void Clear();

    // The build is stale when the track has a different number of events than
    // it was built from, or the generation moved (load, flush, reload).
    bool NeedsRebuild(int nEvents, uint32_t generation) const {
        return m_builtEvents != nEvents || m_builtGeneration != generation;
    }

    size_t Count() const { return m_segments.size(); }
    const Segment& At(size_t i) const { return m_segments[i]; }

    // The first segment that is still to be shown at ms: the one containing
    // ms, or the next one after it. npos when there is none.
    size_t FindAt(long long ms) const;

    // The events a track holds, as the timeline reads them.
    static std::vector<LibassTimelineEvent> EventsOf(const ass_track* track);

    // Indices of the events libass shows at ms (Start <= ms < Start +
    // Duration), in ReadOrder, which is the order they appear in the file.
    static std::vector<size_t> VisibleAt(const LibassTimelineEvent* events, size_t count, long long ms);

private:
    std::vector<Segment> m_segments;
    int m_builtEvents = -1;
    uint32_t m_builtGeneration = 0;
};
