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
 Filename: int_lpc.cpp
 Functions:

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "int_lpc.h"
#include "typedef.h"
#include "cnst.h"
#include "lsp_az.h"
#include "basic_op.h"

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


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Int_lpc_1and3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp_old -- array of type Word16 -- LSP vector at the
                                       4th subfr. of past frame (M)
    lsp_mid -- array of type Word16 -- LSP vector at the 2nd subfr. of
                                       present frame (M)
    lsp_new -- array of type Word16 -- LSP vector at the 4th subfr. of
                                       present frame (M)

 Outputs:
    Az -- array of type Word16 -- interpolated LP parameters in all subfr.
                                  (AZ_SIZE)
    pOverflow -- pointer to type Flag -- Overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose     : Interpolates the LSPs and converts to LPC parameters
               to get a different LP filter in each subframe.
 Description : The 20 ms speech frame is divided into 4 subframes.
               The LSPs are quantized and transmitted at the 2nd and
               4th subframes (twice per frame) and interpolated at the
               1st and 3rd subframe.

                     |------|------|------|------|
                        sf1    sf2    sf3    sf4
                  F0            Fm            F1

                sf1:   1/2 Fm + 1/2 F0         sf3:   1/2 F1 + 1/2 Fm
                sf2:       Fm                  sf4:       F1

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 int_lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Int_lpc_1and3(
    Word16 lsp_old[],  /* i : LSP vector at the 4th subfr. of past frame (M) */
    Word16 lsp_mid[],  /* i : LSP vector at the 2nd subfr. of
                              present frame (M)                              */
    Word16 lsp_new[],  /* i : LSP vector at the 4th subfr. of
                              present frame (M)                              */
    Word16 Az[],       /* o : interpolated LP parameters in all subfr.
                              (AZ_SIZE)                                      */
    Flag  *pOverflow
)
{
    Word16 i;
    Word16 lsp[M];
    Word16 *p_lsp_old = &lsp_old[0];
    Word16 *p_lsp_mid = &lsp_mid[0];
    Word16 *p_lsp_new = &lsp_new[0];
    Word16 *p_lsp     = &lsp[0];

    /*  lsp[i] = lsp_mid[i] * 0.5 + lsp_old[i] * 0.5 */

    for (i = M >> 1; i != 0; i--)
    {
        *(p_lsp++) = (*(p_lsp_old++) >> 1) + (*(p_lsp_mid++) >> 1);
        *(p_lsp++) = (*(p_lsp_old++) >> 1) + (*(p_lsp_mid++) >> 1);
    }

    Lsp_Az(
        lsp,
        Az,
        pOverflow);       /* Subframe 1 */

    Az += MP1;

    Lsp_Az(
        lsp_mid,
        Az,
        pOverflow);       /* Subframe 2 */

    Az += MP1;

    p_lsp_mid = &lsp_mid[0];
    p_lsp     = &lsp[0];

    for (i = M >> 1; i != 0; i--)
    {
        *(p_lsp++) = (*(p_lsp_mid++) >> 1) + (*(p_lsp_new++) >> 1);
        *(p_lsp++) = (*(p_lsp_mid++) >> 1) + (*(p_lsp_new++) >> 1);
    }

    Lsp_Az(
        lsp,
        Az,
        pOverflow);           /* Subframe 3 */

    Az += MP1;

    Lsp_Az(
        lsp_new,
        Az,
        pOverflow);       /* Subframe 4 */

    return;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Int_lpc_1and3_2
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp_old -- array of type Word16 -- LSP vector at the
                                       4th subfr. of past frame (M)
    lsp_mid -- array of type Word16 -- LSP vector at the 2nd subfr. of
                                       present frame (M)
    lsp_new -- array of type Word16 -- LSP vector at the 4th subfr. of
                                       present frame (M)

 Outputs:
    Az -- array of type Word16 -- interpolated LP parameters in.
                                  subfr 1 and 2.
    pOverflow -- pointer to type Flag -- Overflow indicator

 Returns:
    None

 Global Variables Used:


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose : Interpolation of the LPC parameters. Same as the Int_lpc
           function but we do not recompute Az() for subframe 2 and
           4 because it is already available.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 int_lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Int_lpc_1and3_2(
    Word16 lsp_old[],  /* i : LSP vector at the 4th subfr. of past frame (M) */
    Word16 lsp_mid[],  /* i : LSP vector at the 2nd subframe of
                             present frame (M)                               */
    Word16 lsp_new[],  /* i : LSP vector at the 4th subframe of
                             present frame (M)                               */
    Word16 Az[],       /* o :interpolated LP parameters
                             in subframes 1 and 3 (AZ_SIZE)                  */
    Flag  *pOverflow
)
{
    Word16 i;
    Word16 lsp[M];
    Word16 *p_lsp_old = &lsp_old[0];
    Word16 *p_lsp_mid = &lsp_mid[0];
    Word16 *p_lsp_new = &lsp_new[0];
    Word16 *p_lsp     = &lsp[0];

    /*  lsp[i] = lsp_mid[i] * 0.5 + lsp_old[i] * 0.5 */

    for (i = M >> 1; i != 0; i--)
    {
        *(p_lsp++) = (*(p_lsp_old++) >> 1) + (*(p_lsp_mid++) >> 1);
        *(p_lsp++) = (*(p_lsp_old++) >> 1) + (*(p_lsp_mid++) >> 1);
    }
    Lsp_Az(lsp, Az, pOverflow);            /* Subframe 1 */
    Az += MP1 * 2;

    p_lsp_mid = &lsp_mid[0];
    p_lsp     = &lsp[0];

    for (i = M >> 1; i != 0; i--)
    {
        *(p_lsp++) = (*(p_lsp_mid++) >> 1) + (*(p_lsp_new++) >> 1);
        *(p_lsp++) = (*(p_lsp_mid++) >> 1) + (*(p_lsp_new++) >> 1);
    }

    Lsp_Az(lsp, Az, pOverflow);            /* Subframe 3 */

    return;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: lsp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp_old -- array of type Word16 -- LSP vector at the
                                       4th subfr. of past frame (M)
    lsp_new -- array of type Word16 -- LSP vector at the 4th subfr. of
                                       present frame (M)

 Outputs:
    Az -- array of type Word16 -- interpolated LP parameters in.
                                  all subframes.
    pOverflow -- pointer to type Flag -- Overflow indicator

 Returns:
    None

 Global Variables Used:


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE:  Interpolates the LSPs and convert to LP parameters to get
           a different LP filter in each subframe.

 DESCRIPTION:
    The 20 ms speech frame is divided into 4 subframes.
    The LSPs are quantized and transmitted at the 4th subframe
    (once per frame) and interpolated at the 1st, 2nd and 3rd subframe.

         |------|------|------|------|
            sf1    sf2    sf3    sf4
      F0                          F1

    sf1:   3/4 F0 + 1/4 F1         sf3:   1/4 F0 + 3/4 F1
    sf2:   1/2 F0 + 1/2 F1         sf4:       F1

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 int_lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Int_lpc_1to3(
    Word16 lsp_old[], /* input : LSP vector at the 4th SF of past frame    */
    Word16 lsp_new[], /* input : LSP vector at the 4th SF of present frame */
    Word16 Az[],      /* output: interpolated LP parameters in all SFs     */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 temp;

    Word16 lsp[M];

    for (i = 0; i < M; i++)
    {
        temp = lsp_old[i] - (lsp_old[i] >> 2);
        lsp[i] = temp + (lsp_new[i] >> 2);
    }

    Lsp_Az(
        lsp,
        Az,
        pOverflow);        /* Subframe 1 */

    Az += MP1;


    for (i = 0; i < M; i++)
    {
        lsp[i] = (lsp_new[i] >> 1) + (lsp_old[i] >> 1);

    }

    Lsp_Az(lsp, Az, pOverflow);        /* Subframe 2 */

    Az += MP1;

    for (i = 0; i < M; i++)
    {

        temp = lsp_new[i] - (lsp_new[i] >> 2);
        lsp[i] = temp + (lsp_old[i] >> 2);

    }

    Lsp_Az(lsp, Az, pOverflow);        /* Subframe 3 */

    Az += MP1;

    Lsp_Az(lsp_new, Az, pOverflow);    /* Subframe 4 */

    return;
}
/*
------------------------------------------------------------------------------
 FUNCTION NAME: Int_lpc_1to3_2
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp_old -- array of type Word16 -- LSP vector at the
                                       4th subfr. of past frame (M)
    lsp_new -- array of type Word16 -- LSP vector at the 4th subfr. of
                                       present frame (M)

 Outputs:
    Az -- array of type Word16 -- interpolated LP parameters in.
                                  subfr 1, 2, and 3.
    pOverflow -- pointer to type Flag -- Overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Interpolation of the LPC parameters.
 Same as the previous function but we do not recompute Az() for
 subframe 4 because it is already available.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 int_lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Int_lpc_1to3_2(
    Word16 lsp_old[],  /* input : LSP vector at the 4th SF of past frame    */
    Word16 lsp_new[],  /* input : LSP vector at the 4th SF of present frame */
    Word16 Az[],       /* output: interpolated LP parameters in SFs 1,2,3   */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 temp;
    Word16 lsp[M];

    for (i = 0; i < M; i++)
    {
        temp = lsp_old[i] - (lsp_old[i] >> 2);
        lsp[i] = temp + (lsp_new[i] >> 2);

    }

    Lsp_Az(lsp, Az, pOverflow);         /* Subframe 1 */

    Az += MP1;

    for (i = 0; i < M; i++)
    {
        lsp[i] = (lsp_new[i] >> 1) + (lsp_old[i] >> 1);

    }

    Lsp_Az(lsp, Az, pOverflow);         /* Subframe 2 */

    Az += MP1;

    for (i = 0; i < M; i++)
    {
        temp = lsp_new[i] - (lsp_new[i] >> 2);
        lsp[i] = temp + (lsp_old[i] >> 2);

    }

    Lsp_Az(lsp, Az, pOverflow);         /* Subframe 3 */

    return;
}

