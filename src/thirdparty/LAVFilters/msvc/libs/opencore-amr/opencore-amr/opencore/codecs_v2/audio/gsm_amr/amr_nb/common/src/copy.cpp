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
********************************************************************************
*
*      GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001
*                                R99   Version 3.2.0
*                                REL-4 Version 4.0.0
*
********************************************************************************
*      File             : copy.h
*
********************************************************************************
*/

/*
********************************************************************************
*                         MODULE INCLUDE FILE AND VERSION ID
********************************************************************************
*/
#include "copy.h"

/*
********************************************************************************
*                         INCLUDE FILES
********************************************************************************
*/
#include "typedef.h"
#include "basic_op.h"
#include "oscl_mem.h"

/*
********************************************************************************
*                         PUBLIC PROGRAM CODE
********************************************************************************
*/
/*************************************************************************
 *
 *   FUNCTION:   Copy
 *
 *   PURPOSE:   Copy vector x[] to y[]
 *
 *
 *************************************************************************/
/*
**************************************************************************
*
*  Function    : Copy
*  Purpose     : Copy vector x[] to y[]
*
**************************************************************************
*/
void Copy(
    const Word16 x[],   /* i : input vector (L)  */
    Word16 y[],         /* o : output vector (L) */
    Word16 L            /* i : vector length     */
)
{
    oscl_memmove(y, x, L*sizeof(*x));
}
