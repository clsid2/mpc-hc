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
/*___________________________________________________________________________

    This file contains mathematic operations in fixed point.

    mult_int16_r()     : Same as mult_int16 with rounding
    shr_rnd()          : Same as shr(var1,var2) but with rounding
    div_16by16()       : fractional integer division
    one_ov_sqrt()      : Compute 1/sqrt(L_x)
    one_ov_sqrt_norm() : Compute 1/sqrt(x)
    power_of_2()       : power of 2
    Dot_product12()    : Compute scalar product of <x[],y[]> using accumulator
    Isqrt()            : inverse square root (16 bits precision).
    amrwb_log_2()      : log2 (16 bits precision).

    These operations are not standard double precision operations.
    They are used where low complexity is important and the full 32 bits
    precision is not necessary. For example, the function Div_32() has a
    24 bits precision which is enough for our purposes.

    In this file, the values use theses representations:

    int32 L_32     : standard signed 32 bits format
    int16 hi, lo   : L_32 = hi<<16 + lo<<1  (DPF - Double Precision Format)
    int32 frac, int16 exp : L_32 = frac << exp-31  (normalised format)
    int16 int, frac        : L_32 = int.frac        (fractional format)
 ----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwb_math_op.h"


/*----------------------------------------------------------------------------

     Function Name : mult_int16_r

     Purpose :

     Same as mult_int16 with rounding, i.e.:
       mult_int16_r(var1,var2) = extract_l(L_shr(((var1 * var2) + 16384),15)) and
       mult_int16_r(-32768,-32768) = 32767.

     Complexity weight : 2

     Inputs :

      var1
               16 bit short signed integer (int16) whose value falls in the
               range : 0xffff 8000 <= var1 <= 0x0000 7fff.

      var2
               16 bit short signed integer (int16) whose value falls in the
               range : 0xffff 8000 <= var1 <= 0x0000 7fff.

     Outputs :

      none

     Return Value :

      var_out
               16 bit short signed integer (int16) whose value falls in the
               range : 0xffff 8000 <= var_out <= 0x0000 7fff.
 ----------------------------------------------------------------------------*/

int16 mult_int16_r(int16 var1, int16 var2)
{
    int32 L_product_arr;

    L_product_arr = (int32) var1 * (int32) var2;      /* product */
    L_product_arr += (int32) 0x00004000L;      /* round */
    L_product_arr >>= 15;       /* shift */
    if ((L_product_arr >> 15) != (L_product_arr >> 31))
    {
        L_product_arr = (L_product_arr >> 31) ^ MAX_16;
    }

    return ((int16)L_product_arr);
}



/*----------------------------------------------------------------------------

     Function Name : shr_rnd

     Purpose :

     Same as shr(var1,var2) but with rounding. Saturate the result in case of|
     underflows or overflows :
      - If var2 is greater than zero :
            if (sub(shl_int16(shr(var1,var2),1),shr(var1,sub(var2,1))))
            is equal to zero
                       then
                       shr_rnd(var1,var2) = shr(var1,var2)
                       else
                       shr_rnd(var1,var2) = add_int16(shr(var1,var2),1)
      - If var2 is less than or equal to zero :
                       shr_rnd(var1,var2) = shr(var1,var2).

     Complexity weight : 2

     Inputs :

      var1
               16 bit short signed integer (int16) whose value falls in the
               range : 0xffff 8000 <= var1 <= 0x0000 7fff.

      var2
               16 bit short signed integer (int16) whose value falls in the
               range : 0x0000 0000 <= var2 <= 0x0000 7fff.

     Outputs :

      none

     Return Value :

      var_out
               16 bit short signed integer (int16) whose value falls in the
               range : 0xffff 8000 <= var_out <= 0x0000 7fff.
 ----------------------------------------------------------------------------*/

int16 shr_rnd(int16 var1, int16 var2)
{
    int16 var_out;

    var_out = (int16)(var1 >> (var2 & 0xf));
    if (var2)
    {
        if ((var1 & ((int16) 1 << (var2 - 1))) != 0)
        {
            var_out++;
        }
    }
    return (var_out);
}


/*----------------------------------------------------------------------------

     Function Name : div_16by16

     Purpose :

     Produces a result which is the fractional integer division of var1  by
     var2; var1 and var2 must be positive and var2 must be greater or equal
     to var1; the result is positive (leading bit equal to 0) and truncated
     to 16 bits.
     If var1 = var2 then div(var1,var2) = 32767.

     Complexity weight : 18

     Inputs :

      var1
               16 bit short signed integer (int16) whose value falls in the
               range : 0x0000 0000 <= var1 <= var2 and var2 != 0.

      var2
               16 bit short signed integer (int16) whose value falls in the
               range : var1 <= var2 <= 0x0000 7fff and var2 != 0.

     Outputs :

      none

     Return Value :

      var_out
               16 bit short signed integer (int16) whose value falls in the
               range : 0x0000 0000 <= var_out <= 0x0000 7fff.
               It's a Q15 value (point between b15 and b14).
 ----------------------------------------------------------------------------*/

