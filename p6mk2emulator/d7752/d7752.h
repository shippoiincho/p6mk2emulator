/* $Id: d7752.h,v 1.4 2004/03/16 14:40:31 cisc Exp $ */

/*
 * É PD7752 ïóñ° âπê∫çáê¨ÉGÉìÉWÉì
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

#ifndef incl_D7752_h
#define incl_D7752_h

#ifdef __cplusplus
extern "C" 
{
#endif

typedef int D7752_SAMPLE;
typedef int D7752_FIXED;

#define D7752_ERR_SUCCESS		0
#define D7752_ERR_PARAM			-1

#define D7752_ERR_DEVICE_MODE   -2
#define D7752_ERR_MEMORY        -3
#define D7752_ERR_BUFFER_FULL   -4
#define D7752_ERR_BUFFER_EMPTY  -5

typedef struct D7752_t D7752;

D7752* D7752_Open();

int D7752_Close(D7752* inst);
int D7752_Start(D7752* inst, int mode);
int D7752_Synth(D7752* inst, unsigned char* data, D7752_SAMPLE* frame);
int D7752_GetFrameSize(D7752* inst);

#ifdef __cplusplus
}
#endif

#endif // incl_D7752_h

