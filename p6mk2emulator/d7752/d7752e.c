/* $Id: d7752e.c,v 1.1 2004/03/16 14:40:31 cisc Exp $ */

/*
 * D7752 �p�T�[�r�X���W���[��
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

#include "d7752e.h"

#define D7752_RCUNIT    8192

typedef unsigned char uint8;

struct D7752e_t
{
    D7752* engine;

    /* ���[�g�ϊ��֌W */
    int rcout0;
	int rcout1;
	int rcpos;
	int rcstep;

    /* �p�����[�^�o�b�t�@ */
    uint8* parambuf;
    int pbread;
    int pbwrite;
    int pbsize;

    int paramindex;
    uint8 param0;
    uint8 mode;

    /* �T���v���o�b�t�@ */
    D7752_SAMPLE* framebuf;
    int fbpos;
    int fbsize;

    int status;
};

/* internal macros */
#define ASSERT_D7752E(inst)	\
	assert(inst);	

#define CHECK_D7752E(inst)	\
	if (inst == 0) return D7752_ERR_PARAM; \
	ASSERT_D7752E(inst);

/* prototypes for static function */
static int D7752e_isBufFull(D7752e* inst);
static int D7752e_bufSet(D7752e* inst, uint8 data);
static int D7752e_bufGet(D7752e* inst);
static int D7752e_getSample(D7752e* inst);

/*
 * D7752e �̃C���X�^���X���쐬���܂��B
 * 
 * ����:    �Ȃ�
 * �Ԓl:	D7752e*     D7752e �̃C���X�^���X
 */
D7752e* D7752e_Open()
{
    D7752e* inst = (D7752e*) malloc(sizeof(D7752e));
    if (!inst)
        return NULL;

    memset(inst, 0, sizeof(D7752e));

    inst->parambuf = 0;
    inst->framebuf = 0;

    inst->engine = D7752_Open();
    if (!inst->engine)
    {
        free(inst);
        return NULL;
    }

    if (D7752e_SetBufferLength(inst, 7) < 0)
    {
        goto failure;
    }

    if (D7752e_SetRate(inst, 10000, 10000) < 0)
    {
        goto failure;
    }

    inst->status = 0;
    inst->mode = 0;
    
    return inst;

failure:
    D7752e_Close(inst);
    return NULL;
}

/*
 * D7752e �̃C���X�^���X��j�����܂��B
 * 
 * ����:    inst    D7752e �̃C���X�^���X
 * �Ԓl:    int     �G���[�R�[�h
 */
int D7752e_Close(D7752e* inst)
{
    if (inst)
    {
        ASSERT_D7752E(inst);
        D7752_Close(inst->engine);
        free(inst->parambuf);
        free(inst->framebuf);
        free(inst);
    }
    return D7752_ERR_SUCCESS;
}

/*
 * ���������̏o�̓T���v�����O���[�g��ݒ肵�܂��B
 * 
 * ����:    inst        D7752e �̃C���X�^���X
 *          chiprate    ���������`�b�v�̃T���v�����O���[�g
 *          pcmrate     PCM �o�͂̃T���v�����O���[�g
 * �Ԓl:    int         �G���[�R�[�h
 */
int D7752e_SetRate(D7752e* inst, int chiprate, int pcmrate)
{
    CHECK_D7752E(inst);
    assert(chiprate > 0);
    assert(pcmrate > 0);
    
    inst->rcout0 = 0;
    inst->rcout1 = 0;
    inst->rcpos = 0;
    inst->rcstep = chiprate * D7752_RCUNIT / pcmrate;
    
    return D7752_ERR_SUCCESS;
}

/*
 * �p�����[�^�o�b�t�@�̒�����ݒ肵�܂��B
 * �����������̌Ăяo���̓G���[�ɂȂ�܂��B
 *
 * inst:        D7752e �̃C���X�^���X
 * length:      �o�b�t�@��(�t���[���P��)  �Œ�ł�1�ȏ�͕K�v
 */
