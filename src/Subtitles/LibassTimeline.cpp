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

#include "stdafx.h"
#include "LibassTimeline.h"
#include "ass/ass.h"
#include <algorithm>
#include <cstring>

namespace
{
    // The override tags that set CSubtitle::m_bIsAnimated in the internal
    // renderer: \t, \move, \fad and \fade, and the karaoke tags \k, \K, \kf,
    // \ko and \kt. The internal renderer reads a tag name as everything up
    // to the next '(' or '\' and then takes the longest table entry it begins
    // with, so "\kf200" is \kf and "\k200" is \k. No other tag begins with
    // one of these names, so matching the prefix gives the same answer.
    const char* const animatedTags[] = { "t", "move", "fad", "fade", "k", "K", "kf", "ko", "kt" };

    bool IsAnimatedTagName(const char* name, size_t len)
    {
        for (const char* tag : animatedTags) {
            size_t tagLen = strlen(tag);
            if (len >= tagLen && strncmp(name, tag, tagLen) == 0) {
                return true;
            }
        }
        return false;
    }

    bool StartsWithNoCase(const char* s, const char* prefix)
    {
        return _strnicmp(s, prefix, strlen(prefix)) == 0;
    }
}

bool LibassTimeline::IsAnimatedEvent(const char* text, const char* effect)
{
    if (effect) {
        while (*effect == ' ' || *effect == '\t') {
            effect++;
        }
        // ParseEffect compares the text up to and including the first ';'
        if (StartsWithNoCase(effect, "Banner;") || StartsWithNoCase(effect, "Scroll up;") || StartsWithNoCase(effect, "Scroll down;")) {
            return true;
        }
    }

    if (!text) {
        return false;
    }

    // tags count only inside an override block; outside it a backslash is
    // text or a line break (\N, \n, \h)
    bool inBlock = false;
    for (const char* p = text; *p; p++) {
        if (*p == '{') {
            inBlock = true;
        } else if (*p == '}') {
            inBlock = false;
        } else if (inBlock && *p == '\\') {
            const char* name = p + 1;
            while (*name == ' ' || *name == '\t') {
                name++;
            }
            const char* end = name;
            while (*end && *end != '(' && *end != '\\' && *end != '}') {
                end++;
            }
            size_t len = end - name;
            while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t')) {
                len--;
            }
            if (IsAnimatedTagName(name, len)) {
                return true;
            }
            p = end - 1;
        }
    }
    return false;
}

void LibassTimeline::Clear()
{
    m_segments.clear();
    m_builtEvents = -1;
    m_builtGeneration = 0;
}

void LibassTimeline::Build(const LibassTimelineEvent* events, size_t count, int builtEvents, uint32_t generation)
{
    struct Edge {
        long long t;
        int visible;   // +1 at an event's start, -1 at its end
        int animated;  // the same, for events that animate
    };
    std::vector<Edge> edges;
    edges.reserve(count * 2);

    for (size_t i = 0; i < count; i++) {
        const LibassTimelineEvent& e = events[i];
        if (e.duration <= 0) {
            continue;  // libass never shows it: Start <= now < Start + Duration is empty
        }
        int animated = IsAnimatedEvent(e.text, e.effect) ? 1 : 0;
        edges.push_back({ e.start, 1, animated });
        edges.push_back({ e.start + e.duration, -1, -animated });
    }

    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) { return a.t < b.t; });

    m_segments.clear();
    int visible = 0, animated = 0;
    long long segmentStart = 0;
    size_t i = 0;
    while (i < edges.size()) {
        const long long t = edges[i].t;
        if (visible > 0) {
            m_segments.push_back({ segmentStart, t, animated > 0 });
        }
        // every edge at this time: an end that meets a start leaves one
        // boundary, not a gap or a doubled one
        while (i < edges.size() && edges[i].t == t) {
            visible += edges[i].visible;
            animated += edges[i].animated;
            i++;
        }
        segmentStart = t;
    }

    m_builtEvents = builtEvents;
    m_builtGeneration = generation;
}

void LibassTimeline::Build(const ass_track* track, uint32_t generation)
{
    std::vector<LibassTimelineEvent> events;
    if (track) {
        events.reserve(track->n_events);
        for (int i = 0; i < track->n_events; i++) {
            const ASS_Event& e = track->events[i];
            events.push_back({ e.Start, e.Duration, e.Text, e.Effect });
        }
    }
    Build(events.data(), events.size(), track ? track->n_events : 0, generation);
}

size_t LibassTimeline::FindAt(long long ms) const
{
    // segments are sorted and do not overlap, so their stops are sorted too
    auto it = std::partition_point(m_segments.begin(), m_segments.end(), [ms](const Segment& s) { return s.stop <= ms; });
    return it == m_segments.end() ? npos : (size_t)(it - m_segments.begin());
}