int16 div_16by16(int16 var1, int16 var2)
{

    int16 var_out = 0;
    register int16 iteration;
    int32 L_num;
    int32 L_denom;
    int32 L_denom_by_2;
    int32 L_denom_by_4;

    if ((var1 > var2) || (var1 < 0))
    {
        return 0; // used to exit(0);
    }
    if (var1)
    {
        if (var1 != var2)
        {

            L_num = (int32) var1;
            L_denom = (int32) var2;
            L_denom_by_2 = (L_denom << 1);
            L_denom_by_4 = (L_denom << 2);
            for (iteration = 5; iteration > 0; iteration--)
            {
                var_out <<= 3;
                L_num   <<= 3;

                if (L_num >= L_denom_by_4)
                {
                    L_num -= L_denom_by_4;
                    var_out |= 4;
                }

                if (L_num >= L_denom_by_2)
                {
                    L_num -= L_denom_by_2;
                    var_out |=  2;
                }

                if (L_num >= (L_denom))
                {
                    L_num -= (L_denom);
                    var_out |=  1;
                }

            }
        }
        else
        {
            var_out = MAX_16;
        }
    }

    return (var_out);

}



/*----------------------------------------------------------------------------

     Function Name : one_ov_sqrt

         Compute 1/sqrt(L_x).
         if L_x is negative or zero, result is 1 (7fffffff).

  Algorithm:

     1- Normalization of L_x.
     2- call Isqrt_n(L_x, exponant)
     3- L_y = L_x << exponant
 ----------------------------------------------------------------------------*/
int32 one_ov_sqrt(     /* (o) Q31 : output value (range: 0<=val<1)         */
    int32 L_x         /* (i) Q0  : input value  (range: 0<=val<=7fffffff) */
)
{
    int16 exp;
    int32 L_y;

    exp = normalize_amr_wb(L_x);
    L_x <<= exp;                 /* L_x is normalized */
    exp = 31 - exp;

    one_ov_sqrt_norm(&L_x, &exp);

    L_y = shl_int32(L_x, exp);                 /* denormalization   */

    return (L_y);
}

/*----------------------------------------------------------------------------

     Function Name : one_ov_sqrt_norm

         Compute 1/sqrt(value).
         if value is negative or zero, result is 1 (frac=7fffffff, exp=0).

  Algorithm:

     The function 1/sqrt(value) is approximated by a table and linear
     interpolation.

     1- If exponant is odd then shift fraction right once.
     2- exponant = -((exponant-1)>>1)
     3- i = bit25-b30 of fraction, 16 <= i <= 63 ->because of normalization.
     4- a = bit10-b24
     5- i -=16
     6- fraction = table[i]<<16 - (table[i] - table[i+1]) * a * 2
 ----------------------------------------------------------------------------*/
static const int16 table_isqrt[49] =
{
    32767, 31790, 30894, 30070, 29309, 28602, 27945, 27330, 26755, 26214,
    25705, 25225, 24770, 24339, 23930, 23541, 23170, 22817, 22479, 22155,
    21845, 21548, 21263, 20988, 20724, 20470, 20225, 19988, 19760, 19539,
    19326, 19119, 18919, 18725, 18536, 18354, 18176, 18004, 17837, 17674,
    17515, 17361, 17211, 17064, 16921, 16782, 16646, 16514, 16384
};

void one_ov_sqrt_norm(
    int32 * frac,                        /* (i/o) Q31: normalized value (1.0 < frac <= 0.5) */
    int16 * exp                          /* (i/o)    : exponent (value = frac x 2^exponent) */
)
{
    int16 i, a, tmp;


    if (*frac <= (int32) 0)
    {
        *exp = 0;
        *frac = 0x7fffffffL;
        return;
    }

    if ((*exp & 1) == 1)  /* If exponant odd -> shift right */
        *frac >>= 1;

    *exp = negate_int16((*exp -  1) >> 1);

    *frac >>= 9;
    i = extract_h(*frac);                  /* Extract b25-b31 */
    *frac >>= 1;
    a = (int16)(*frac);                  /* Extract b10-b24 */
    a = (int16)(a & (int16) 0x7fff);

    i -= 16;

    *frac = L_deposit_h(table_isqrt[i]);   /* table[i] << 16         */
    tmp = table_isqrt[i] - table_isqrt[i + 1];      /* table[i] - table[i+1]) */

    *frac = msu_16by16_from_int32(*frac, tmp, a);          /* frac -=  tmp*a*2       */

    return;
}