int D7752e_SetBufferLength(D7752e* inst, int length)
{
    uint8* newbuffer;

    CHECK_D7752E(inst);

    /* �Œ�ł�1�t���[�����̃o�b�t�@���K�v */
    assert(length >= 1);

    /* �Đ����ɂ̓o�b�t�@���̕ύX�����ۂ��� */
    if (inst->status & D7752E_BSY)
    {
        return D7752_ERR_DEVICE_MODE;
    }

    /* �o�b�t�@�̃o�C�g�����v�Z */
    length *= 7;
    
    /* �V�����o�b�t�@�����蓖�Ă� */
    newbuffer = (uint8*) malloc(length);
    if (newbuffer == NULL)
    {
        return D7752_ERR_MEMORY;
    }

    /* ����܂Ŏg���Ă����o�b�t�@��j�� */
    free(inst->parambuf);

    /* �V�����o�b�t�@�ɓ���ւ��� */
    inst->parambuf = newbuffer;
    inst->pbread = 0;
    inst->pbwrite = 0;
    inst->pbsize = length;

    return D7752_ERR_SUCCESS;
}

/*
 * ���������̃p�����[�^�o�b�t�@���t�����ǂ������m�F
 */
static int D7752e_isBufFull(D7752e* inst)
{
    /* �o�b�t�@��FULL�Ƃ������Ƃ́Apbwrite + 1 = pbread �Ƃ������� */
    if (inst->pbread == inst->pbwrite + 1)
        return 1;

    if (inst->pbread == 0 && inst->pbwrite == inst->pbsize-1)
        return 1;

    return 0;
}
/*
 * ���������̃p�����[�^���Z�b�g���܂��B
 * 
 * ����:    inst    D7752e �̃C���X�^���X
 *          data    �����p�����[�^
 * �Ԓl:    int     �G���[�R�[�h
 */
int D7752e_SetData(D7752e* inst, uint8 data)
{
    CHECK_D7752E(inst);

    /* �Đ����̂݃f�[�^���󂯕t���� */
    if ((inst->status & D7752E_BSY) == 0)
    {
        return D7752_ERR_DEVICE_MODE;
    }

    /* �ǉ������o�����o�b�t�@��7�o�C�g�P�ʂȂ̂ŁA�o�b�t�@�T�C�Y�̃`�F�b�N�͈�x�����s�� */
    if (D7752e_isBufFull(inst))
    {
        return D7752_ERR_BUFFER_FULL;
    }

    if (inst->paramindex < 0)
    {
        /* �J��Ԃ��t���[���̏��� */
        int i;

        D7752e_bufSet(inst, inst->param0);
        for (i=0; i<5; i++)
        {
            D7752e_bufSet(inst, 0);
        }
        D7752e_bufSet(inst, data);
        inst->paramindex++;
    }
    else if (inst->paramindex == 0)
    {
        D7752e_bufSet(inst, data);
        inst->paramindex++;
        inst->param0 = data;
    }
    else
    {
        D7752e_bufSet(inst, data);
        inst->paramindex++;
        if (inst->paramindex == 7)
        {
            inst->paramindex = -(inst->param0 >> 3) + 1;
        }
    }
    return D7752_ERR_SUCCESS;
}

/*
 * ���������p�����[�^�o�b�t�@��1�o�C�g�f�[�^��ۑ�
 */
static int D7752e_bufSet(D7752e* inst, uint8 data)
{
    if (D7752e_isBufFull(inst))
    {
        return D7752_ERR_BUFFER_FULL;
    }
    
    /* �o�b�t�@�Ƀf�[�^��ǉ� */
    inst->parambuf[inst->pbwrite++] = data;
    if (inst->pbwrite == inst->pbsize)
    {
        inst->pbwrite = 0;
    }
    return D7752_ERR_SUCCESS;
}

/*
 * ���������p�����[�^�o�b�t�@����f�[�^��1�o�C�g�擾���܂��B
 */
static int D7752e_bufGet(D7752e* inst)
{
    int data;

    /* �o�b�t�@����łȂ����Ƃ��m�F */
    if (inst->pbread == inst->pbwrite)
    {
        return D7752_ERR_BUFFER_EMPTY;
    }
    
    /* �o�b�t�@����1�o�C�g�擾 */
    data = inst->parambuf[inst->pbread++];
    if (inst->pbread == inst->pbsize)
    {
        inst->pbread = 0;
    }
    return data;
}

/*
 * 1�T���v�����̉������擾���܂��B
 */
