/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2021 see Authors.txt
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

#define CHARDET_OUT_OF_MEMORY -128
#define CHARDET_MEM_ALLOCATED_FAIL -127
#define CHARDET_SUCCESS     0
#define CHARDET_NO_RESULT   1
#define CHARDET_NULL_OBJECT 2

namespace charDet {
    typedef struct DetectObject {
        char * encoding;
        float confidence;
        short bom;
    } DetectObj;

    typedef short(*detect_r)(const char *, size_t, DetectObj **);
    typedef short(*detect) (const char *, DetectObj **);

    DetectObj* charDetect(const char* input, size_t len);
}

