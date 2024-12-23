/* $Id: d7752e.h,v 1.1 2004/03/16 14:40:31 cisc Exp $ */

/*
 * D7752 用サービスモジュール
 *
 * Copyright (c) 2004 cisc.
 * All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef incl_D7752e_h
#define incl_D7752e_h

#include "d7752.h"

#ifdef __cplusplus
extern "C" 
{
#endif

typedef struct D7752e_t D7752e;

D7752e* D7752e_Open();
int D7752e_Close(D7752e* inst);

int D7752e_SetRate(D7752e* inst, int chiprate, int pcmrate);
int D7752e_SetBufferLength(D7752e* inst, int length);
int D7752e_Synth(D7752e* inst, D7752_SAMPLE* samples, int length);

int D7752e_SetCommand(D7752e* inst, unsigned char command);
int D7752e_SetMode(D7752e* inst, unsigned char mode);
int D7752e_SetData(D7752e* inst, unsigned char data);
int D7752e_GetStatus(D7752e* inst);

#define D7752E_BSY      (0x80)    /* b7 BSY - 音声合成中なら 1 */
#define D7752E_REQ      (0x40)    /* b6 REQ - 音声パラメータバッファに余裕があれば 1 */
#define D7752E_EXT      (0x20)    /* b5 INT/EXT - 1 */
#define D7752E_ERR      (0x10)    /* b4 ERR - 転送エラーがあった場合 1 */

#ifdef __cplusplus
}
#endif

#endif // incl_D7752e_h
