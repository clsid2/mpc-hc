/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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

 Filename: gsm_amr_typedefs.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains the definition of the amr codec types.

------------------------------------------------------------------------------
*/
#ifndef GSM_AMR_TYPEDEFS_H
#define GSM_AMR_TYPEDEFS_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include    "oscl_base.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here.
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; SIMPLE TYPEDEF'S
----------------------------------------------------------------------------*/

#ifndef Word8
typedef int8        Word8;
#endif

#ifndef UWord8
typedef uint8       UWord8;
#endif


/*----------------------------------------------------------------------------
; Define 16 bit signed and unsigned words
----------------------------------------------------------------------------*/

#ifndef Word16
typedef int16       Word16;

#endif

#ifndef UWord16
typedef uint16      UWord16;

#endif

/*----------------------------------------------------------------------------
; Define 32 bit signed and unsigned words
----------------------------------------------------------------------------*/

#ifndef Word32
typedef int32       Word32;
#endif

#ifndef UWord32
typedef uint32      UWord32;
#endif


/*----------------------------------------------------------------------------
; Define boolean type
----------------------------------------------------------------------------*/
#ifndef Bool
typedef int     Bool;
#endif

#ifndef FALSE
#define FALSE       0
#endif

#ifndef TRUE
#define TRUE        1
#endif

#ifndef OFF
#define OFF     0
#endif

#ifndef ON
#define ON      1
#endif

#ifndef NO
#define NO      0
#endif

#ifndef YES
#define YES     1
#endif

#ifndef SUCCESS
#define SUCCESS     0
#endif

#ifndef  NULL
#define  NULL       0
#endif

#ifndef Flag
typedef int32     Flag;
#endif

#endif   /*  GSM_AMR_TYPEDEFS_H */