/*----------------------------------------------------------------------------

     Function Name : power_2()

       L_x = pow(2.0, exponant.fraction)         (exponant = interger part)
           = pow(2.0, 0.fraction) << exponant

  Algorithm:

     The function power_2(L_x) is approximated by a table and linear
     interpolation.

     1- i = bit10-b15 of fraction,   0 <= i <= 31
     2- a = bit0-b9   of fraction
     3- L_x = table[i]<<16 - (table[i] - table[i+1]) * a * 2
     4- L_x = L_x >> (30-exponant)     (with rounding)
 ----------------------------------------------------------------------------*/
const int16 table_pow2[33] =
{
    16384, 16743, 17109, 17484, 17867, 18258, 18658, 19066, 19484, 19911,
    20347, 20792, 21247, 21713, 22188, 22674, 23170, 23678, 24196, 24726,
    25268, 25821, 26386, 26964, 27554, 28158, 28774, 29405, 30048, 30706,
    31379, 32066, 32767
};

int32 power_of_2(                         /* (o) Q0  : result       (range: 0<=val<=0x7fffffff) */
    int16 exponant,                      /* (i) Q0  : Integer part.      (range: 0<=val<=30)   */
    int16 fraction                       /* (i) Q15 : Fractionnal part.  (range: 0.0<=val<1.0) */
)
{
    int16 exp, i, a, tmp;
    int32 L_x;

    L_x = fraction << 5;          /* L_x = fraction<<6           */
    i = (fraction >> 10);                  /* Extract b10-b16 of fraction */
    a = (int16)(L_x);                    /* Extract b0-b9   of fraction */
    a = (int16)(a & (int16) 0x7fff);

    L_x = ((int32)table_pow2[i]) << 15;    /* table[i] << 16        */
    tmp = table_pow2[i] - table_pow2[i + 1];        /* table[i] - table[i+1] */
    L_x -= ((int32)tmp * a);             /* L_x -= tmp*a*2        */

    exp = 29 - exponant ;

    if (exp)
    {
        L_x = ((L_x >> exp) + ((L_x >> (exp - 1)) & 1));
    }

    return (L_x);
}

/*----------------------------------------------------------------------------
 *
 *   Function Name : Dot_product12()
 *
 *       Compute scalar product of <x[],y[]> using accumulator.
 *
 *       The result is normalized (in Q31) with exponent (0..30).
 *
 *  Algorithm:
 *
 *       dot_product = sum(x[i]*y[i])     i=0..N-1
 ----------------------------------------------------------------------------*/

int32 Dot_product12(   /* (o) Q31: normalized result (1 < val <= -1) */
    int16 x[],        /* (i) 12bits: x vector                       */
    int16 y[],        /* (i) 12bits: y vector                       */
    int16 lg,         /* (i)    : vector length                     */
    int16 * exp       /* (o)    : exponent of result (0..+30)       */
)
{
    int16 i, sft;
    int32 L_sum;
    int16 *pt_x = x;
    int16 *pt_y = y;

    L_sum = 1L;


    for (i = lg >> 3; i != 0; i--)
    {
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
        L_sum = mac_16by16_to_int32(L_sum, *(pt_x++), *(pt_y++));
    }

    /* Normalize acc in Q31 */

    sft = normalize_amr_wb(L_sum);
    L_sum <<= sft;

    *exp = 30 - sft;                    /* exponent = 0..30 */

    return (L_sum);
}

/* Table for Log2() */
const int16 Log2_norm_table[33] =
{
    0, 1455, 2866, 4236, 5568, 6863, 8124, 9352, 10549, 11716,
    12855, 13967, 15054, 16117, 17156, 18172, 19167, 20142, 21097, 22033,
    22951, 23852, 24735, 25603, 26455, 27291, 28113, 28922, 29716, 30497,
    31266, 32023, 32767
};

