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
#include "get_const_tbls.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    extern const Word16 dgray[];
    extern const Word16 dico1_lsf_3[];
    extern const Word16 dico1_lsf_5[];
    extern const Word16 dico2_lsf_3[];
    extern const Word16 dico2_lsf_5[];
    extern const Word16 dico3_lsf_3[];
    extern const Word16 dico3_lsf_5[];
    extern const Word16 dico4_lsf_5[];
    extern const Word16 dico5_lsf_5[];
    extern const Word16 gray[];
    extern const Word16 lsp_init_data[];
    extern const Word16 mean_lsf_3[];
    extern const Word16 mean_lsf_5[];
    extern const Word16 mr515_3_lsf[];
    extern const Word16 mr795_1_lsf[];
    extern const Word16 past_rq_init[];
    extern const Word16 pred_fac_3[];
    extern const Word16 qua_gain_code[];
    extern const Word16 qua_gain_pitch[];
    extern const Word16 startPos[];
    extern const Word16 table_gain_lowrates[];
    extern const Word16 table_gain_highrates[];
    extern const Word16 prmno[];
    extern const Word16* const bitno[];
    extern const Word16 numOfBits[];
    extern const Word16* const reorderBits[];
    extern const Word16 numCompressedBytes[];
    extern const Word16 window_200_40[];
    extern const Word16 window_160_80[];
    extern const Word16 window_232_8[];
    extern const Word16 ph_imp_low_MR795[];
    extern const Word16 ph_imp_mid_MR795[];
    extern const Word16 ph_imp_low[];
    extern const Word16 ph_imp_mid[];

#ifdef __cplusplus
}
#endif

OSCL_EXPORT_REF void get_const_tbls(CommonAmrTbls* tbl_struct_ptr)
{
    tbl_struct_ptr->dgray_ptr = dgray;
    tbl_struct_ptr->dico1_lsf_3_ptr = dico1_lsf_3;
    tbl_struct_ptr->dico1_lsf_5_ptr = dico1_lsf_5;
    tbl_struct_ptr->dico2_lsf_3_ptr = dico2_lsf_3;
    tbl_struct_ptr->dico2_lsf_5_ptr = dico2_lsf_5;
    tbl_struct_ptr->dico3_lsf_3_ptr = dico3_lsf_3;
    tbl_struct_ptr->dico3_lsf_5_ptr = dico3_lsf_5;
    tbl_struct_ptr->dico4_lsf_5_ptr = dico4_lsf_5;
    tbl_struct_ptr->dico5_lsf_5_ptr = dico5_lsf_5;
    tbl_struct_ptr->gray_ptr = gray;
    tbl_struct_ptr->lsp_init_data_ptr = lsp_init_data;
    tbl_struct_ptr->mean_lsf_3_ptr = mean_lsf_3;
    tbl_struct_ptr->mean_lsf_5_ptr = mean_lsf_5;
    tbl_struct_ptr->mr515_3_lsf_ptr = mr515_3_lsf;
    tbl_struct_ptr->mr795_1_lsf_ptr = mr795_1_lsf;
    tbl_struct_ptr->past_rq_init_ptr = past_rq_init;
    tbl_struct_ptr->pred_fac_3_ptr = pred_fac_3;
    tbl_struct_ptr->qua_gain_code_ptr = qua_gain_code;
    tbl_struct_ptr->qua_gain_pitch_ptr = qua_gain_pitch;
    tbl_struct_ptr->startPos_ptr = startPos;
    tbl_struct_ptr->table_gain_lowrates_ptr = table_gain_lowrates;
    tbl_struct_ptr->table_gain_highrates_ptr = table_gain_highrates;
    tbl_struct_ptr->prmno_ptr = prmno;
    tbl_struct_ptr->bitno_ptr = bitno;
    tbl_struct_ptr->numOfBits_ptr = numOfBits;
    tbl_struct_ptr->reorderBits_ptr = reorderBits;
    tbl_struct_ptr->numCompressedBytes_ptr = numCompressedBytes;
    tbl_struct_ptr->window_200_40_ptr = window_200_40;
    tbl_struct_ptr->window_160_80_ptr = window_160_80;
    tbl_struct_ptr->window_232_8_ptr = window_232_8;
    tbl_struct_ptr->ph_imp_low_MR795_ptr = ph_imp_low_MR795;
    tbl_struct_ptr->ph_imp_mid_MR795_ptr = ph_imp_mid_MR795;
    tbl_struct_ptr->ph_imp_low_ptr = ph_imp_low;
    tbl_struct_ptr->ph_imp_mid_ptr = ph_imp_mid;
}
