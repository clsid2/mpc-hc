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

 Filename: r_fft.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "oper_32b.h"
#include "vad2.h"
#include "sub.h"
#include "add.h"
#include "shr.h"
#include "shl.h"
#include "l_mult.h"
#include "l_mac.h"
#include "l_msu.h"
#include "round.h"
#include "l_negate.h"
#include "l_deposit_h.h"
#include "l_shr.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define         SIZE            128
#define         SIZE_BY_TWO     64
#define         NUM_STAGE       6
#define         TRUE            1
#define         FALSE           0

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

const Word16 phs_tbl[] =
{

    32767,       0,  32729,  -1608,  32610,  -3212,  32413,  -4808,
    32138,   -6393,  31786,  -7962,  31357,  -9512,  30853, -11039,
    30274,  -12540,  29622, -14010,  28899, -15447,  28106, -16846,
    27246,  -18205,  26320, -19520,  25330, -20788,  24279, -22006,
    23170,  -23170,  22006, -24279,  20788, -25330,  19520, -26320,
    18205,  -27246,  16846, -28106,  15447, -28899,  14010, -29622,
    12540,  -30274,  11039, -30853,   9512, -31357,   7962, -31786,
    6393,  -32138,   4808, -32413,   3212, -32610,   1608, -32729,
    0,  -32768,  -1608, -32729,  -3212, -32610,  -4808, -32413,
    -6393, -32138,  -7962, -31786,  -9512, -31357, -11039, -30853,
    -12540, -30274, -14010, -29622, -15447, -28899, -16846, -28106,
    -18205, -27246, -19520, -26320, -20788, -25330, -22006, -24279,
    -23170, -23170, -24279, -22006, -25330, -20788, -26320, -19520,
    -27246, -18205, -28106, -16846, -28899, -15447, -29622, -14010,
    -30274, -12540, -30853, -11039, -31357,  -9512, -31786,  -7962,
    -32138,  -6393, -32413,  -4808, -32610,  -3212, -32729,  -1608

};

const Word16 ii_table[] =
    {SIZE / 2, SIZE / 4, SIZE / 8, SIZE / 16, SIZE / 32, SIZE / 64};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: c_fft
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    farray_ptr = pointer to complex array that the FFT operates on of type
                 Word16.
    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow = 1 if the math functions called by cor_h_x2 result in overflow
    else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This is an implementation of decimation-in-time FFT algorithm for
 real sequences.  The techniques used here can be found in several
 books, e.g., i) Proakis and Manolakis, "Digital Signal Processing",
 2nd Edition, Chapter 9, and ii) W.H. Press et. al., "Numerical
 Recipes in C", 2nd Ediiton, Chapter 12.

 Input -  There is one input to this function:

   1) An integer pointer to the input data array

 Output - There is no return value.
   The input data are replaced with transformed data.  If the
   input is a real time domain sequence, it is replaced with
   the complex FFT for positive frequencies.  The FFT value
   for DC and the foldover frequency are combined to form the
   first complex number in the array.  The remaining complex
   numbers correspond to increasing frequencies.  If the input
   is a complex frequency domain sequence arranged as above,
   it is replaced with the corresponding time domain sequence.

 Notes:

   1) This function is designed to be a part of a VAD
      algorithm that requires 128-point FFT of real
      sequences.  This is achieved here through a 64-point
      complex FFT.  Consequently, the FFT size information is
      not transmitted explicitly.  However, some flexibility
      is provided in the function to change the size of the
      FFT by specifying the size information through "define"
      statements.

   2) The values of the complex sinusoids used in the FFT
      algorithm are stored in a ROM table.

   3) In the c_fft function, the FFT values are divided by
      2 after each stage of computation thus dividing the
      final FFT values by 64.  This is somewhat different
          from the usual definition of FFT where the factor 1/N,
          i.e., 1/64, used for the IFFT and not the FFT.  No factor
          is used in the r_fft function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 r_fft.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

The original etsi reference code uses a global flag Overflow. However, in the
actual implementation a pointer to a the overflow flag is passed in.

