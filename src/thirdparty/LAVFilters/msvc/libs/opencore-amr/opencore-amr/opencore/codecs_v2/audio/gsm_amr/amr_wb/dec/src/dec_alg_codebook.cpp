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



 Filename: dec_alg_codebook.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

        decoding of algebraic codebook

------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "q_pulse.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/

#define NB_POS 16                          /* pos in track, mask for sign bit */

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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
void dec_1p_N1(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 pos1;
    int32 mask, i;

    mask = ((1 << N) - 1);
    /*-------------------------------------------------------*
     * Decode 1 pulse with N+1 bits:                         *
     *-------------------------------------------------------*/
    pos1 = ((index & mask) + offset);

    i = ((index >> N) & 1L);            /* i = ((index >> N) & 1); */

    if (i == 1)
    {
        pos1 += NB_POS;
    }
    pos[0] = pos1;

}



/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_2p_2N1(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 pos1, pos2, tmp;
    int32 mask, i;

    mask = (int32)(sub_int16(shl_int16(1, N), 1)); /* mask = ((1<<N)-1); */
    /*-------------------------------------------------------*
     * Decode 2 pulses with 2*N+1 bits:                      *
     *-------------------------------------------------------*/
    /* pos1 = (((index >> N) & mask) + offset); */
    pos1 = (int16)(add_int32((shr_int32(index, N) & mask), (int32)(offset)));
    tmp = shl_int16(N, 1);
    i = (index >> tmp) & 1L;         /* i = (index >> (2*N)) & 1; */
    pos2 = add_int16((int16)(index & mask), offset); /* pos2 = ((index & mask) + offset); */

    if (pos2 < pos1)              /* ((pos2 - pos1) < 0) */
    {
        if (i == 1)
        {                                  /* (i == 1) */
            pos1 += NB_POS;      /* pos1 += NB_POS; */
        }
        else
        {
            pos2 += NB_POS;      /* pos2 += NB_POS;  */
        }
    }
    else
    {
        if (i == 1)
        {                                  /* (i == 1) */
            pos1 += NB_POS;      /* pos1 += NB_POS; */
            pos2 += NB_POS;      /* pos2 += NB_POS; */
        }
    }

    pos[0] = pos1;
    pos[1] = pos2;

    return;
}



