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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Pathname: ./cpp/include/mime_io.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef MIME_IO_H
#define MIME_IO_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
----------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
----------------------------------------------------------------------------*/
extern const uint8 toc_byte[16];

/* number of speech bits for all modes */
extern const int16 unpacked_size[16];

/* size of packed frame for each mode, excluding TOC byte */
extern const int16 packed_size[16];

/* number of unused speech bits in packed format for each mode */
extern const int16 unused_size[16];

/* sorting tables for all modes */

extern const int16 sort_660[132];

extern const int16 sort_885[177];

extern const int16 sort_1265[253];

extern const int16 sort_1425[285];

extern const int16 sort_1585[317];

extern const int16 sort_1825[365];

extern const int16 sort_1985[397];

extern const int16 sort_2305[461];

extern const int16 sort_2385[477];

extern const int16 sort_SID[35];


/*----------------------------------------------------------------------------
; SIMPLE TYPEDEF'S
----------------------------------------------------------------------------*/



#ifdef __cplusplus
extern "C"
{
#endif


#ifdef __cplusplus
}
#endif




#endif  /* MIME_IO_H */