void c_fft(Word16 * farray_ptr)
{
    Word16 i, j, k, ii, jj, kk, ji, kj, ii2;
    Word32 ftmp, ftmp_real, ftmp_imag;
    Word16 tmp, tmp1, tmp2;

    // Rearrange the input array in bit reversed order
    for (i = 0, j = 0; i < SIZE - 2; i = i + 2)
    {
        if (sub(j, i) > 0)
        {
            ftmp = *(farray_ptr + i);
            *(farray_ptr + i) = *(farray_ptr + j);
            *(farray_ptr + j) = ftmp;

            ftmp = *(farray_ptr + i + 1);
            *(farray_ptr + i + 1) = *(farray_ptr + j + 1);
            *(farray_ptr + j + 1) = ftmp;
        }

        k = SIZE_BY_TWO;
        while (sub(j, k) >= 0)
        {
            j = sub(j, k);
            k = shr(k, 1);
        }
        j = add(j, k);
    }

    // The FFT part
    for (i = 0; i < NUM_STAGE; i++)
    {               // i is stage counter
        jj = shl(2, i);     // FFT size
        kk = shl(jj, 1);    // 2 * FFT size
        ii = ii_table[i];   // 2 * number of FFT's
        ii2 = shl(ii, 1);
        ji = 0;         // ji is phase table index

        for (j = 0; j < jj; j = j + 2)
        {                   // j is sample counter

            for (k = j; k < SIZE; k = k + kk)
            {               // k is butterfly top
                kj = add(k, jj);    // kj is butterfly bottom

                // Butterfly computations
                ftmp_real = L_mult(*(farray_ptr + kj), phs_tbl[ji]);
                ftmp_real = L_msu(ftmp_real, *(farray_ptr + kj + 1), phs_tbl[ji + 1]);

                ftmp_imag = L_mult(*(farray_ptr + kj + 1), phs_tbl[ji]);
                ftmp_imag = L_mac(ftmp_imag, *(farray_ptr + kj), phs_tbl[ji + 1]);

                tmp1 = pv_round(ftmp_real);
                tmp2 = pv_round(ftmp_imag);

                tmp = sub(*(farray_ptr + k), tmp1);
                *(farray_ptr + kj) = shr(tmp, 1);

                tmp = sub(*(farray_ptr + k + 1), tmp2);
                *(farray_ptr + kj + 1) = shr(tmp, 1);

                tmp = add(*(farray_ptr + k), tmp1);
                *(farray_ptr + k) = shr(tmp, 1);

                tmp = add(*(farray_ptr + k + 1), tmp2);
                *(farray_ptr + k + 1) = shr(tmp, 1);
            }

            ji =  add(ji, ii2);
        }
    }
}                               // end of c_fft ()


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/* FFT function for complex sequences */
/*
 * The decimation-in-time complex FFT is implemented below.
 * The input complex numbers are presented as real part followed by
 * imaginary part for each sample.  The counters are therefore
 * incremented by two to access the complex valued samples.
 */

void c_fft(Word16 * farray_ptr, Flag *pOverflow)
{

    Word16 i;
    Word16 j;
    Word16 k;
    Word16 ii;
    Word16 jj;
    Word16 kk;
    Word16 ji;
    Word16 kj;
    Word16 ii2;
    Word32 ftmp;
    Word32 ftmp_real;
    Word32 ftmp_imag;
    Word16 tmp;
    Word16 tmp1;
    Word16 tmp2;

    /* Rearrange the input array in bit reversed order */
    for (i = 0, j = 0; i < SIZE - 2; i = i + 2)
    {
        if (j > i)
        {
            ftmp = *(farray_ptr + i);
            *(farray_ptr + i) = *(farray_ptr + j);
            *(farray_ptr + j) = (Word16)ftmp;

            ftmp = *(farray_ptr + i + 1);
            *(farray_ptr + i + 1) = *(farray_ptr + j + 1);
            *(farray_ptr + j + 1) = (Word16)ftmp;
        }

        k = SIZE_BY_TWO;
        while (j >= k)
        {
            j = sub(j, k, pOverflow);
            k = shr(k, 1, pOverflow);
        }
        j = add(j, k, pOverflow);
    }

    /* The FFT part */
    for (i = 0; i < NUM_STAGE; i++)
    {               /* i is stage counter */
        jj = shl(2, i, pOverflow);     /* FFT size */
        kk = shl(jj, 1, pOverflow);    /* 2 * FFT size */
        ii = ii_table[i];   /* 2 * number of FFT's */
        ii2 = shl(ii, 1, pOverflow);
        ji = 0;         /* ji is phase table index */

        for (j = 0; j < jj; j = j + 2)
        {                   /* j is sample counter */

            for (k = j; k < SIZE; k = k + kk)
            {               /* k is butterfly top */
                kj = add(k, jj, pOverflow);    /* kj is butterfly bottom */

                /* Butterfly computations */
                ftmp_real = L_mult(*(farray_ptr + kj), phs_tbl[ji], pOverflow);
                ftmp_real = L_msu(ftmp_real, *(farray_ptr + kj + 1),
                                  phs_tbl[ji + 1], pOverflow);

                ftmp_imag = L_mult(*(farray_ptr + kj + 1),
                                   phs_tbl[ji], pOverflow);
                ftmp_imag = L_mac(ftmp_imag, *(farray_ptr + kj),
                                  phs_tbl[ji + 1], pOverflow);

                tmp1 = pv_round(ftmp_real, pOverflow);
                tmp2 = pv_round(ftmp_imag, pOverflow);

                tmp = sub(*(farray_ptr + k), tmp1, pOverflow);
                *(farray_ptr + kj) = shr(tmp, 1, pOverflow);

                tmp = sub(*(farray_ptr + k + 1), tmp2, pOverflow);
                *(farray_ptr + kj + 1) = shr(tmp, 1, pOverflow);

                tmp = add(*(farray_ptr + k), tmp1, pOverflow);
                *(farray_ptr + k) = shr(tmp, 1, pOverflow);

                tmp = add(*(farray_ptr + k + 1), tmp2, pOverflow);
                *(farray_ptr + k + 1) = shr(tmp, 1, pOverflow);
            }

            ji =  add(ji, ii2, pOverflow);
        }
    }
}                               /* end of c_fft () */


