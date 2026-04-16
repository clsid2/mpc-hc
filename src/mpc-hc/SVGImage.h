/*
* (C) 2016-2017 see Authors.txt
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

namespace ATL
{
    class CImage;
}

namespace SVGImage
{
    enum IconTransform {
        TRANSFORM_NONE = 0,
        TRANSFORM_ROT90CW,
        TRANSFORM_ROT90CCW,
        TRANSFORM_FLIP_HORZ
    };
    struct IconDef {
        UINT nIDIcon;
        int svgTargetWidth = 0;
        IconTransform svgTransform = TRANSFORM_NONE;
    };

    struct icomp {
        bool operator() (const IconDef& c1, const IconDef& c2) const {
            return c1.nIDIcon < c2.nIDIcon ||
                (c1.nIDIcon == c2.nIDIcon && c1.svgTargetWidth < c2.svgTargetWidth) ||
                (c1.nIDIcon == c2.nIDIcon && c1.svgTargetWidth == c2.svgTargetWidth && c1.svgTransform < c2.svgTransform);
        }
    };

    HRESULT Load(LPCTSTR filename, ATL::CImage& image, float scale = 1.0f);
    HRESULT Load(UINT uResId, ATL::CImage& image, float scale = 1.0f);
    HRESULT LoadIconDef(IconDef iconDef, CImage& image);
}
