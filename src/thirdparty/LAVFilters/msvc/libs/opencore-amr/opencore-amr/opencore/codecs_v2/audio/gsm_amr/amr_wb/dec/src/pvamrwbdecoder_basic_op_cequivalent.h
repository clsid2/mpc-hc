/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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



 Pathname: ./src/pvamrwbdecoder_basic_op_cequivalent.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/
#ifndef PVAMRWBDECODER_BASIC_OP_CEQUIVALENT_H
#define PVAMRWBDECODER_BASIC_OP_CEQUIVALENT_H

#ifdef __cplusplus
extern "C"
{
#endif


#include "normalize_amr_wb.h"

#if defined(C_EQUIVALENT)


    /*----------------------------------------------------------------------------

         Function Name : add_int16

         Purpose :

          Performs the addition (var1+var2) with overflow control and saturation;
          the 16 bit result is set at +32767 when overflow occurs or at -32768
          when underflow occurs.

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
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var_out <= 0x0000 7fff.

     ----------------------------------------------------------------------------*/
    static inline int16 add_int16(int16 var1, int16 var2)
    {
        int32 L_sum;

        L_sum = (int32) var1 + var2;
        if ((L_sum >> 15) != (L_sum >> 31))
        {
            L_sum = (L_sum >> 31) ^ MAX_16;
        }
        return ((int16)(L_sum));
    }


    /*----------------------------------------------------------------------------

         Function Name : sub_int16

          Performs the subtraction (var1+var2) with overflow control and satu-
          ration; the 16 bit result is set at +32767 when overflow occurs or at
          -32768 when underflow occurs.

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
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var_out <= 0x0000 7fff.

     ----------------------------------------------------------------------------*/
    static inline int16 sub_int16(int16 var1, int16 var2)
    {
        int32 L_diff;

        L_diff = (int32) var1 - var2;
        if ((L_diff >> 15) != (L_diff >> 31))
        {
            L_diff = (L_diff >> 31) ^ MAX_16;
        }
        return ((int16)(L_diff));
    }


    /*----------------------------------------------------------------------------

         Function Name : mult_int16

          Performs the multiplication of var1 by var2 and gives a 16 bit result
          which is scaled i.e.:
                   mult_int16(var1,var2) = extract_l(L_shr((var1 times var2),15)) and
                   mult_int16(-32768,-32768) = 32767.

         Inputs :
          var1
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.

          var2
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.


         Return Value :
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var_out <= 0x0000 7fff.

     ----------------------------------------------------------------------------*/

    static inline int16 mult_int16(int16 var1, int16 var2)
    {
        int32 L_product;

        L_product = ((int32) var1 * (int32) var2) >> 15;

        if ((L_product >> 15) != (L_product >> 31))
        {
            L_product = (L_product >> 31) ^ MAX_16;
        }

        return ((int16)L_product);
    }


    /*----------------------------------------------------------------------------

         Function Name : add_int32

         32 bits addition of the two 32 bits variables (L_var1+L_var2) with
         overflow control and saturation; the result is set at +2147483647 when
         overflow occurs or at -2147483648 when underflow occurs.

         Inputs :

          L_var1   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.

          L_var2   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.


         Return Value :
          L_var_out
                   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

     ----------------------------------------------------------------------------*/


    static inline int32 add_int32(int32 L_var1, int32 L_var2)
    {
        int32 L_var_out;

        L_var_out = L_var1 + L_var2;

        if (((L_var1 ^ L_var2) & MIN_32) == 0)  /* same sign ? */
        {
            if ((L_var_out ^ L_var1) & MIN_32)  /* addition matches sign ? */
            {
                L_var_out = (L_var1 >> 31) ^ MAX_32;
            }
        }
        return (L_var_out);
    }




    /*----------------------------------------------------------------------------

         Function Name : sub_int32

         32 bits subtraction of the two 32 bits variables (L_var1-L_var2) with
         overflow control and saturation; the result is set at +2147483647 when
         overflow occurs or at -2147483648 when underflow occurs.

         Inputs :

          L_var1   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.

          L_var2   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.


         Return Value :
          L_var_out
                   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

     ----------------------------------------------------------------------------*/


    static inline int32 sub_int32(int32 L_var1, int32 L_var2)
    {
        int32 L_var_out;

        L_var_out = L_var1 - L_var2;

        if (((L_var1 ^ L_var2) & MIN_32) != 0)  /* different sign ? */
        {
            if ((L_var_out ^ L_var1) & MIN_32)  /* difference matches sign ? */
            {
                L_var_out = (L_var1 >> 31) ^ MAX_32;
            }
        }
        return (L_var_out);
    }



    /*----------------------------------------------------------------------------

         Function Name : mac_16by16_to_int32

         Multiply var1 by var2 and shift the result left by 1. Add the 32 bit
         result to L_var3 with saturation, return a 32 bit result:
              L_mac(L_var3,var1,var2) = L_add(L_var3,L_mult(var1,var2)).

         Inputs :

          L_var3   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.

          var1
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.

          var2
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.


         Return Value :
                   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

     ----------------------------------------------------------------------------*/


    static inline   int32 mac_16by16_to_int32(int32 L_var3, int16 var1, int16 var2)
    {
        int32 L_var_out;
        int32 L_mul;

        L_mul  = ((int32) var1 * (int32) var2);

        if (L_mul != 0x40000000)
        {
            L_mul <<= 1;
        }
        else
        {
            L_mul = MAX_32;     /* saturation */
        }

        L_var_out = L_var3 + L_mul;

        if (((L_mul ^ L_var3) & MIN_32) == 0)  /* same sign ? */
        {
            if ((L_var_out ^ L_var3) & MIN_32)  /* addition matches sign ? */
            {
                L_var_out = (L_var3 >> 31) ^ MAX_32;
            }
        }

        return (L_var_out);
    }



    /*----------------------------------------------------------------------------

         Function Name : msu_16by16_from_int32

         Multiply var1 by var2 and shift the result left by 1. Subtract the 32 bit
         result to L_var3 with saturation, return a 32 bit result:
              L_msu(L_var3,var1,var2) = L_sub(L_var3,L_mult(var1,var2)).

         Inputs :

          L_var3   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.

          var1
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.

          var2
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.


         Return Value :
                   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

     ----------------------------------------------------------------------------*/

    static inline int32 msu_16by16_from_int32(int32 L_var3, int16 var1, int16 var2)
    {
        int32 L_var_out;
        int32 L_mul;

        L_mul  = ((int32) var1 * (int32) var2);

        if (L_mul != 0x40000000)
        {
            L_mul <<= 1;
        }
        else
        {
            L_mul = MAX_32;     /* saturation */
        }

        L_var_out = L_var3 - L_mul;

        if (((L_mul ^ L_var3) & MIN_32) != 0)  /* different sign ? */
        {
            if ((L_var_out ^ L_var3) & MIN_32)  /* difference matches sign ? */
            {
                L_var_out = (L_var3 >> 31) ^ MAX_32;
            }
        }

        return (L_var_out);
    }


    /*----------------------------------------------------------------------------

         Function Name : mul_16by16_to_int32

         mul_16by16_to_int32 is the 32 bit result of the multiplication of var1
         times var2 with one shift left i.e.:
              L_mult(var1,var2) = L_shl((var1 times var2),1) and
              L_mult(-32768,-32768) = 2147483647.

         Inputs :
          var1
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.

          var2
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var1 <= 0x0000 7fff.

         Return Value :
                   32 bit long signed integer (int32) whose value falls in the
                   range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

     ----------------------------------------------------------------------------*/


    static inline int32 mul_16by16_to_int32(int16 var1, int16 var2)
    {
        int32 L_mul;

        L_mul  = ((int32) var1 * (int32) var2);

        if (L_mul != 0x40000000)
        {
            L_mul <<= 1;
        }
        else
        {
            L_mul = MAX_32;     /* saturation */
        }

        return (L_mul);

    }

    /*----------------------------------------------------------------------------

         Function Name : amr_wb_round

         Round the lower 16 bits of the 32 bit input number into the MS 16 bits
         with saturation. Shift the resulting bits right by 16 and return the 16
         bit number:
                     round(L_var1) = extract_h(L_add(L_var1,32768))

         Inputs :
          L_var1
                   32 bit long signed integer (int32 ) whose value falls in the
                   range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

         Return Value :
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var_out <= 0x0000 7fff.

     ----------------------------------------------------------------------------*/
    static inline int16 amr_wb_round(int32 L_var1)
    {
        if (L_var1 != MAX_32)
        {
            L_var1 +=  0x00008000L;
        }
        return ((int16)(L_var1 >> 16));
    }


    /*----------------------------------------------------------------------------

         Function Name : amr_wb_shl1_round

         Shift the 32 bit input number to the left by 1, round up the result and
         shift down by 16
                     amr_wb_shl1_round(L_var1) = round(L_shl(L_var1,1))

         Inputs :
          L_var1
                   32 bit long signed integer (int32 ) whose value falls in the
                   range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

         Return Value :
                   16 bit short signed integer (int16) whose value falls in the
                   range : 0xffff 8000 <= var_out <= 0x0000 7fff.

     ----------------------------------------------------------------------------*/
    static inline int16 amr_wb_shl1_round(int32 L_var1)
    {
        int16 var_out;

        if ((L_var1 << 1) >> 1 == L_var1)
        {
            var_out = (int16)((L_var1 + 0x00004000) >> 15);
        }
        else
        {
            var_out = (int16)(((L_var1 >> 31) ^ MAX_32) >> 16);
        }

        return (var_out);
    }

    /*----------------------------------------------------------------------------
             Function Name : mul_32by16

             Multiply a 16 bit integer by a 32 bit (DPF). The result is divided
             by 2^15

                    L_32 = (hi1*lo2)<<1 + ((lo1*lo2)>>15)<<1

             Inputs :

             hi          hi part of 32 bit number.
             lo          lo part of 32 bit number.
             n           16 bit number.

         ----------------------------------------------------------------------------*/


    static inline  int32 mul_32by16(int16 hi, int16 lo, int16 n)
    {
        return (((((int32)hi*n)) + ((((int32)lo*n) >> 15))) << 1);
    }

    static inline   int32 fxp_mac_16by16(int16 var1,  int16 var2, int32 L_add)
    {

        L_add += (int32)var1 * var2;

        return L_add;
    }

    static inline   int32 fxp_mul_16by16(int16 var1, const int16 var2)
    {
        int32 L_mul = (int32)var1 * var2;

        return L_mul;
    }

    static inline   int32 fxp_mul32_by_16b(int32 L_var1, const int32 L_var2)
    {
        int32 L_mul = (int32)(((int64)L_var1 * (L_var2 << 16)) >> 32);

        return L_mul;
    }


#ifdef __cplusplus
}
#endif

#endif

#endif   /*  PVAMRWBDECODER_BASIC_OP_CEQUIVALENT_H  */