/*
------------------------------------------------------------------------------
 FUNCTION NAME: r_fft
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    farray_ptr = pointer to complex array that the FFT operates on of type
                 Word16.
    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow = 1 if the math functions called by cor_h_x2 result in overflow
    else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This is an implementation of decimation-in-time FFT algorithm for
 real sequences.  The techniques used here can be found in several
 books, e.g., i) Proakis and Manolakis, "Digital Signal Processing",
 2nd Edition, Chapter 9, and ii) W.H. Press et. al., "Numerical
 Recipes in C", 2nd Ediiton, Chapter 12.

 Input -  There is one input to this function:

   1) An integer pointer to the input data array

 Output - There is no return value.
   The input data are replaced with transformed data.  If the
   input is a real time domain sequence, it is replaced with
   the complex FFT for positive frequencies.  The FFT value
   for DC and the foldover frequency are combined to form the
   first complex number in the array.  The remaining complex
   numbers correspond to increasing frequencies.  If the input
   is a complex frequency domain sequence arranged as above,
   it is replaced with the corresponding time domain sequence.

 Notes:

   1) This function is designed to be a part of a VAD
      algorithm that requires 128-point FFT of real
      sequences.  This is achieved here through a 64-point
      complex FFT.  Consequently, the FFT size information is
      not transmitted explicitly.  However, some flexibility
      is provided in the function to change the size of the
      FFT by specifying the size information through "define"
      statements.

   2) The values of the complex sinusoids used in the FFT
      algorithm are stored in a ROM table.

   3) In the c_fft function, the FFT values are divided by
      2 after each stage of computation thus dividing the
      final FFT values by 64.  This is somewhat different
          from the usual definition of FFT where the factor 1/N,
          i.e., 1/64, used for the IFFT and not the FFT.  No factor
          is used in the r_fft function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 r_fft.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

The original etsi reference code uses a global flag Overflow. However, in the
actual implementation a pointer to a the overflow flag is passed in.

void r_fft(Word16 * farray_ptr)
{

    Word16 ftmp1_real, ftmp1_imag, ftmp2_real, ftmp2_imag;
    Word32 Lftmp1_real, Lftmp1_imag;
    Word16 i, j;
    Word32 Ltmp1;

    // Perform the complex FFT
    c_fft(farray_ptr);

    // First, handle the DC and foldover frequencies
    ftmp1_real = *farray_ptr;
    ftmp2_real = *(farray_ptr + 1);
    *farray_ptr = add(ftmp1_real, ftmp2_real);
    *(farray_ptr + 1) = sub(ftmp1_real, ftmp2_real);

    // Now, handle the remaining positive frequencies
    for (i = 2, j = SIZE - i; i <= SIZE_BY_TWO; i = i + 2, j = SIZE - i)
    {
        ftmp1_real = add(*(farray_ptr + i), *(farray_ptr + j));
        ftmp1_imag = sub(*(farray_ptr + i + 1), *(farray_ptr + j + 1));
        ftmp2_real = add(*(farray_ptr + i + 1), *(farray_ptr + j + 1));
        ftmp2_imag = sub(*(farray_ptr + j), *(farray_ptr + i));

        Lftmp1_real = L_deposit_h(ftmp1_real);
        Lftmp1_imag = L_deposit_h(ftmp1_imag);

        Ltmp1 = L_mac(Lftmp1_real, ftmp2_real, phs_tbl[i]);
        Ltmp1 = L_msu(Ltmp1, ftmp2_imag, phs_tbl[i + 1]);
        *(farray_ptr + i) = pv_round(L_shr(Ltmp1, 1));

        Ltmp1 = L_mac(Lftmp1_imag, ftmp2_imag, phs_tbl[i]);
        Ltmp1 = L_mac(Ltmp1, ftmp2_real, phs_tbl[i + 1]);
        *(farray_ptr + i + 1) = pv_round(L_shr(Ltmp1, 1));

        Ltmp1 = L_mac(Lftmp1_real, ftmp2_real, phs_tbl[j]);
        Ltmp1 = L_mac(Ltmp1, ftmp2_imag, phs_tbl[j + 1]);
        *(farray_ptr + j) = pv_round(L_shr(Ltmp1, 1));

        Ltmp1 = L_negate(Lftmp1_imag);
        Ltmp1 = L_msu(Ltmp1, ftmp2_imag, phs_tbl[j]);
        Ltmp1 = L_mac(Ltmp1, ftmp2_real, phs_tbl[j + 1]);
        *(farray_ptr + j + 1) = pv_round(L_shr(Ltmp1, 1));

    }
}                               // end r_fft ()

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/* FFT function for complex sequences */
/*
 * The decimation-in-time complex FFT is implemented below.
 * The input complex numbers are presented as real part followed by
 * imaginary part for each sample.  The counters are therefore
 * incremented by two to access the complex valued samples.
 */