static int D7752e_getSample(D7752e* inst)
{
    ASSERT_D7752E(inst);

    if (inst->fbpos >= inst->fbsize)
    {
        int i;
        int data;
        uint8 param[7];

// DEBUG
//        printf("[B:%d-%d]\n",inst->pbread,inst->pbwrite);
// DEBUF
        int writeptr;
        if(inst->pbread>inst->pbwrite) { 
            writeptr=inst->pbwrite+inst->pbsize;
        } else {
            writeptr=inst->pbwrite;
        }

        if((writeptr-inst->pbread)>=6) {

        for (i=0; i<7; i++)
        {
            data = D7752e_bufGet(inst);
            if (data < 0)
            {
                inst->status &= ~D7752E_BSY;
                inst->status |= D7752E_ERR;
                return 0;
            }
            if (i == 0 && (data & 0xf8) == 0)
            {
                inst->status &= ~D7752E_BSY;
                return 0;
            }
            param[i] = data;
        }

        /* 1�t���[�������� */
        D7752_Synth(inst->engine, param, inst->framebuf);

        } else { // Full Zero
    
            memset(inst->framebuf,0,(inst->fbsize)*sizeof(D7752_SAMPLE));
        }

        inst->fbpos = 0;
    }

    inst->rcout0 = inst->rcout1;
    inst->rcout1 = inst->framebuf[inst->fbpos++];
    return 1;
}


/*
 * �w�肳�ꂽ�T���v�����̉������������܂��B
 *
 * ����:	inst	�{�C�X�W�F�l���[�^�̃C���X�^���X
 *			samples	���������g�`�f�[�^�̊i�[��B
 *          length  ��������g�`�f�[�^�̒���(�P��: �T���v��)
 * �Ԓl:	int		�G���[�R�[�h
 */
int D7752e_Synth(D7752e* inst, D7752_SAMPLE* samples, int length)
{
    CHECK_D7752E(inst);
    assert(samples);
    assert(length >= 0);

    /* 
     * �Đ����t���O�`�F�b�N
     */
    if ((inst->status & D7752E_BSY) == 0)
    {
        while (length-- > 0)
            *samples++ = 0;
        return D7752_ERR_DEVICE_MODE;
    }

    /*
     * �t���[���o�b�t�@���特�������o���Ȃ���A���`��Ԃ��s��
     */
    if (inst->rcstep <= D7752_RCUNIT)
    {
        for (; length > 0; length--)
        {
            int sample;

            if (inst->rcpos < 0)
            {
                inst->rcpos += D7752_RCUNIT;
                if (!D7752e_getSample(inst))
                    break;
            }
            sample = (inst->rcpos * inst->rcout0 + (D7752_RCUNIT - inst->rcpos) * inst->rcout1) / D7752_RCUNIT;

            *samples++ = sample * 4;
            inst->rcpos -= inst->rcstep;
        }
    }
    else
    {
        int t = (-D7752_RCUNIT*D7752_RCUNIT) / inst->rcstep;
        for (; length > 0; length--)
        {
            int sample = inst->rcout0 * (D7752_RCUNIT + inst->rcpos);
            while (inst->rcpos < 0)
            {
                if (!D7752e_getSample(inst))
                    break;
                sample -= inst->rcout0 * (inst->rcpos > t ? inst->rcpos : t);
                inst->rcpos -= t;
            }
            inst->rcpos -= D7752_RCUNIT;
            sample /= D7752_RCUNIT;

            *samples++ = sample * 4;
        }
    }

    /*
     * �������Ō�܂ōs���Ȃ������ꍇ�A�o�b�t�@���N���A����
     */
    for (; length > 0; length--)
    {
        *samples++ = 0;
    }

    return D7752_ERR_SUCCESS;
}

/*
 * ���������̊J�n�E��~���s���܂��B
 *
 * ����:    inst    D7752e �̃C���X�^���X
 *          command 
 *              = 0xfe  ���������̊J�n
 *              = 0xff  ���������̏I��
 * �Ԓl:    int     �G���[�R�[�h
 */
