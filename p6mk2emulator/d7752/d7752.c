/* $Id: d7752.c,v 1.3 2004/02/25 12:25:58 cisc Exp $ */

/*
 * ��PD7752 ���� ���������G���W��
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "d7752.h"

/* internal macros */
#define ASSERT_D7752(inst)	\
	assert(inst);	

#define CHECK_D7752(inst)	\
	if (inst == 0) return D7752_ERR_PARAM; \
	ASSERT_D7752(inst);

#define I2F(a) (((D7752_FIXED) a) << 16)
#define F2I(a) ((int)((a) >> 16))


/*
 * �t�B���^�W��
 */
struct D7752Coef_t
{
	D7752_FIXED	f[5];
	D7752_FIXED	b[5];
	D7752_FIXED	amp;
	D7752_FIXED pitch;
};
typedef struct D7752Coef_t D7752Coef;

/*
 * �{�C�X
 */
struct D7752_t
{
	D7752Coef coef;
	int y[5][2];
	int pitch_count;
	int frame_size;
};
typedef struct D7752_t D7752;

static void d7752_printparam(char* s, D7752Coef* p);


/* �U���W�J�e�[�u�� */
const static int amp_table[16] = 
{
	  0, 1, 1, 2, 3, 4, 5, 7, 9, 13, 17, 23, 31, 42, 56, 75
};

/* ��̃t�B���^�W�� (uPD7752����) */
const static int iir1[128] = 
{
	 11424, 11400, 11377, 11331, 11285, 11265, 11195, 11149, 
	 11082, 11014, 10922, 10830, 10741, 10629, 10491, 10332, 
	 10172,  9992,  9788,  9560,  9311,  9037,  8721,  8377, 
	  8016,  7631,  7199,  6720,  6245,  5721,  5197,  4654, 
	-11245,-11200,-11131,-11064,-10995,-10905,-10813,-10700, 
	-10585,-10447,-10291,-10108, -9924, -9722, -9470, -9223, 
	 -8928, -8609, -8247, -7881, -7472, -7019, -6566, -6068, 
	 -5545, -4999, -4452, -3902, -3363, -2844, -2316, -1864, 
	 11585, 11561, 11561, 11561, 11561, 11540, 11540, 11540, 
	 11515, 11515, 11494, 11470, 11470, 11448, 11424, 11400, 
	 11377, 11356, 11307, 11285, 11241, 11195, 11149, 11082, 
	 11014, 10943, 10874, 10784, 10695, 10583, 10468, 10332, 
	 10193, 10013,  9833,  9628,  9399,  9155,  8876,  8584, 
	  8218,  7857,  7445,  6970,  6472,  5925,  5314,  4654, 
	  3948,  3178,  2312,  1429,   450,  -543, -1614, -2729, 
	 -3883, -5066, -6250, -7404, -8500, -9497,-10359,-11038, 
};

const static int iir2[64] = 
{
	 8192, 8105, 7989, 7844, 7670, 7424, 7158, 6803, 
	 6370, 5860, 5252, 4579, 3824, 3057, 2307, 1623, 
	 8193, 8100, 7990, 7844, 7672, 7423, 7158, 6802, 
	 6371, 5857, 5253, 4576, 3825, 3057, 2309, 1617, 
	-6739,-6476,-6141,-5738,-5225,-4604,-3872,-2975, 
	-1930, -706,  686, 2224, 3871, 5518, 6992, 8085, 
	-6746,-6481,-6140,-5738,-5228,-4602,-3873,-2972, 
	-1931, -705,  685, 2228, 3870, 5516, 6993, 8084  
};


/*
 * �{�C�X�W�F�l���[�^�̃C���X�^���X���쐬���܂��B
 * ����:	�Ȃ�
 * �Ԓl:	�{�C�X�W�F�l���[�^�̃C���X�^���X
 */
D7752* D7752_Open()
{
	D7752* inst = (D7752*) malloc(sizeof(D7752));
	if (!inst)
	{
		return NULL;
	}
	
	if (D7752_Start(inst, 0) < 0)
	{
		D7752_Close(inst);
		return NULL;
	}
	return inst;
}

/*
 * �{�C�X�W�F�l���[�^�̃C���X�^���X��j�����܂��B
 * ����:	inst	�{�C�X�W�F�l���[�^�̃C���X�^���X
 * �Ԓl:	int		�G���[�R�[�h
 */
int D7752_Close(D7752* inst)
{
	if (inst)
	{
		ASSERT_D7752(inst);
		free(inst);
	}
	return D7752_ERR_SUCCESS;
}

/*
 * �����������J�n���܂��B
 * ����:	inst	�{�C�X�W�F�l���[�^�̃C���X�^���X
 *			mode	���������̃��[�h���w�肵�܂�
 *					b2 F: ���̓t���[�� BIT
 *						F=0 10ms/�t���[��
 *						F=1 20ms/�t���[��
 *					b1-0 S: �������x BIT
 *						S=00: NORMAL SPEED
 *						S=01: SLOW SPEED 
 *						S=10: FAST SPEED
 * �Ԓl:	int		�G���[�R�[�h
 */