void r_fft(Word16 * farray_ptr, Flag *pOverflow)
{

    Word16 ftmp1_real;
    Word16 ftmp1_imag;
    Word16 ftmp2_real;
    Word16 ftmp2_imag;
    Word32 Lftmp1_real;
    Word32 Lftmp1_imag;
    Word16 i;
    Word16 j;
    Word32 Ltmp1;

    /* Perform the complex FFT */
    c_fft(farray_ptr, pOverflow);

    /* First, handle the DC and foldover frequencies */
    ftmp1_real = *farray_ptr;
    ftmp2_real = *(farray_ptr + 1);
    *farray_ptr = add(ftmp1_real, ftmp2_real, pOverflow);
    *(farray_ptr + 1) = sub(ftmp1_real, ftmp2_real, pOverflow);

    /* Now, handle the remaining positive frequencies */
    for (i = 2, j = SIZE - i; i <= SIZE_BY_TWO; i = i + 2, j = SIZE - i)
    {
        ftmp1_real = add(*(farray_ptr + i), *(farray_ptr + j), pOverflow);
        ftmp1_imag = sub(*(farray_ptr + i + 1),
                         *(farray_ptr + j + 1), pOverflow);
        ftmp2_real = add(*(farray_ptr + i + 1),
                         *(farray_ptr + j + 1), pOverflow);
        ftmp2_imag = sub(*(farray_ptr + j),
                         *(farray_ptr + i), pOverflow);

        Lftmp1_real = L_deposit_h(ftmp1_real);
        Lftmp1_imag = L_deposit_h(ftmp1_imag);

        Ltmp1 = L_mac(Lftmp1_real, ftmp2_real, phs_tbl[i], pOverflow);
        Ltmp1 = L_msu(Ltmp1, ftmp2_imag, phs_tbl[i + 1], pOverflow);
        *(farray_ptr + i) = pv_round(L_shr(Ltmp1, 1, pOverflow), pOverflow);

        Ltmp1 = L_mac(Lftmp1_imag, ftmp2_imag, phs_tbl[i], pOverflow);
        Ltmp1 = L_mac(Ltmp1, ftmp2_real, phs_tbl[i + 1], pOverflow);
        *(farray_ptr + i + 1) = pv_round(L_shr(Ltmp1, 1, pOverflow), pOverflow);

        Ltmp1 = L_mac(Lftmp1_real, ftmp2_real, phs_tbl[j], pOverflow);
        Ltmp1 = L_mac(Ltmp1, ftmp2_imag, phs_tbl[j + 1], pOverflow);
        *(farray_ptr + j) = pv_round(L_shr(Ltmp1, 1, pOverflow), pOverflow);

        Ltmp1 = L_negate(Lftmp1_imag);
        Ltmp1 = L_msu(Ltmp1, ftmp2_imag, phs_tbl[j], pOverflow);
        Ltmp1 = L_mac(Ltmp1, ftmp2_real, phs_tbl[j + 1], pOverflow);
        *(farray_ptr + j + 1) = pv_round(L_shr(Ltmp1, 1, pOverflow), pOverflow);

    }
}                               /* end r_fft () */
