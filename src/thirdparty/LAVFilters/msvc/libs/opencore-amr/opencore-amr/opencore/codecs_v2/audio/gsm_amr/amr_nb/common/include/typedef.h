/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/****************************************************************************************
Portions of this file are derived from the following 3GPP standard:

    3GPP TS 26.073
    ANSI-C code for the Adaptive Multi-Rate (AMR) speech codec
    Available from http://www.3gpp.org

(C) 2004, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
********************************************************************************
*
*      GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001
*                                R99   Version 3.2.0
*                                REL-4 Version 4.0.0
*
********************************************************************************
*
*      File             : typedef.c
*      Purpose          : Basic types.
*
********************************************************************************
*/
#include "oscl_base_macros.h"// has integer values of PV_COMPILER
#ifndef typedef_h
#define typedef_h "$Id $"

#undef ORIGINAL_TYPEDEF_H /* CHANGE THIS TO #define to get the      */
/*  "original" ETSI version of typedef.h  */
/* CHANGE TO #undef for PV version        */

#ifdef ORIGINAL_TYPEDEF_H
/*
 * this is the original code from the ETSI file typedef.h
 */

#if   defined(__unix__) || defined(__unix)
typedef signed char Word8;
typedef short Word16;
typedef int Word32;
typedef int Flag;

#else
#error No System recognized
#endif
#else /* not original typedef.h */

/*
 * use (improved) type definition file typdefs.h
 */
#include "gsm_amr_typedefs.h"

#endif

#endif
