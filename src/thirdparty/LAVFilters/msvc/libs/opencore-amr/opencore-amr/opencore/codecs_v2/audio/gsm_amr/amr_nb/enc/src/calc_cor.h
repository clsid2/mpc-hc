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
********************************************************************************
*
*      GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001
*                                R99   Version 3.2.0
*                                REL-4 Version 4.0.0
*
********************************************************************************
*
*      File             : calc_cor.h
*      Purpose          : Calculate all correlations for prior the OL LTP
*
********************************************************************************
*/
#ifndef calc_cor_h
#define calc_cor_h "$Id $"

/*
********************************************************************************
*                         INCLUDE FILES
********************************************************************************
*/
#include "typedef.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    ********************************************************************************
    *                         DECLARATION OF PROTOTYPES
    ********************************************************************************
    */
    /*************************************************************************
     *
     *  FUNCTION: comp_corr
     *
     *  PURPOSE: Calculate all correlations of scal_sig[] in a given delay
     *           range.
     *
     *  DESCRIPTION:
     *      The correlation is given by
     *           cor[t] = <scal_sig[n], scal_sig[n-t]>,  t=lag_min,...,lag_max
     *      The functions outputs all correlations in the given range
     *
     *************************************************************************/
    void comp_corr(Word16 scal_sig[],   /* i   : scaled signal.                     */
    Word16 L_frame,     /* i   : length of frame to compute pitch   */
    Word16 lag_max,     /* i   : maximum lag                        */
    Word16 lag_min,     /* i   : minimum lag                        */
    Word32 corr[]       /* o   : correlation of selected lag        */
                  );

#ifdef __cplusplus
}
#endif

#endif