/*----------------------------------------------------------------------------
 *
 *   FUNCTION:   Lg2_normalized()
 *
 *   PURPOSE:   Computes log2(L_x, exp),  where   L_x is positive and
 *              normalized, and exp is the normalisation exponent
 *              If L_x is negative or zero, the result is 0.
 *
 *   DESCRIPTION:
 *        The function Log2(L_x) is approximated by a table and linear
 *        interpolation. The following steps are used to compute Log2(L_x)
 *
 *           1- exponent = 30-norm_exponent
 *           2- i = bit25-b31 of L_x;  32<=i<=63  (because of normalization).
 *           3- a = bit10-b24
 *           4- i -=32
 *           5- fraction = table[i]<<16 - (table[i] - table[i+1]) * a * 2
 *
----------------------------------------------------------------------------*/
void Lg2_normalized(
    int32 L_x,         /* (i) : input value (normalized)                    */
    int16 exp,         /* (i) : norm_l (L_x)                                */
    int16 *exponent,   /* (o) : Integer part of Log2.   (range: 0<=val<=30) */
    int16 *fraction    /* (o) : Fractional part of Log2. (range: 0<=val<1)  */
)
{
    int16 i, a, tmp;
    int32 L_y;

    if (L_x <= (int32) 0)
    {
        *exponent = 0;
        *fraction = 0;;
        return;
    }

    *exponent = 30 - exp;

    L_x >>= 9;
    i = extract_h(L_x);                 /* Extract b25-b31 */
    L_x >>= 1;
    a = (int16)(L_x);                 /* Extract b10-b24 of fraction */
    a &= 0x7fff;

    i -= 32;

    L_y = L_deposit_h(Log2_norm_table[i]);             /* table[i] << 16        */
    tmp = Log2_norm_table[i] - Log2_norm_table[i + 1]; /* table[i] - table[i+1] */
    L_y = msu_16by16_from_int32(L_y, tmp, a);           /* L_y -= tmp*a*2        */

    *fraction = extract_h(L_y);

    return;
}



/*----------------------------------------------------------------------------
 *
 *   FUNCTION:   amrwb_log_2()
 *
 *   PURPOSE:   Computes log2(L_x),  where   L_x is positive.
 *              If L_x is negative or zero, the result is 0.
 *
 *   DESCRIPTION:
 *        normalizes L_x and then calls Lg2_normalized().
 *
 ----------------------------------------------------------------------------*/
void amrwb_log_2(
    int32 L_x,         /* (i) : input value                                 */
    int16 *exponent,   /* (o) : Integer part of Log2.   (range: 0<=val<=30) */
    int16 *fraction    /* (o) : Fractional part of Log2. (range: 0<=val<1) */
)
{
    int16 exp;

    exp = normalize_amr_wb(L_x);
    Lg2_normalized(shl_int32(L_x, exp), exp, exponent, fraction);
}


/*****************************************************************************
 *
 *  These operations are not standard double precision operations.           *
 *  They are used where single precision is not enough but the full 32 bits  *
 *  precision is not necessary. For example, the function Div_32() has a     *
 *  24 bits precision which is enough for our purposes.                      *
 *                                                                           *
 *  The double precision numbers use a special representation:               *
 *                                                                           *
 *     L_32 = hi<<16 + lo<<1                                                 *
 *                                                                           *
 *  L_32 is a 32 bit integer.                                                *
 *  hi and lo are 16 bit signed integers.                                    *
 *  As the low part also contains the sign, this allows fast multiplication. *
 *                                                                           *
 *      0x8000 0000 <= L_32 <= 0x7fff fffe.                                  *
 *                                                                           *
 *  We will use DPF (Double Precision Format )in this file to specify        *
 *  this special format.                                                     *
 *****************************************************************************
*/


/*----------------------------------------------------------------------------
 *
 *  Function int32_to_dpf()
 *
 *  Extract from a 32 bit integer two 16 bit DPF.
 *
 *  Arguments:
 *
 *   L_32      : 32 bit integer.
 *               0x8000 0000 <= L_32 <= 0x7fff ffff.
 *   hi        : b16 to b31 of L_32
 *   lo        : (L_32 - hi<<16)>>1
 *
 ----------------------------------------------------------------------------*/

void int32_to_dpf(int32 L_32, int16 *hi, int16 *lo)
{
    *hi = (int16)(L_32 >> 16);
    *lo = (int16)((L_32 - (*hi << 16)) >> 1);
    return;
}


/*----------------------------------------------------------------------------
 * Function mpy_dpf_32()
 *
 *   Multiply two 32 bit integers (DPF). The result is divided by 2**31
 *
 *   L_32 = (hi1*hi2)<<1 + ( (hi1*lo2)>>15 + (lo1*hi2)>>15 )<<1
 *
 *   This operation can also be viewed as the multiplication of two Q31
 *   number and the result is also in Q31.
 *
 * Arguments:
 *
 *  hi1         hi part of first number
 *  lo1         lo part of first number
 *  hi2         hi part of second number
 *  lo2         lo part of second number
 *
 ----------------------------------------------------------------------------*/

int32 mpy_dpf_32(int16 hi1, int16 lo1, int16 hi2, int16 lo2)
{
    int32 L_32;

    L_32 = mul_16by16_to_int32(hi1, hi2);
    L_32 = mac_16by16_to_int32(L_32, mult_int16(hi1, lo2), 1);
    L_32 = mac_16by16_to_int32(L_32, mult_int16(lo1, hi2), 1);

    return (L_32);
}


