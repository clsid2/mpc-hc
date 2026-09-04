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
#ifndef GET_CONST_TBLS_H
#define GET_CONST_TBLS_H

#include "typedef.h"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        const Word16* dgray_ptr;
        const Word16* dico1_lsf_3_ptr;
        const Word16* dico1_lsf_5_ptr;
        const Word16* dico2_lsf_3_ptr;
        const Word16* dico2_lsf_5_ptr;
        const Word16* dico3_lsf_3_ptr;
        const Word16* dico3_lsf_5_ptr;
        const Word16* dico4_lsf_5_ptr;
        const Word16* dico5_lsf_5_ptr;
        const Word16* gray_ptr;
        const Word16* lsp_init_data_ptr;
        const Word16* mean_lsf_3_ptr;
        const Word16* mean_lsf_5_ptr;
        const Word16* mr515_3_lsf_ptr;
        const Word16* mr795_1_lsf_ptr;
        const Word16* past_rq_init_ptr;
        const Word16* pred_fac_3_ptr;
        const Word16* qua_gain_code_ptr;
        const Word16* qua_gain_pitch_ptr;
        const Word16* startPos_ptr;
        const Word16* table_gain_lowrates_ptr;
        const Word16* table_gain_highrates_ptr;
        const Word16* prmno_ptr;
        const Word16* const* bitno_ptr;
        const Word16* numOfBits_ptr;
        const Word16* const* reorderBits_ptr;
        const Word16* numCompressedBytes_ptr;
        const Word16* window_200_40_ptr;
        const Word16* window_160_80_ptr;
        const Word16* window_232_8_ptr;
        const Word16* ph_imp_low_MR795_ptr;
        const Word16* ph_imp_mid_MR795_ptr;
        const Word16* ph_imp_low_ptr;
        const Word16* ph_imp_mid_ptr;
    } CommonAmrTbls;

    OSCL_IMPORT_REF void get_const_tbls(CommonAmrTbls* tbl_struct_ptr);


#ifdef __cplusplus
}
#endif

#endif
