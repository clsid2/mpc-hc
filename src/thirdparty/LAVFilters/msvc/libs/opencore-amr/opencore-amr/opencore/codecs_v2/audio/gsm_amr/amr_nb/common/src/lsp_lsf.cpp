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
 Filename: lsp_lsf.cpp
 Functions: Lsp_lsf
            Lsf_lsp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that convert line spectral pairs (LSP) to
 line spectral frequencies (LSF) and vice-versa.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lsp_lsf.h"
#include "basicop_malloc.h"
#include "basic_op.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; Define module specific macros here
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; Include all pre-processor statements here. Include conditional
    ; compile variables also.
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    extern const Word16 table[];
    extern const Word16 slope[];


    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Lsf_lsp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf = buffer containing normalized line spectral frequencies; valid
          range is between 0 and 0.5 (Word16)
    lsp = buffer containing line spectral pairs; valid range is between
          -1 and 1 (Word16)
    m = LPC order (Word16)

 Outputs:
    lsp contains the newly calculated line spectral pairs

 Returns:
    None

 Global Variables Used:
    table = cosine table

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the LSF to LSP transformation using the equation:

    lsf[i] = arccos(lsp[i])/(2*pi)

 The transformation from lsp[i] to lsf[i] is approximated by a look-up table
 and interpolation.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp_lsf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Lsf_lsp (
    Word16 lsf[],       // (i) : lsf[m] normalized (range: 0.0<=val<=0.5)
    Word16 lsp[],       // (o) : lsp[m] (range: -1<=val<1)
    Word16 m            // (i) : LPC order
)
{
    Word16 i, ind, offset;
    Word32 L_tmp;

    for (i = 0; i < m; i++)
    {
        ind = shr (lsf[i], 8);      // ind    = b8-b15 of lsf[i]
        offset = lsf[i] & 0x00ff;    // offset = b0-b7  of lsf[i]

        // lsp[i] = table[ind]+ ((table[ind+1]-table[ind])*offset) / 256

        L_tmp = L_mult (sub (table[ind + 1], table[ind]), offset);
        lsp[i] = add (table[ind], extract_l (L_shr (L_tmp, 9)));

    }
    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Lsf_lsp(
    Word16 lsf[],       /* (i) : lsf[m] normalized (range: 0.0<=val<=0.5) */
    Word16 lsp[],       /* (o) : lsp[m] (range: -1<=val<1)                */
    Word16 m,           /* (i) : LPC order                                */
    Flag   *pOverflow   /* (o) : Flag set when overflow occurs            */
)
{
    Word16 i, ind, offset;
    Word32 L_tmp;

    for (i = 0; i < m; i++)
    {
        ind = lsf[i] >> 8;           /* ind    = b8-b15 of lsf[i] */
        offset = lsf[i] & 0x00ff;    /* offset = b0-b7  of lsf[i] */

        /* lsp[i] = table[ind]+ ((table[ind+1]-table[ind])*offset) / 256 */

        L_tmp = ((Word32)(table[ind + 1] - table[ind]) * offset) >> 8;
        lsp[i] = table[ind] + (Word16) L_tmp;

    }

    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Lsp_lsf
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp = buffer containing line spectral pairs; valid range is between
          -1 and 1 (Word16)
    lsf = buffer containing normalized line spectral frequencies; valid
          range is between 0 and 0.5 (Word16)
    m = LPC order (Word16)

 Outputs:
    lsf contains the newly calculated normalized line spectral frequencies

 Returns:
    None

 Global Variables Used:
    table = cosine table
    slope = table to used to calculate inverse cosine

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the LSP to LSF transformation using the equation:

    lsp[i] = cos(2*pi*lsf[i])

 The transformation from lsf[i] to lsp[i] is approximated by a look-up table
 and interpolation.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp_lsf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Lsp_lsf (
    Word16 lsp[],       // (i)  : lsp[m] (range: -1<=val<1)
    Word16 lsf[],       // (o)  : lsf[m] normalized (range: 0.0<=val<=0.5)
    Word16 m            // (i)  : LPC order
)
{
    Word16 i, ind;
    Word32 L_tmp;

    ind = 63;                        // begin at end of table -1

    for (i = m - 1; i >= 0; i--)
    {
        // find value in table that is just greater than lsp[i]

        while (sub (table[ind], lsp[i]) < 0)
        {
            ind--;

        }

        // acos(lsp[i])= ind*256 + ( ( lsp[i]-table[ind] ) *
           slope[ind] )/4096

        L_tmp = L_mult (sub (lsp[i], table[ind]), slope[ind]);
        //(lsp[i]-table[ind])*slope[ind])>>12
        lsf[i] = pv_round (L_shl (L_tmp, 3));
        lsf[i] = add (lsf[i], shl (ind, 8));
    }
    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Lsp_lsf(
    Word16 lsp[],       /* (i)  : lsp[m] (range: -1<=val<1)                */
    Word16 lsf[],       /* (o)  : lsf[m] normalized (range: 0.0<=val<=0.5) */
    Word16 m,           /* (i)  : LPC order                                */
    Flag  *pOverflow    /* (o)  : Flag set when overflow occurs            */
)
{
    Word16 i;
    Word16 ind;
    Word16 temp;
    Word32 L_tmp;
    Word16 *p_lsp = &lsp[m-1];
    Word16 *p_lsf = &lsf[m-1];
    OSCL_UNUSED_ARG(pOverflow);

    ind = 63;                        /* begin at end of table -1 */

    for (i = m - 1; i >= 0; i--)
    {
        /* find value in table that is just greater than lsp[i] */
        temp = *(p_lsp--);
        while (table[ind] < temp)
        {
            ind--;
        }

        /* acos(lsp[i])= ind*256 + ( ( lsp[i]-table[ind] ) *
           slope[ind] )/4096 */

        L_tmp = (Word32)(temp - table[ind]) * slope[ind];

        /*(lsp[i]-table[ind])*slope[ind])>>12*/
        L_tmp  = (L_tmp + 0x00000800) >> 12;

        *(p_lsf--) = (Word16)(L_tmp) + (ind << 8);
    }

    return;
}