/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_3p_3N1(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 j, tmp;
    int32 mask, idx;

    /*-------------------------------------------------------*
     * Decode 3 pulses with 3*N+1 bits:                      *
     *-------------------------------------------------------*/
    tmp = sub_int16(shl_int16(N, 1), 1);               /* mask = ((1<<((2*N)-1))-1); */

    mask = ((1 << ((2 * N) - 1)) - 1);

    idx = index & mask;
    j = offset;
    tmp = (N << 1) - 1;


    if (((index >> tmp) & 1L) != 0L)
    {                                      /* if (((index >> ((2*N)-1)) & 1) == 1){ */
        j += (1 << (N - 1)); /* j += (1<<(N-1)); */
    }
    dec_2p_2N1(idx, (int16)(N - 1), j, pos);

    mask = ((1 << (N + 1)) - 1);
    tmp = N << 1;                     /* idx = (index >> (2*N)) & mask; */
    idx = (index >> tmp) & mask;

    dec_1p_N1(idx, N, offset, pos + 2);

    return;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_4p_4N1(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 j, tmp;
    int32 mask, idx;

    /*-------------------------------------------------------*
     * Decode 4 pulses with 4*N+1 bits:                      *
     *-------------------------------------------------------*/
    tmp = (N << 1) - 1;
    mask = (1L << tmp) - 1L;
    idx = index & mask;
    j = offset;
    tmp = (N << 1) - 1;


    if (((index >> tmp) & 1L) != 0L)
    {                                      /* (((index >> ((2*N)-1)) & 1) == 1) */
        j += (1 << (N - 1)); /* j += (1<<(N-1)); */
    }
    dec_2p_2N1(idx, (int16)(N - 1), j, pos);


    tmp = (N << 1) + 1;             /* mask = ((1<<((2*N)+1))-1); */
    mask = (1L << tmp) - 1L;
    idx = (index >> (N << 1)) & mask;   /* idx = (index >> (2*N)) & mask; */
    dec_2p_2N1(idx, N, offset, pos + 2);      /* dec_2p_2N1(idx, N, offset, pos+2); */

    return;
}



/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_4p_4N(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 j, n_1, tmp;

    /*-------------------------------------------------------*
     * Decode 4 pulses with 4*N bits:                        *
     *-------------------------------------------------------*/

    n_1 = N - 1;
    j = offset + (1 << n_1);          /* j = offset + (1 << n_1); */

    tmp = (N << 2) - 2;

    switch ((index >> tmp) & 3)
    {                                      /* ((index >> ((4*N)-2)) & 3) */
        case 0:
            tmp = (n_1 << 2) + 1;

            if ((index >> tmp) & 1)
            {                                  /* (((index >> ((4*n_1)+1)) & 1) == 0) */
                dec_4p_4N1(index, n_1, j, pos);
            }
            else
            {
                dec_4p_4N1(index, n_1, offset, pos);
            }
            break;
        case 1:
            tmp = (3 * n_1) + 1; /* dec_1p_N1((index>>((3*n_1)+1)), n_1, offset, pos) */
            dec_1p_N1(index >> tmp, n_1, offset, pos);
            dec_3p_3N1(index, n_1, j, pos + 1);
            break;
        case 2:
            tmp = (n_1 << 1) + 1;       /* dec_2p_2N1((index>>((2*n_1)+1)), n_1, offset, pos); */
            dec_2p_2N1(index >> tmp, n_1, offset, pos);
            dec_2p_2N1(index, n_1, j, pos + 2);
            break;
        case 3:
            tmp = n_1 + 1;                 /* dec_3p_3N1((index>>(n_1+1)), n_1, offset, pos); */
            dec_3p_3N1(index >> tmp, n_1, offset, pos);
            dec_1p_N1(index, n_1, j, pos + 3);
            break;
    }
    return;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_5p_5N(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 j, n_1, tmp;
    int32 idx;

    /*-------------------------------------------------------*
     * Decode 5 pulses with 5*N bits:                        *
     *-------------------------------------------------------*/

    n_1 = (int16)(N - 1);
    j = add_int16(offset, shl_int16(1, n_1));          /* j = offset + (1 << n_1); */
    tmp = (N << 1) + 1;             /* idx = (index >> ((2*N)+1)); */
    idx = index >> tmp;
    tmp = (5 * N) - 1;    /* ((5*N)-1)) */


    if ((index >> tmp) & 1)    /* ((index >> ((5*N)-1)) & 1)  */
    {
        dec_3p_3N1(idx, n_1, j, pos);
        dec_2p_2N1(index, N, offset, pos + 3);
    }
    else
    {
        dec_3p_3N1(idx, n_1, offset, pos);
        dec_2p_2N1(index, N, offset, pos + 3);
    }
    return;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_6p_6N_2(int32 index, int16 N, int16 offset, int16 pos[])
{
    int16 j, n_1, offsetA, offsetB;

    n_1 = N - 1;
    j = offset + (1 << n_1);       /* j = offset + (1 << n_1); */


    /* !!  N and n_1 are constants -> it doesn't need to be operated by Basic Operators */

    offsetA = offsetB = j;

    if (((index >> (6*N - 5)) & 1L) == 0)
    {                                      /* if (((index >> ((6*N)-5)) & 1) == 0) */
        offsetA = offset;
    }
    else
    {
        offsetB = offset;
    }


    switch ((index >> (6*N - 4)) & 3)
    {                                      /* (index >> ((6*N)-4)) & 3 */
        case 0:
            dec_5p_5N(index >> N, n_1, offsetA, pos);  /* dec_5p_5N(index>>N, n_1, offsetA, pos); */
            dec_1p_N1(index, n_1, offsetA, pos + 5);
            break;
        case 1:
            dec_5p_5N(index >> N, n_1, offsetA, pos);  /* dec_5p_5N(index>>N, n_1, offsetA, pos); */
            dec_1p_N1(index, n_1, offsetB, pos + 5);
            break;
        case 2:
            dec_4p_4N(index >> (2*n_1 + 1), n_1, offsetA, pos); /* dec_4p_4N(index>>((2*n_1)+1 ), n_1, offsetA, pos); */
            dec_2p_2N1(index, n_1, offsetB, pos + 4);
            break;
        case 3:
            dec_3p_3N1(index >> (3*n_1 + 1), n_1, offset, pos); /* dec_3p_3N1(index>>((3*n_1)+ 1), n_1, offset, pos); */
            dec_3p_3N1(index, n_1, j, pos + 3);
            break;
    }
    return;
}
