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



 Pathname: ./src/pvamrwbdecoder_basic_op.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/


#ifndef PVAMRWBDECODER_BASIC_OP_H
#define PVAMRWBDECODER_BASIC_OP_H


#include "normalize_amr_wb.h"


#define MAX_32 (int32)0x7fffffffL
#define MIN_32 (int32)0x80000000L

#define MAX_16 (int16)+32767    /* 0x7fff */
#define MIN_16 (int16)-32768    /* 0x8000 */




/*----------------------------------------------------------------------------
     Function Name : negate_int16

     Negate var1 with saturation, saturate in the case where input is -32768:
                  negate(var1) = sub(0,var1).

     Inputs :
      var1
               16 bit short signed integer (int16) whose value falls in the
               range : 0x8000 <= var1 <= 0x7fff.

     Outputs :
      none

     Return Value :
               16 bit short signed integer (int16) whose value falls in the
               range : 0x8000 <= var_out <= 0x7fff.
 ----------------------------------------------------------------------------*/

static inline  int16 negate_int16(int16 var1)
{
    return (((var1 == MIN_16) ? MAX_16 : -var1));
}


/*----------------------------------------------------------------------------

     Function Name : shl_int16

     Arithmetically shift the 16 bit input var1 left var2 positions.Zero fill
     the var2 LSB of the result. If var2 is negative, arithmetically shift
     var1 right by -var2 with sign extension. Saturate the result in case of
     underflows or overflows.

     Inputs :
      var1
               16 bit short signed integer (int16) whose value falls in the
               range : 0x8000 <= var1 <= 0x7fff.

      var2
               16 bit short signed integer (int16) whose value falls in the
               range : 0x8000 <= var1 <= 0x7fff.

     Return Value :
      var_out
               16 bit short signed integer (int16) whose value falls in the
               range : 0x8000 <= var_out <= 0x7fff.
 ----------------------------------------------------------------------------*/

static inline  int16 shl_int16(int16 var1, int16 var2)
{
    int16 var_out;

    if (var2 < 0)
    {
        var2 = (-var2) & (0xf);
        var_out = var1 >> var2;
    }
    else
    {
        var2 &= 0xf;
        var_out = var1 << var2;
        if (var_out >> var2 != var1)
        {
            var_out = (var1 >> 15) ^ MAX_16;
        }
    }
    return (var_out);
}


/*----------------------------------------------------------------------------

     Function Name : shl_int32

     Arithmetically shift the 32 bit input L_var1 left var2 positions. Zero
     fill the var2 LSB of the result. If var2 is negative, arithmetically
     shift L_var1 right by -var2 with sign extension. Saturate the result in
     case of underflows or overflows.

     Inputs :
      L_var1   32 bit long signed integer (int32) whose value falls in the
               range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

      var2
               16 bit short signed integer (int16) whose value falls in the
               range :  8000 <= var2 <= 7fff.
     Return Value :
               32 bit long signed integer (int32) whose value falls in the
               range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

 ----------------------------------------------------------------------------*/

static inline  int32 shl_int32(int32 L_var1, int16 var2)
{
    int32 L_var_out;

    if (var2 > 0)
    {
        L_var_out = L_var1 << var2;
        if (L_var_out >> var2 != L_var1)
        {
            L_var_out = (L_var1 >> 31) ^ MAX_32;
        }
    }
    else
    {
        var2 = (-var2) & (0xf);
        L_var_out = L_var1 >> var2;
    }

    return (L_var_out);
}


/*----------------------------------------------------------------------------

     Function Name : shr_int32

     Arithmetically shift the 32 bit input L_var1 right var2 positions with
     sign extension. If var2 is negative, arithmetically shift L_var1 left
     by -var2 and zero fill the -var2 LSB of the result. Saturate the result
     in case of underflows or overflows.

     Inputs :
      L_var1   32 bit long signed integer (int32) whose value falls in the
               range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

      var2
               16 bit short signed integer (int16) whose value falls in the
               range :  8000 <= var2 <= 7fff.
     Return Value :
               32 bit long signed integer (int32) whose value falls in the
               range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.

 ----------------------------------------------------------------------------*/

static inline  int32 shr_int32(int32 L_var1, int16 var2)
{
    int32 L_var_out;

    if (var2 >= 0)
    {
        L_var_out = L_var1 >> (var2 & 0x1f);
    }
    else
    {
        var2 = (int16)(-var2);
        var2 &= 0x1f;
        L_var_out = L_var1 << var2;
        if (L_var_out >> var2 != L_var1)
        {
            L_var_out = (L_var1 >> 31) ^ MAX_32;
        }

    }
    return (L_var_out);
}






#if   ((PV_CPU_ARCH_VERSION >=5) && (PV_COMPILER == EPV_ARM_GNUC))

#include "pvamrwbdecoder_basic_op_gcc_armv5.h"

#else

#ifndef C_EQUIVALENT
#define C_EQUIVALENT        // default to C_EQUIVALENT
#endif

#include "pvamrwbdecoder_basic_op_cequivalent.h"

#endif


#endif   /*  PVAMRWBDECODER_BASIC_OP_H  */

