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



 Pathname: ./src/pvamrwbdecoder_basic_op_gcc_armv5.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef PVAMRWBDECODER_BASIC_OP_GCC_ARMV5_H
#define PVAMRWBDECODER_BASIC_OP_GCC_ARMV5_H


#ifdef __cplusplus
extern "C"
{
#endif


    static inline int16 sub_int16(int16 var1, int16 var2)
    {
        register int32 L_var_out;
        register int32 L_var_aux;
        register int32 ra = (int32)var1;
        register int32 rb = (int32)var2;

        asm volatile(
            "mov  %0, %2, lsl #16\n"
            "mov  %1, %3, lsl #16\n"
            "qsub %0, %0, %1\n"
            "mov  %0, %0, asr #16"
    : "=&r*i"(L_var_out),
            "=&r*i"(L_var_aux)
                    : "r"(ra),
                    "r"(rb));

        return (int16)L_var_out;

    }

    static inline int16 add_int16(int16 var1, int16 var2)
{
        register int32 L_var_out;
        register int32 L_var_aux;
        register int32 ra = (int32)var1;
        register int32 rb = (int32)var2;

        asm volatile(
            "mov  %0, %2, lsl #16\n"
            "mov  %1, %3, lsl #16\n"
            "qadd %0, %0, %1\n"
            "mov  %0, %0, asr #16"
    : "=&r*i"(L_var_out),
            "=&r*i"(L_var_aux)
                    : "r"(ra),
                    "r"(rb));

        return (int16)L_var_out;

    }

    static inline  int32 mul_32by16(int16 hi, int16 lo, int16 n)
{
        register int32 H_32;
        register int32 L_32;
        register int32 ra = (int32)hi;
        register int32 rb = (int32)lo;
        register int32 rc = (int32)n;


        asm volatile(
            "smulbb %0, %2, %4\n"
            "smulbb %1, %3, %4\n"
            "add    %0, %0, %1, asr #15\n"
            "qadd   %0, %0, %0"
    : "=&r*i"(H_32),
            "=&r*i"(L_32)
                    : "r"(ra),
                    "r"(rb),
                    "r"(rc));

        return H_32;
    }


    static inline int32 sub_int32(int32 L_var1, int32 L_var2)
{
        register int32 L_var_out;
        register int32 ra = L_var1;
        register int32 rb = L_var2;

        asm volatile(
            "qsub %0, %1, %2"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb));

        return L_var_out;
    }

    static inline int32 add_int32(int32 L_var1, int32 L_var2)
{
        register int32 L_var_out;
        register int32 ra = L_var1;
        register int32 rb = L_var2;

        asm volatile(
            "qadd %0, %1, %2"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb));

        return L_var_out;
    }

    static inline int32 msu_16by16_from_int32(int32 L_var3, int16 var1, int16 var2)
{
        register int32 L_var_out;
        register int32 ra = (int32)var1;
        register int32 rb = (int32)var2;
        register int32 rc = L_var3;

        asm volatile(
            "smulbb %0, %1, %2\n"
            "qdsub %0, %3, %0"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb),
                    "r"(rc));

        return L_var_out;
    }


    static inline int32 mac_16by16_to_int32(int32 L_var3, int16 var1, int16 var2)
{
        register int32 L_var_out;
        register int32 ra = (int32)var1;
        register int32 rb = (int32)var2;
        register int32 rc = L_var3;

        asm volatile(
            "smulbb %0, %1, %2\n"
            "qdadd %0, %3, %0"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb),
                    "r"(rc));

        return L_var_out;
    }


    static inline  int32 mul_16by16_to_int32(int16 var1, int16 var2)
{
        register int32 L_var_out;
        register int32 ra = (int32)var1;
        register int32 rb = (int32)var2;

        asm volatile(
            "smulbb %0, %1, %2\n"
            "qadd %0, %0, %0"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb));

        return L_var_out;
    }


    static inline int16 mult_int16(int16 var1, int16 var2)
{
        register int32 L_var_out;
        register int32 ra = (int32)var1;
        register int32 rb = (int32)var2;

        asm volatile(
            "smulbb %0, %1, %2\n"
            "mov %0, %0, asr #15"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb));

        return (int16)L_var_out;
    }

    static inline int16 amr_wb_round(int32 L_var1)
{
        register int32 L_var_out;
        register int32 ra = (int32)L_var1;
        register int32 rb = (int32)0x00008000L;

        asm volatile(
            "qadd %0, %1, %2\n"
            "mov %0, %0, asr #16"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb));
        return (int16)L_var_out;
    }

    static inline int16 amr_wb_shl1_round(int32 L_var1)
{
        register int32 L_var_out;
        register int32 ra = (int32)L_var1;
        register int32 rb = (int32)0x00008000L;

        asm volatile(
            "qadd %0, %1, %1\n"
            "qadd %0, %0, %2\n"
            "mov %0, %0, asr #16"
    : "=&r*i"(L_var_out)
                    : "r"(ra),
                    "r"(rb));
        return (int16)L_var_out;
    }


    static inline int32 fxp_mac_16by16(const int16 L_var1, const int16 L_var2, int32 L_add)
{
        register int32 tmp;
        register int32 ra = (int32)L_var1;
        register int32 rb = (int32)L_var2;
        register int32 rc = (int32)L_add;

        asm volatile(
            "smlabb %0, %1, %2, %3"
    : "=&r*i"(tmp)
                    : "r"(ra),
                    "r"(rb),
                    "r"(rc));
        return (tmp);
    }

    static inline int32 fxp_mul_16by16bb(int16 L_var1, const int16 L_var2)
{
        register int32 tmp;
        register int32 ra = (int32)L_var1;
        register int32 rb = (int32)L_var2;

        asm volatile(
            "smulbb %0, %1, %2"
    : "=&r*i"(tmp)
                    : "r"(ra),
                    "r"(rb));
        return (tmp);
    }


#define fxp_mul_16by16(a, b)  fxp_mul_16by16bb(  a, b)


    static inline int32 fxp_mul32_by_16(int32 L_var1, const int32 L_var2)
{
        register int32 tmp;
        register int32 ra = (int32)L_var1;
        register int32 rb = (int32)L_var2;

        asm volatile(
            "smulwb %0, %1, %2"
    : "=&r*i"(tmp)
                    : "r"(ra),
                    "r"(rb));
        return (tmp);
    }

#define fxp_mul32_by_16b( a, b)   fxp_mul32_by_16( a, b)



#ifdef __cplusplus
}
#endif




#endif   /*  PVAMRWBDECODER_BASIC_OP_GCC_ARMV5_H  */

