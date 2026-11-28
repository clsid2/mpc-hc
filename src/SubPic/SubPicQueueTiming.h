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

// The time decisions of the subpic queues, kept free of the queues themselves
// so they can be tested on their own. All times are in 100 ns units.
namespace SubPicQueueTiming
{
    // Events are timed to video frames, so a time this close to an edge of an
    // animated segment is taken to mean that edge.
    constexpr long long EDGE_TOLERANCE = 20000; // 2 ms

    // The time to render an animated subpic at, for a frame wanted at rt.
    // The stop time is exclusive, so the end snaps to the last tick before it.
    inline long long SnapToSegmentEdge(long long rt, long long rtSegmentStart, long long rtSegmentStop)
    {
        if (rt >= rtSegmentStart && rt - rtSegmentStart <= EDGE_TOLERANCE) {
            return rtSegmentStart;
        }
        if (rt < rtSegmentStop && rtSegmentStop - rt <= EDGE_TOLERANCE) {
            return rtSegmentStop - 1;
        }
        return rt;
    }

    // The first predicted frame time that is not before rt. Frames are predicted
    // at whole frame durations from rtFrame, the time of a frame the renderer
    // has shown. A frame time less than the tolerance before rt still counts,
    // and then rt itself is returned.
    inline long long AlignToFrameGrid(long long rt, long long rtFrame, long long rtTimePerFrame)
    {
        if (rtTimePerFrame <= 0) {
            return rt;
        }
        long long offset = rt - EDGE_TOLERANCE - rtFrame;
        long long frames = offset / rtTimePerFrame;
        if (offset > 0 && offset % rtTimePerFrame != 0) {
            frames++;
        }
        return std::max(rt, rtFrame + frames * rtTimePerFrame);
    }
}
