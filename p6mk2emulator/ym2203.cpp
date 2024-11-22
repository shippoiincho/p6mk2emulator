#include <stdio.h>
#include <stdint.h>
#include "fmgen/headers.h"
#include "fmgen/opna.h"
#include "ym2203.h"

FM::OPN opn;

void ym2203_init(uint32_t sampling) {

    opn.Init(3993600, sampling ,1); 

    opn.SetVolumePSG(14);
    opn.SetVolumeFM(14);

}

void ym2203_reset(void)
{
    opn.Reset();
}

void ym2203_write(uint8_t regno,uint8_t data) {

    opn.SetReg(regno,data);

}

uint8_t ym2203_read(uint8_t regno) {

    return opn.GetReg(regno);

}

int32_t ym2203_process(void) {

    FM::Sample buffer[8];

    for(int i=0;i<8;i++) {
        buffer[i]=0;
    }
 
    opn.Mix(buffer,1);

    return buffer[0];

}

void ym2203_fillbuffer(int16_t *buffer) {

    for(int i=0;i<16;i++) {
        buffer[i]=0;
    }
 
    opn.Mix(buffer,8);

    return;

}