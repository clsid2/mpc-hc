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



 Filename: lagconceal.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 gain_hist[],                     (i)  : Gain history
     int16 lag_hist[],                      (i)  : Subframe size
     int16 * T0,                            (i/o): current lag
     int16 * old_T0,                        (i/o): previous lag
     int16 * seed,
     int16 unusable_frame

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Concealment of LTP lags during bad frames

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
#include "pvamrwbdecoder_cnst.h"
#include "pvamrwbdecoder_acelp.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define L_LTPHIST 5
#define ONE_PER_3 10923
#define ONE_PER_LTPHIST 6554

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/
void insertion_sort(int16 array[], int16 n);
void insert(int16 array[], int16 num, int16 x);

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


void Init_Lagconc(int16 lag_hist[])
{
    int16 i;

    for (i = 0; i < L_LTPHIST; i++)
    {
        lag_hist[i] = 64;
    }
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void lagconceal(
    int16 gain_hist[],                   /* (i) : Gain history     */
    int16 lag_hist[],                    /* (i) : Subframe size    */
    int16 * T0,
    int16 * old_T0,
    int16 * seed,
    int16 unusable_frame
)
{
    int16 maxLag, minLag, lastLag, lagDif, meanLag = 0;
    int16 lag_hist2[L_LTPHIST] = {0};
    int16 i, tmp, tmp2;
    int16 minGain, lastGain, secLastGain;
    int16 D, D2;

    /* Is lag index such that it can be aplied directly or does it has to be subtituted */

    lastGain = gain_hist[4];
    secLastGain = gain_hist[3];

    lastLag = lag_hist[0];

    /******* SMALLEST history lag *******/
    minLag = lag_hist[0];
    /*******  BIGGEST history lag *******/
    maxLag = lag_hist[0];
    for (i = 1; i < L_LTPHIST; i++)
    {
        if (lag_hist[i] < minLag)
        {
            minLag = lag_hist[i];
        }
        if (lag_hist[i] > maxLag)
        {
            maxLag = lag_hist[i];
        }
    }
    /***********SMALLEST history gain***********/
    minGain = gain_hist[0];
    for (i = 1; i < L_LTPHIST; i++)
    {

        if (gain_hist[i] < minGain)
        {
            minGain = gain_hist[i];
        }
    }
    /***Difference between MAX and MIN lag**/
    lagDif = sub_int16(maxLag, minLag);


    if (unusable_frame != 0)
    {
        /* LTP-lag for RX_SPEECH_LOST */
        /**********Recognition of the LTP-history*********/

        if ((minGain > 8192) && (lagDif < 10))
        {
            *T0 = *old_T0;
        }
        else if (lastGain > 8192 && secLastGain > 8192)
        {
            *T0 = lag_hist[0];
        }
        else
        {
            /********SORT************/
            /* The sorting of the lag history */
            for (i = 0; i < L_LTPHIST; i++)
            {
                lag_hist2[i] = lag_hist[i];
            }
            insertion_sort(lag_hist2, 5);

            /* Lag is weighted towards bigger lags */
            /* and random variation is added */
            lagDif = sub_int16(lag_hist2[4], lag_hist2[2]);


            if (lagDif > 40)
            {
                lagDif = 40;
            }

            D = noise_gen_amrwb(seed);              /* D={-1, ...,1} */
            /* D2={-lagDif/2..lagDif/2} */
            tmp = lagDif >> 1;
            D2 = mult_int16(tmp, D);
            tmp = add_int16(add_int16(lag_hist2[2], lag_hist2[3]), lag_hist2[4]);
            *T0 = add_int16(mult_int16(tmp, ONE_PER_3), D2);
        }
        /* New lag is not allowed to be bigger or smaller than last lag values */

        if (*T0 > maxLag)
        {
            *T0 = maxLag;
        }

        if (*T0 < minLag)
        {
            *T0 = minLag;
        }
    }
    else
    {
        /* LTP-lag for RX_BAD_FRAME */

        /***********MEAN lag**************/
        meanLag = 0;
        for (i = 0; i < L_LTPHIST; i++)
        {
            meanLag = add_int16(meanLag, lag_hist[i]);
        }
        meanLag = mult_int16(meanLag, ONE_PER_LTPHIST);

        tmp  = *T0 - maxLag;
        tmp2 = *T0 - lastLag;

        if ((lagDif < 10) && (*T0 > (minLag - 5)) && (tmp < 5))
        {
            *T0 = *T0;
        }
        else if ((lastGain > 8192) && (secLastGain > 8192) && ((tmp2 + 10) > 0 && tmp2 < 10))
        {
            *T0 = *T0;
        }
        else if ((minGain < 6554) && (lastGain == minGain) && (*T0 > minLag && *T0 < maxLag))
        {
            *T0 = *T0;
        }
        else if ((lagDif < 70) && (*T0 > minLag) && (*T0 < maxLag))
        {
            *T0 = *T0;
        }
        else if ((*T0 > meanLag) && (*T0 < maxLag))
        {
            *T0 = *T0;
        }
        else
        {


            if ((minGain > 8192) & (lagDif < 10))
            {
                *T0 = lag_hist[0];
            }
            else if ((lastGain > 8192) && (secLastGain > 8192))
            {
                *T0 = lag_hist[0];
            }
            else
            {
                /********SORT************/
                /* The sorting of the lag history */
                for (i = 0; i < L_LTPHIST; i++)
                {
                    lag_hist2[i] = lag_hist[i];
                }
                insertion_sort(lag_hist2, 5);

                /* Lag is weighted towards bigger lags */
                /* and random variation is added */
                lagDif = sub_int16(lag_hist2[4], lag_hist2[2]);

                if (lagDif > 40)
                {
                    lagDif = 40;
                }

                D = noise_gen_amrwb(seed);          /* D={-1,.., 1} */
                /* D2={-lagDif/2..lagDif/2} */
                tmp = lagDif >> 1;
                D2 = mult_int16(tmp, D);
                tmp = add_int16(add_int16(lag_hist2[2], lag_hist2[3]), lag_hist2[4]);
                *T0 = add_int16(mult_int16(tmp, ONE_PER_3), D2);
            }
            /* New lag is not allowed to be bigger or smaller than last lag values */

            if (*T0 > maxLag)
            {
                *T0 = maxLag;
            }

            if (*T0 < minLag)
            {
                *T0 = minLag;
            }
        }
    }
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void insertion_sort(int16 array[], int16 n)
{
    int16 i;

    for (i = 0; i < n; i++)
    {
        insert(array, i, array[i]);
    }
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void insert(int16 array[], int16 n, int16 x)
{
    int16 i;

    for (i = (n - 1); i >= 0; i--)
    {

        if (x < array[i])
        {
            array[i + 1] = array[i];
        }
        else
        {
            break;
        }
    }
    array[i + 1] = x;
}
