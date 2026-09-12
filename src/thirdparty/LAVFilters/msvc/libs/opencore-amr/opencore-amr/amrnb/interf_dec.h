/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
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

#ifndef OPENCORE_AMRNB_INTERF_DEC_H
#define OPENCORE_AMRNB_INTERF_DEC_H

#ifdef __cplusplus
extern "C" {
#endif

void* Decoder_Interface_init(void);
void Decoder_Interface_exit(void* state);
void Decoder_Interface_Decode(void* state, const unsigned char* in, short* out, int bfi);

#ifdef __cplusplus
}
#endif

#endif
