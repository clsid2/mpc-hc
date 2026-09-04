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
------------------------------------------------------------------------------



 Filename: s10_8pf.cpp
 Funtions: search_10and8i40

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "s10_8pf.h"
#include "cnst.h"
#include "oscl_mem.h"

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
 FUNCTION NAME: search_10and8i40
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    nbPulse = nbPulses to find (Word16)
    step = step size (Word16)
    nbTracks = nbTracks (Word16)
    dn[] = correlation between target and h[] (Word16)
    rr[][] = matrix of autocorrelation (Word16)
    ipos[] = starting position of each pulse (Word16)
    pos_max[] = Position of maximum dn[] (Word16)
    codvec[] = Algebraic codebook vector (Word16)
    pOverflow = pointer to Overflow flag (Flag)

 Outputs:
    codvec[] = Algebraic codebook vector (Word16)
    pOverflow -> 1 if processing this funvction results in satuaration

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function searches for the best codevector; It determines the positions
 of the 10/8 pulses in the 40-sample frame.

    search_10and8i40 (10,5,5,dn, rr, ipos, pos_max, codvec);   for GSMEFR
    search_10and8i40 (8, 4,4,dn, rr, ipos, pos_max, codvec);   for 10.2


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 s10_8pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void search_10and8i40 (
    Word16 nbPulse,      // i : nbpulses to find
    Word16 step,         // i :  stepsize
    Word16 nbTracks,     // i :  nbTracks
    Word16 dn[],         // i : correlation between target and h[]
    Word16 rr[][L_CODE], // i : matrix of autocorrelation
    Word16 ipos[],       // i : starting position for each pulse
    Word16 pos_max[],    // i : position of maximum of dn[]
    Word16 codvec[]      // o : algebraic codebook vector
)
{
   Word16 i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;
   Word16 i, j, k, pos, ia, ib;
   Word16 psk, ps, ps0, ps1, ps2, sq, sq2;
   Word16 alpk, alp, alp_16;
   Word16 rrv[L_CODE];
   Word32 s, alp0, alp1, alp2;
   Word16 gsmefrFlag;


   if (sub(nbPulse, 10) == 0)
   {
      gsmefrFlag=1;
   }
   else
   {
      gsmefrFlag=0;
   }

   // fix i0 on maximum of correlation position
   i0 = pos_max[ipos[0]];

   //
   // i1 loop:                                                         *
   //

   // Default value

   psk = -1;
   alpk = 1;
   for (i = 0; i < nbPulse; i++)
   {
      codvec[i] = i;
   }

   for (i = 1; i < nbTracks; i++)
   {
      i1 = pos_max[ipos[1]];
      ps0 = add (dn[i0], dn[i1]);
      alp0 = L_mult (rr[i0][i0], _1_16);
      alp0 = L_mac (alp0, rr[i1][i1], _1_16);
      alp0 = L_mac (alp0, rr[i0][i1], _1_8);

      //
      // i2 and i3 loop
      //

      for (i3 = ipos[3]; i3 < L_CODE; i3 += step)
      {
         s = L_mult (rr[i3][i3], _1_8);       // index incr= step+L_CODE
         s = L_mac (s, rr[i0][i3], _1_4);     // index increment = step
         s = L_mac (s, rr[i1][i3], _1_4);     // index increment = step
         rrv[i3] = pv_round (s);
      }

      // Default value
      sq = -1;
      alp = 1;
      ps = 0;
      ia = ipos[2];
      ib = ipos[3];

      for (i2 = ipos[2]; i2 < L_CODE; i2 += step)
      {
         // index increment = step
         ps1 = add (ps0, dn[i2]);

         // index incr= step+L_CODE
         alp1 = L_mac (alp0, rr[i2][i2], _1_16);

         // index increment = step
         alp1 = L_mac (alp1, rr[i0][i2], _1_8);

         // index increment = step
         alp1 = L_mac (alp1, rr[i1][i2], _1_8);

         for (i3 = ipos[3]; i3 < L_CODE; i3 += step)
         {
            // index increment = step
            ps2 = add (ps1, dn[i3]);

            // index increment = step
            alp2 = L_mac (alp1, rrv[i3], _1_2);

            // index increment = step
            alp2 = L_mac (alp2, rr[i2][i3], _1_8);

            sq2 = mult (ps2, ps2);

            alp_16 = pv_round (alp2);

            s = L_msu (L_mult (alp, sq2), sq, alp_16);

            if (s > 0)
            {
               sq = sq2;
               ps = ps2;
               alp = alp_16;
               ia = i2;
               ib = i3;
            }
         }
      }
      i2 = ia;
      i3 = ib;

        //
        // i4 and i5 loop:
        //

        ps0 = ps;
        alp0 = L_mult (alp, _1_2);

        for (i5 = ipos[5]; i5 < L_CODE; i5 += step)
        {
            s = L_mult (rr[i5][i5], _1_8);
            s = L_mac (s, rr[i0][i5], _1_4);
            s = L_mac (s, rr[i1][i5], _1_4);
            s = L_mac (s, rr[i2][i5], _1_4);
            s = L_mac (s, rr[i3][i5], _1_4);
            rrv[i5] = pv_round (s);
        }

        // Default value
        sq = -1;
        alp = 1;
        ps = 0;
        ia = ipos[4];
        ib = ipos[5];

        for (i4 = ipos[4]; i4 < L_CODE; i4 += step)
        {
            ps1 = add (ps0, dn[i4]);

            alp1 = L_mac (alp0, rr[i4][i4], _1_32);
            alp1 = L_mac (alp1, rr[i0][i4], _1_16);
            alp1 = L_mac (alp1, rr[i1][i4], _1_16);
            alp1 = L_mac (alp1, rr[i2][i4], _1_16);
            alp1 = L_mac (alp1, rr[i3][i4], _1_16);

            for (i5 = ipos[5]; i5 < L_CODE; i5 += step)
            {
                ps2 = add (ps1, dn[i5]);

                alp2 = L_mac (alp1, rrv[i5], _1_4);
                alp2 = L_mac (alp2, rr[i4][i5], _1_16);

                sq2 = mult (ps2, ps2);

                alp_16 = pv_round (alp2);

                s = L_msu (L_mult (alp, sq2), sq, alp_16);

                if (s > 0)
                {
                    sq = sq2;
                    ps = ps2;
                    alp = alp_16;
                    ia = i4;
                    ib = i5;
                }
            }
        }
        i4 = ia;
        i5 = ib;

        //
        // i6 and i7 loop:
        //

        ps0 = ps;
        alp0 = L_mult (alp, _1_2);

        for (i7 = ipos[7]; i7 < L_CODE; i7 += step)
        {
            s = L_mult (rr[i7][i7], _1_16);
            s = L_mac (s, rr[i0][i7], _1_8);
            s = L_mac (s, rr[i1][i7], _1_8);
            s = L_mac (s, rr[i2][i7], _1_8);
            s = L_mac (s, rr[i3][i7], _1_8);
            s = L_mac (s, rr[i4][i7], _1_8);
            s = L_mac (s, rr[i5][i7], _1_8);
            rrv[i7] = pv_round (s);
        }

        // Default value
        sq = -1;
        alp = 1;
        ps = 0;
        ia = ipos[6];
        ib = ipos[7];

        for (i6 = ipos[6]; i6 < L_CODE; i6 += step)
        {
            ps1 = add (ps0, dn[i6]);

            alp1 = L_mac (alp0, rr[i6][i6], _1_64);
            alp1 = L_mac (alp1, rr[i0][i6], _1_32);
            alp1 = L_mac (alp1, rr[i1][i6], _1_32);
            alp1 = L_mac (alp1, rr[i2][i6], _1_32);
            alp1 = L_mac (alp1, rr[i3][i6], _1_32);
            alp1 = L_mac (alp1, rr[i4][i6], _1_32);
            alp1 = L_mac (alp1, rr[i5][i6], _1_32);

            for (i7 = ipos[7]; i7 < L_CODE; i7 += step)
            {
                ps2 = add (ps1, dn[i7]);

                alp2 = L_mac (alp1, rrv[i7], _1_4);
                alp2 = L_mac (alp2, rr[i6][i7], _1_32);

                sq2 = mult (ps2, ps2);

                alp_16 = pv_round (alp2);

                s = L_msu (L_mult (alp, sq2), sq, alp_16);

                if (s > 0)
                {
                    sq = sq2;
                    ps = ps2;
                    alp = alp_16;
                    ia = i6;
                    ib = i7;
                }
            }
        }
        i6 = ia;
        i7 = ib;

        // now finished searching a set of 8 pulses

        if(gsmefrFlag != 0){
           // go on with the two last pulses for GSMEFR
           //
           // i8 and i9 loop:
           //

           ps0 = ps;
           alp0 = L_mult (alp, _1_2);

           for (i9 = ipos[9]; i9 < L_CODE; i9 += step)
           {
              s = L_mult (rr[i9][i9], _1_16);
              s = L_mac (s, rr[i0][i9], _1_8);
              s = L_mac (s, rr[i1][i9], _1_8);
              s = L_mac (s, rr[i2][i9], _1_8);
              s = L_mac (s, rr[i3][i9], _1_8);
              s = L_mac (s, rr[i4][i9], _1_8);
              s = L_mac (s, rr[i5][i9], _1_8);
              s = L_mac (s, rr[i6][i9], _1_8);
              s = L_mac (s, rr[i7][i9], _1_8);
              rrv[i9] = pv_round (s);
           }

           // Default value
           sq = -1;
           alp = 1;
           ps = 0;
           ia = ipos[8];
           ib = ipos[9];

           for (i8 = ipos[8]; i8 < L_CODE; i8 += step)
           {
              ps1 = add (ps0, dn[i8]);

              alp1 = L_mac (alp0, rr[i8][i8], _1_128);
              alp1 = L_mac (alp1, rr[i0][i8], _1_64);
              alp1 = L_mac (alp1, rr[i1][i8], _1_64);
              alp1 = L_mac (alp1, rr[i2][i8], _1_64);
              alp1 = L_mac (alp1, rr[i3][i8], _1_64);
              alp1 = L_mac (alp1, rr[i4][i8], _1_64);
              alp1 = L_mac (alp1, rr[i5][i8], _1_64);
              alp1 = L_mac (alp1, rr[i6][i8], _1_64);
              alp1 = L_mac (alp1, rr[i7][i8], _1_64);

              for (i9 = ipos[9]; i9 < L_CODE; i9 += step)
              {
                 ps2 = add (ps1, dn[i9]);

                 alp2 = L_mac (alp1, rrv[i9], _1_8);
                 alp2 = L_mac (alp2, rr[i8][i9], _1_64);

                 sq2 = mult (ps2, ps2);

                 alp_16 = pv_round (alp2);

                 s = L_msu (L_mult (alp, sq2), sq, alp_16);

                 if (s > 0)
                 {
                    sq = sq2;
                    ps = ps2;
                    alp = alp_16;
                    ia = i8;
                    ib = i9;
                 }
              }
           }
        } // end  gsmefrFlag

        //
        // test and memorise if this combination is better than the last one/
        //

        s = L_msu (L_mult (alpk, sq), psk, alp);

        if (s > 0)
        {
            psk = sq;
            alpk = alp;
            codvec[0] = i0;
            codvec[1] = i1;
            codvec[2] = i2;
            codvec[3] = i3;
            codvec[4] = i4;
            codvec[5] = i5;
            codvec[6] = i6;
            codvec[7] = i7;

            if (gsmefrFlag != 0)
            {
               codvec[8] = ia;
               codvec[9] = ib;
            }
        }

        //
        // Cyclic permutation of i1,i2,i3,i4,i5,i6,i7,(i8 and i9)/
        //

        pos = ipos[1];
        for (j = 1, k = 2; k < nbPulse; j++, k++)
        {
            ipos[j] = ipos[k];
        }
        ipos[sub(nbPulse,1)] = pos;
   } // end 1..nbTracks  loop
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
void search_10and8i40(
    Word16 nbPulse,      /* i : nbpulses to find                       */
    Word16 step,         /* i : stepsize                               */
    Word16 nbTracks,     /* i : nbTracks                               */
    Word16 dn[],         /* i : correlation between target and h[]     */
    Word16 rr[][L_CODE], /* i : matrix of autocorrelation              */
    Word16 ipos[],       /* i : starting position for each pulse       */
    Word16 pos_max[],    /* i : position of maximum of dn[]            */
    Word16 codvec[],     /* o : algebraic codebook vector              */
    Flag   *pOverflow    /* i/o : overflow flag                        */
)
{
    Word16 i0, i1, i2, i3, i4, i5, i6, i7, i9;
    Word16 i, j, k/*, m*/;
    Word16 pos, ia, ib;
    Word16 psk;
    Word16 sq, sq2;
    Word16 alpk, alp, alp_16;
    Word32 s;
    Word32 alp0, alp1, alp2;
    Word16 gsmefrFlag;
    Word16 *p_codvec = codvec;
    Word16  *p_temp2;

    Word16  temp1[2*L_CODE];
    Word16  *p_temp1;
    Word16  ps2;
    Word16  ps1;
    Word16  ps;
    Word16 ps0;

    Word16  index[10];

    OSCL_UNUSED_ARG(pOverflow);

    if (nbPulse == 10)
    {
        gsmefrFlag = 1;
    }
    else
    {
        gsmefrFlag = 0;
    }

    /* fix i0 on maximum of correlation position */
    i0 = pos_max[ipos[0]];
    index[0] = i0;
    /*------------------------------------------------------------------*
    * i1 loop:                                                         *
    *------------------------------------------------------------------*/

    /* Default value */
    psk = -1;
    alpk = 1;
    for (i = 0; i < nbPulse; i++)
    {
        *(p_codvec++) = i;
    }

    for (i = 1; i < nbTracks; i++)
    {
        i1 = pos_max[ipos[1]];
        index[1] = i1;

        /* ps0 = add (dn[i0], dn[i1], pOverflow);*/
        ps0 = (Word16)((Word32) dn[i0] + dn[i1]);

        /* alp0 = L_mult (rr[i0][i0], _1_16, pOverflow); */
        alp0 = (Word32) rr[i0][i0] << 12;

        /* alp0 = L_mac (alp0, rr[i1][i1], _1_16, pOverflow); */
        alp0 += (Word32) rr[i1][i1] << 12;

        /* alp0 = L_mac (alp0, rr[i0][i1], _1_8, pOverflow); */
        alp0 += (Word32) rr[i0][i1] << 13;
        alp0 += 0x00008000L;

        /*----------------------------------------------------------------*
        * i2 and i3 loop:                                                *
        *----------------------------------------------------------------*/

        p_temp1 = temp1;
        for (i3 = ipos[3]; i3 < L_CODE; i3 += step)
        {
            p_temp2 = &rr[i3][0];
            s  = (Word32) * (p_temp2 + i3) >> 1;
            s += (Word32) * (p_temp2 + i0);
            s += (Word32) * (p_temp2 + i1);
            *(p_temp1++) = ps0 + dn[i3];
            *(p_temp1++) = (Word16)((s + 2) >> 2);
        }

        /* Default value */
        sq = -1;
        alp = 1;
        ps = 0;
        ia = ipos[2];
        ib = ipos[3];

        s = (alp0 >> 12);

        for (j = ipos[2]; j < L_CODE; j += step)
        {
            /* index increment = step  */
            p_temp2 = &rr[j][0];

            alp1 = (s + (Word32) * (p_temp2 + j)) >> 1;

            alp1 += (Word32) * (p_temp2 + i0);

            alp1 += (Word32) * (p_temp2 + i1);

            p_temp1 = temp1;
            ps1 = dn[j];


            for (i3 = ipos[3]; i3 < L_CODE; i3 += step)
            {
                /* index increment = step */
                ps2 = ps1 + *(p_temp1++);

                sq2 = (Word16)(((Word32) ps2 * ps2) >> 15);

                alp2 = (alp1 + p_temp2[i3]) >> 2;
                alp2 = (alp2 + *(p_temp1++)) >> 1;  /*  alp2 is always > 0  */
                if (((Word32) sq2 * alp) > ((Word32) sq * alp2))
                {
                    sq = sq2;
                    ps = ps2;
                    alp = (Word16)alp2;
                    ia = j;
                    ib = i3;
                }
            }

        }
        i2 = ia;
        i3 = ib;
        index[2] = ia;
        index[3] = ib;

        /*----------------------------------------------------------------*
        * i4 and i5 loop:                                                *
        *----------------------------------------------------------------*/

        alp0 = ((Word32) alp << 15) + 0x00008000L;
        p_temp1 = temp1;

        for (i5 = ipos[5]; i5 < L_CODE; i5 += step)
        {
            p_temp2 = &rr[i5][0];
            s = (Word32) * (p_temp2 + i5) >> 1;
            s += (Word32) * (p_temp2 + i0);
            s += (Word32) * (p_temp2 + i1);
            s += (Word32) * (p_temp2 + i2);
            s += (Word32) * (p_temp2 + i3);

            *(p_temp1++) = ps + dn[i5];
            *(p_temp1++) = (Word16)((s + 2) >> 2);
        }

        /* Default value */
        sq = -1;
        alp = 1;
        ps = 0;
        ia = ipos[4];
        ib = ipos[5];

        for (j = ipos[4]; j < L_CODE; j += step)
        {
            /* ps1 = add (ps0, dn[i4], pOverflow); */
            p_temp2 = &rr[j][0];

            /* alp1 = L_mac (alp0, rr[i4][i4], _1_32, pOverflow); */
            alp1 = alp0 + ((Word32) * (p_temp2 + j) << 11);

            /* alp1 = L_mac (alp1, rr[i0][i4], _1_16, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i0) << 12;

            /* alp1 = L_mac (alp1, rr[i1][i4], _1_16, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i1) << 12;

            /* alp1 = L_mac (alp1, rr[i2][i4], _1_16, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i2) << 12;

            /* alp1 = L_mac (alp1, rr[i3][i4], _1_16, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i3) << 12;

            p_temp1 = temp1;
            ps1 =  dn[j];

            for (i5 = ipos[5]; i5 < L_CODE; i5 += step)
            {
                ps2 = ps1 + *(p_temp1++);

                alp2 = alp1 + ((Word32) * (p_temp2 + i5) << 12);

                alp_16 = (Word16)((alp2 + ((Word32) * (p_temp1++) << 14)) >> 16);
                sq2 = (Word16)(((Word32) ps2 * ps2) >> 15);

                if (((Word32) sq2 * alp) > ((Word32) sq * alp_16))
                {
                    sq = sq2;
                    ps = ps2;
                    alp = alp_16;
                    ia = j;
                    ib = i5;
                }

            }
        }
        i4 = ia;
        i5 = ib;
        index[4] = ia;
        index[5] = ib;

        /*----------------------------------------------------------------*
        * i6 and i7 loop:                                                *
        *----------------------------------------------------------------*/

        alp0 = ((Word32) alp << 15) + 0x00008000L;

        p_temp1 = temp1;

        for (i7 = ipos[7]; i7 < L_CODE; i7 += step)
        {
            s = (Word32) rr[i7][i7] >> 1;
            s += (Word32) rr[i0][i7];
            s += (Word32) rr[i1][i7];
            s += (Word32) rr[i2][i7];
            s += (Word32) rr[i3][i7];
            s += (Word32) rr[i4][i7];
            s += (Word32) rr[i5][i7];
            *(p_temp1++) = ps + dn[i7];
            *(p_temp1++) = (Word16)((s + 4) >> 3);
        }


        /* Default value */
        sq = -1;
        alp = 1;
        ps = 0;
        ia = ipos[6];
        ib = ipos[7];

        for (j = ipos[6]; j < L_CODE; j += step)
        {
            /* ps1 = add (ps0, dn[i6], pOverflow); */

            p_temp2 = (Word16 *) & rr[j];

            /* alp1 = L_mac (alp0, rr[i6][i6], _1_64, pOverflow); */
            alp1 = alp0 + ((Word32) * (p_temp2 + j) << 10);

            /* alp1 = L_mac (alp1, rr[i0][i6], _1_32, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i0) << 11;


            /* alp1 = L_mac (alp1, rr[i1][i6], _1_32, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i1) << 11;

            /* alp1 = L_mac (alp1, rr[i2][i6], _1_32, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i2) << 11;

            /* alp1 = L_mac (alp1, rr[i3][i6], _1_32, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i3) << 11;

            /* alp1 = L_mac (alp1, rr[i4][i6], _1_32, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i4) << 11;

            /* alp1 = L_mac (alp1, rr[i5][i6], _1_32, pOverflow); */
            alp1 += (Word32) * (p_temp2 + i5) << 11;

            p_temp1 = temp1;
            ps1 = dn[j];

            for (i7 = ipos[7]; i7 < L_CODE; i7 += step)
            {
                ps2 = ps1 + *(p_temp1++);

                alp2 = alp1 + ((Word32) * (p_temp2 + i7) << 11);

                alp_16 = (Word16)((alp2 + ((Word32) * (p_temp1++) << 14)) >> 16);

                sq2 = (Word16)(((Word32) ps2 * ps2) >> 15);

                if (((Word32) sq2 * alp) > ((Word32) sq * alp_16))
                {
                    sq = sq2;
                    ps = ps2;
                    alp = alp_16;
                    ia = j;
                    ib = i7;
                }
            }
        }

        i6 = ia;
        i7 = ib;
        index[6] = ia;
        index[7] = ib;

        /* now finished searching a set of 8 pulses */

        if (gsmefrFlag != 0)
        {
            /* go on with the two last pulses for GSMEFR                      */
            /*----------------------------------------------------------------*
            * i8 and i9 loop:                                                *
            *----------------------------------------------------------------*/

            alp0 = ((Word32) alp << 15) + 0x00008000L;

            p_temp1 = temp1;

            for (i9 = ipos[9]; i9 < L_CODE; i9 += step)
            {
                s = (Word32) rr[i9][i9] >> 1;
                s += (Word32) rr[i0][i9];
                s += (Word32) rr[i1][i9];
                s += (Word32) rr[i2][i9];
                s += (Word32) rr[i3][i9];
                s += (Word32) rr[i4][i9];
                s += (Word32) rr[i5][i9];
                s += (Word32) rr[i6][i9];
                s += (Word32) rr[i7][i9];

                *(p_temp1++) = ps + dn[i9];
                *(p_temp1++) = (Word16)((s + 4) >> 3);
            }

            /* Default value */
            sq = -1;
            alp = 1;
            ps = 0;
            ia = ipos[8];
            ib = ipos[9];

            for (j = ipos[8]; j < L_CODE; j += step)
            {
                /* ps1 = add (ps0, dn[i8], pOverflow); */
                p_temp2 = &rr[j][0];

                /* alp1 = L_mac (alp0, rr[i8][i8], _1_128, pOverflow); */
                alp1 = alp0 + ((Word32) * (p_temp2 + j) << 9);

                /* alp1 = L_mac (alp1, rr[i0][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i0][j] << 10;

                /* alp1 = L_mac (alp1, rr[i1][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i1][j] << 10;

                /* alp1 = L_mac (alp1, rr[i2][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i2][j] << 10;

                /* alp1 = L_mac (alp1, rr[i3][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i3][j] << 10;

                /* alp1 = L_mac (alp1, rr[i4][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i4][j] << 10;

                /* alp1 = L_mac (alp1, rr[i5][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i5][j] << 10;

                /* alp1 = L_mac (alp1, rr[i6][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i6][j] << 10;

                /* alp1 = L_mac (alp1, rr[i7][i8], _1_64, pOverflow); */
                alp1 += (Word32) rr[i7][j] << 10;

                p_temp1 = temp1;
                ps1 = dn[j];

                for (i9 = ipos[9]; i9 < L_CODE; i9 += step)
                {
                    /* ps2 = add (ps1, dn[i9], pOverflow); */
                    ps2 = ps1 + *(p_temp1++);

                    /* sq2 = mult (ps2, ps2, pOverflow); */
                    sq2 = (Word16)(((Word32) ps2 * ps2) >> 15);

                    /* alp2 = L_mac (alp1, rrv[i9], _1_8, pOverflow); */
                    alp2 = alp1 + ((Word32) * (p_temp2 + i9) << 10) ;

                    /* alp2 = L_mac (alp2, rr[i8][i9], _1_64, pOverflow); */
                    alp_16 = (Word16)((alp2 + ((Word32) * (p_temp1++) << 13)) >> 16);

                    if (((Word32) sq2 * alp) > ((Word32) sq * alp_16))
                    {
                        sq = sq2;
                        ps = ps2;
                        alp = alp_16;
                        ia = j;
                        ib = i9;
                    }
                }
            }

            index[8] = ia;
            index[9] = ib;

        }/* end  gsmefrFlag */

        /*----------------------------------------------------------------  *
         * test and memorise if this combination is better than the last one.*
         *----------------------------------------------------------------*/

        if (((Word32) alpk * sq) > ((Word32) psk * alp))
        {
            psk = sq;
            alpk = alp;

            if (gsmefrFlag != 0)
            {
                oscl_memcpy(codvec, index, (2*NB_TRACK)*sizeof(*index));
            }
            else
            {
                oscl_memcpy(codvec, index, (2*NB_TRACK_MR102)*sizeof(*index));
            }

        }
        /*----------------------------------------------------------------*
        * Cyclic permutation of i1,i2,i3,i4,i5,i6,i7,(i8 and i9).          *
        *----------------------------------------------------------------*/

        pos = ipos[1];
        for (j = 1, k = 2; k < nbPulse; j++, k++)
        {
            ipos[j] = ipos[k];
        }
        ipos[nbPulse-1] = pos;
    } /* end 1..nbTracks  loop*/
}

