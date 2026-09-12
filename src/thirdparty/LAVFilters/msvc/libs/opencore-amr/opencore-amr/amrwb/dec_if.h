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

#ifndef OPENCORE_AMRWB_DEC_IF_H
#define OPENCORE_AMRWB_DEC_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#define _good_frame 0

void* D_IF_init(void);
void D_IF_decode(void* state, const unsigned char* bits, short* synth, int bfi);
void D_IF_exit(void* state);

#ifdef __cplusplus
}
#endif

#endif