int D7752_Start(D7752* inst, int mode)
{
	int i;
	const static int frame_size[8] = 
	{
		100,		 /* 10ms, NORMAL */
		120,		 /* 10ms, SLOW  */
		 80,		 /* 10ms, FAST */
		100,		 /* PROHIBITED */
		200,		 /* 20ms, NORMAL */
		240,		 /* 20ms, SLOW */
		160,		 /* 20ms, FAST */
		200,		 /* PROHIBITED */
	};
	
	/* �t�B���^�p�����[�^�̏����l */
	const static int f_default[5] =
	{
		126, 64, 121, 111, 96,
	};
	const static int b_default[5] =
	{
		9, 4, 9, 9, 11,
	};

	CHECK_D7752(inst);

	/* �p�����[�^�ϐ��̏����� */
	inst->frame_size = frame_size[mode & 7];
	
	for (i=0; i<5; i++)
	{
		inst->y[i][0] = 0;
		inst->y[i][1] = 0;
		inst->coef.f[i] = I2F(f_default[i]);
		inst->coef.b[i] = I2F(b_default[i]);
	}
	inst->pitch_count = 0;
	inst->coef.amp = 0;
	inst->coef.pitch = I2F(30);
    
	return D7752_ERR_SUCCESS;
}

/*
 *	����������1�t���[�����̒��������߂܂��B
 *	�t���[�����́AD7752_Start ���Ăяo�������_�Őݒ肳��܂��B
 *	����:	inst	�{�C�X�W�F�l���[�^�̃C���X�^���X
 *	�Ԓl:	int		�t���[���T�C�Y
 */
int D7752_GetFrameSize(D7752* inst)
{
	CHECK_D7752(inst);
	return inst->frame_size;
}

/*
 * 1�t���[�����̉������������܂��B
 * D7752 �{�C�X�W�F�l���[�^�̃C���X�^���X��j�����܂��B
 * ����:	inst	�{�C�X�W�F�l���[�^�̃C���X�^���X
 *			frame	���������g�`�f�[�^�̊i�[��B1�t���[�����̌�
 * �Ԓl:	int		�G���[�R�[�h
 */
int D7752_Synth(D7752* inst, unsigned char* param, D7752_SAMPLE* frame)
{
	int i, p;
	int vu;
	int qmag;
	D7752Coef* curr;
	D7752Coef incr, next;

	CHECK_D7752(inst);
	if (!param || !frame)
		return D7752_ERR_PARAM;

	curr = &inst->coef;

	/* �܂��A�p�����[�^���W���ɓW�J���� */
	qmag = (param[0] & 4) != 0 ? 1 : 0;

	for (i=0; i<5; i++)
	{
		int f, b;
		f = (param[i+1] >> 3) & 31;
		if (f & 16) 
		{
			f -= 32;
		}
		next.f[i] = curr->f[i] + I2F(f << qmag);

		b = param[i+1] & 7;
		if (b & 4) 
		{
			b -= 8;
		}
		next.b[i] = curr->b[i] + I2F(b << qmag);
	}

	next.amp = I2F((param[6] >> 4) & 15);

	p = param[6] & 7;
	if (p & 4)
	{
		p -= 8;
	}
//	next.pitch = curr->pitch + I2F(p);
	next.pitch = I2F(((F2I(curr->pitch) + p - 1) & 0x7f) + 1);

	/* ���`�⊮�p�̑����l�̌v�Z */
	incr.amp = (next.amp - curr->amp) / inst->frame_size;
	incr.pitch = (next.pitch - curr->pitch) / inst->frame_size;
	for (i=0; i<5; i++)
	{
		incr.b[i] = (next.b[i] - curr->b[i]) / inst->frame_size;
		incr.f[i] = (next.f[i] - curr->f[i]) / inst->frame_size;
	}

	/* �C���p���X�E�m�C�Y�̔����L�����`�F�b�N */
	vu  = param[0] & 1 ? 1 : 2;
//	vu |= param[6] & 4 ? 3 : 0;
	vu |= param[6] & 8 ? 3 : 0;

	/* �������� */
	for (i=0; i<inst->frame_size; i++)
	{
		int y;
		int j;
		int c;
		
		y = 0;
		
		/* �C���p���X���� */
		c = F2I(curr->pitch);
//		if (inst->pitch_count > (c > 0 ? c : 128))
		if (inst->pitch_count >= c)
		{
			if (vu & 1)
			{
				y = amp_table[F2I(curr->amp)] * 16 - 1;
			}
			inst->pitch_count = 0;
		}
		inst->pitch_count++;

		/* �m�C�Y���� */
		if (vu & 2)
		{
			if (rand() & 1)
				y += amp_table[F2I(curr->amp)] * 4 - 1;		 /* XXX �m�C�Y�ڍוs�� */
		}

		/* ��̃f�B�W�^���t�B���^ */
		for (j=0; j<5; j++)
		{
			int t;
			t  = inst->y[j][0] * iir1[F2I(curr->f[j]) & 0x7f] / 8192;
			y += t             * iir1[(F2I(curr->b[j]) * 2 + 1) & 0x7f] / 8192;
			y -= inst->y[j][1] * iir2[F2I(curr->b[j]) & 0x3f] / 8192;
			
			y = y > 8191 ? 8191 : y < -8192 ? -8192 : y;
			
			inst->y[j][1] = inst->y[j][0];
			inst->y[j][0] = y;
		}

		/* �f�[�^��ۑ� */
		*frame++ = y;

		/* �p�����[�^�𑝕� */
		curr->amp += incr.amp;
		curr->pitch += incr.pitch;
		for (j=0; j<5; j++)
		{
			curr->b[j] += incr.b[j];
			curr->f[j] += incr.f[j];
		}
	}

	/* �p�����[�^���V�t�g���� */
	memcpy(curr, &next, sizeof(D7752Coef));
	
	return D7752_ERR_SUCCESS;
}

/*
 *	for debug
 */
static void d7752_printparam(char* s, D7752Coef* p)
{
	int i;
	fprintf(stderr, "%s\n", s);
	fprintf(stderr, "\tAMP: %x  PITCH: %x\n", p->amp, p->pitch);
	for (i=0; i<5; i++)
	{
		fprintf(stderr, "\tf[%d] = %x, b[%d] = %x\n", i, p->f[i], i, p->b[i]);
	}
}
