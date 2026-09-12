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



 Filename: get_amr_wb_bits.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 no_of_bits,        input : number of bits
     int16 ** prms            bitstream pointer


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Returns no_of_bits from serial bit stream
    Serial_parm -> convert serial stream to parameters

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
#include "pvamrwbdecoder_cnst.h"
#include "get_amr_wb_bits.h"

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


int16 Serial_parm(                        /* Return the parameter    */
    int16 no_of_bits,                    /* input : number of bits  */
    int16 ** prms
)
{
    int16 value = 0;

    for (int16 i = no_of_bits >> 1; i != 0; i--)
    {
        value <<= 2;

        if (*((*prms)++) == BIT_1)
        {
            value |= 2;
        }

        if (*((*prms)++) == BIT_1)
        {
            value |= 1;
        }

    }

    if (no_of_bits&1)
    {
        value <<= 1;

        if (*((*prms)++) == BIT_1)
        {
            value |= 1;
        }

    }

    return (value);
}


int16 Serial_parm_1bit(int16 ** prms)                 /* Return the parameter    */
{
    int16 value = 0;

    if (*((*prms)++) == BIT_1)
    {
        value = 1;
    }
    return (value);
}
