#ifndef ISPP_INVOKED
/*
 * (C) 2010-2025 see Authors.txt
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


 * Notes:
 * NO_VERSION_REV_NEEDED is defined in those cases where we don't need the revision
   number, but only the major/minor/patch version so the compiler does not rebuild
   everything for every revision. It's currently used in mpcresources, mpciconlib
   and VideoRenderers projects.
 * MPC_VERSION_ARCH is currently used in VSFilter only.
 */
#endif // ISPP_INVOKED

#ifndef MPC_VERSION_H
#define MPC_VERSION_H

#include "mpc-hc_config.h"

#ifndef _T
#if !defined(ISPP_INVOKED) && (defined(UNICODE) || defined(_UNICODE))
#define _T(text)    L##text
#else
#define _T(text)    text
#endif
#endif

#ifdef NO_VERSION_REV_NEEDED
#define MPC_VERSION_REV 0
#else
#include "../build/version_rev.h"
#endif

#define MPC_VERSION_MAJOR       2
#define MPC_VERSION_MINOR       5
#define MPC_VERSION_PATCH       6

#if MPC_VERSION_REV > 0
#define MPC_NIGHTLY_RELEASE     1
#else
#define MPC_NIGHTLY_RELEASE     0
#endif

#define MPC_COMP_NAME_STR       _T("MPC-HC Team")
#define MPC_COPYRIGHT_STR       _T("Copyright 2002-2025 clsid2 and others")
#define MPC_VERSION_COMMENTS    WEBSITE_URL


#ifndef ISPP_INVOKED

#ifdef NO_VERSION_REV_NEEDED

#define MPC_VERSION_NUM         MPC_VERSION_MAJOR,MPC_VERSION_MINOR,MPC_VERSION_PATCH,0
#define MPC_VERSION_STR         MAKE_STR(MPC_VERSION_MAJOR) _T(".") \
                                MAKE_STR(MPC_VERSION_MINOR) _T(".") \
                                MAKE_STR(MPC_VERSION_PATCH)
#define MPC_VERSION_STR_FULL    MPC_VERSION_STR

#else // !NO_VERSION_REV_NEEDED

#define MPC_VERSION_NUM         MPC_VERSION_MAJOR,MPC_VERSION_MINOR,MPC_VERSION_PATCH,MPC_VERSION_REV

#if MPC_NIGHTLY_RELEASE

#define MPC_VERSION_STR         MAKE_STR(MPC_VERSION_MAJOR) _T(".") \
                                MAKE_STR(MPC_VERSION_MINOR) _T(".") \
                                MAKE_STR(MPC_VERSION_PATCH) _T(".") \
                                MAKE_STR(MPC_VERSION_REV)
#define MPC_VERSION_STR_FULL    MAKE_STR(MPC_VERSION_MAJOR) _T(".") \
                                MAKE_STR(MPC_VERSION_MINOR) _T(".") \
                                MAKE_STR(MPC_VERSION_PATCH) _T(".") \
                                MAKE_STR(MPC_VERSION_REV) \
                                MPC_VERSION_ADDITIONAL

#else // !MPC_NIGHTLY_RELEASE

#define MPC_VERSION_STR         MAKE_STR(MPC_VERSION_MAJOR) _T(".") \
                                MAKE_STR(MPC_VERSION_MINOR) _T(".") \
                                MAKE_STR(MPC_VERSION_PATCH)
#define MPC_VERSION_STR_FULL    MAKE_STR(MPC_VERSION_MAJOR) _T(".") \
                                MAKE_STR(MPC_VERSION_MINOR) _T(".") \
                                MAKE_STR(MPC_VERSION_PATCH) \
                                MPC_VERSION_ADDITIONAL

#endif // MPC_NIGHTLY_RELEASE

#endif // NO_VERSION_REV_NEEDED

#endif // ISPP_INVOKED


#if MPC_NIGHTLY_RELEASE
#define MPC_VERSION_NIGHTLY     _T("Nightly")
#endif

#ifdef _WIN64
#define MPC_VERSION_ARCH        _T("x64")
#else
#define MPC_VERSION_ARCH        _T("x86")
#endif

#endif // MPC_VERSION_H