int D7752e_SetCommand(D7752e* inst, unsigned char command)
{
    int err;

    CHECK_D7752E(inst);

    /*
     * �󂯕t�����Ȃ��R�}���h�͖�������B
     * XXX: �Œ�ꔭ�����[�h�̎��͍������Ƃ߂��ق�������?
     */
    if (command != 0xfe && command != 0xff)
    {
        return D7752_ERR_SUCCESS;
    }

    /*
     * �܂��Đ���~�̏��� 
     */
    
    /* �X�e�[�^�X���Đ���~���[�h�ɂ��� */
    inst->status &= ~D7752E_BSY;

    /* �t���[���o�b�t�@�͊J������ */
    free(inst->framebuf);
    inst->framebuf = 0;

    /* �Đ��J�n */
    if (command == 0xfe)
    {
        /* ���������G���W���̏����� */
        err = D7752_Start(inst->engine, inst->mode);
        if (err < 0)
            return err;

        /* �t���[���o�b�t�@�̊��蓖�� */
        inst->fbsize = D7752_GetFrameSize(inst->engine);
        inst->framebuf = (D7752_SAMPLE*) malloc(sizeof(D7752_SAMPLE) * inst->fbsize);
        if (!inst->framebuf)
            return D7752_ERR_MEMORY;

        /* 
         * fbpos = fbsize �Ƃ��邱�ƂŃt���[���o�b�t�@����ɃZ�b�g����B
         * ���̏ꍇ�A�����Ƀp�����[�^���v������邱�ƂɂȂ邪�A���ԓI��
         * �]�T���K�v�Ȃ�Aframebuf �� 0 �Ńt�B������ fbpos = 0 �Ƃ����
         * ���������B
         * ���邢�́A�p�����[�^�o�b�t�@�Ƀ_�~�[�l������Ƃ���������邩�ƁB
         */
        inst->fbpos = inst->fbsize;

        /* ���`�⊮�֌W */
        inst->rcout0 = 0;
        inst->rcout1 = 0;
        inst->rcpos = 0;

        /* 
         * �p�����[�^�o�b�t�@�̏������B
         * pbwrite = pbread �Ȃ�o�b�t�@����Ƃ������ƂɂȂ邪�A�l�͈̔͂�
         * �O��Ă����猙�����B
         */
        inst->pbwrite = inst->pbread = 0; 

        /*
         * param0 �͌J��Ԃ��t���[���̃J�E���^�Ƃ��Ďg���Ă���
         */
        inst->param0 = 0;
        
        /*
         * �X�e�[�^�X���Đ����[�h�ɃZ�b�g
         */
        inst->status = D7752E_BSY | D7752E_EXT;
    }

    return D7752_ERR_SUCCESS;
}

/*
 * D7752�̃X�e�[�^�X��Ԃ��܂��B
 *
 * ����:    inst    D7752e �̃C���X�^���X
 * �Ԓl:    int
 *              b7  BSY - �����������Ȃ� 1
 *              b6  REQ - �����p�����[�^�o�b�t�@�ɗ]�T������� 1
 *              b5  INT/EXT - 1
 *              b4  ERR - �]���G���[���������ꍇ 1
 */
int D7752e_GetStatus(D7752e* inst)
{
    int status;
    CHECK_D7752E(inst);

    status = inst->status & ~D7752E_REQ;
    if ((status & D7752E_BSY) && !D7752e_isBufFull(inst))
    {
        status |= D7752E_REQ;
    }
    return status;
}

/*
 * ���������̃��[�h��ݒ肵�܂��B
 * �V�������[�h�́A����̉��������̊J�n������L���ɂȂ�܂��B
 *
 * ����:    inst    D7752e�̃C���X�^���X
 *          mode    ���������̃��[�h���w�肵�܂�
 *              b2 F: ���̓t���[�� BIT
 *                  F=0 10ms/�t���[��
 *                  F=1 20ms/�t���[��
 *              b1-0 S: �������x BIT
 *                  S=00: NORMAL SPEED
 *                  S=01: SLOW SPEED 
 *                  S=10: FAST SPEED
 * �Ԓl:    int     �G���[�R�[�h
 */
int D7752e_SetMode(D7752e* inst, unsigned char mode)
{
    CHECK_D7752E(inst);

    inst->mode = mode;
    return D7752_ERR_SUCCESS;
}
