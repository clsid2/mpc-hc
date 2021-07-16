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

#include "stdafx.h"
#include "chardet.h"

charDet::DetectObj* charDet::charDetect(const char* input, size_t len) {
    HMODULE lib = NULL;
    lib = LoadLibrary(_T("libchardet.dll"));
    if (lib == NULL) return NULL;
    detect_r pdetect_r;
    pdetect_r = (detect_r)GetProcAddress(lib, "detect_r"); // from 1.0.5
    if (pdetect_r == NULL) {
        detect pdetect;
        pdetect = (detect)GetProcAddress(lib, "detect"); // before 1.0.5. This API is deprecated on 1.0.5
        if (pdetect == NULL) return NULL;
        DetectObj* obj;
        obj = (DetectObj*)malloc(sizeof(DetectObj));
        short re = pdetect(input, &obj);
        if (re == CHARDET_SUCCESS) {
            return obj;
        }
        return NULL;
    }
    DetectObj* obj;
    obj = (DetectObj*) malloc(sizeof(DetectObj));
    short re = pdetect_r(input, len, &obj);
    if (re == CHARDET_SUCCESS) {
        return obj;
    }
    return NULL;
}
