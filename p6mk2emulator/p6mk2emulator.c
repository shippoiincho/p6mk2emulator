//  NEC PC-6001mk2 / 6601 emulator
//
//  GP0: HSYNC
//  GP1: VSYNC
//  GP2: Blue0
//  GP3: Blue1
//  GP4: Red0
//  GP5: Red1
//  GP6: Green0
//  GP7: Green1
//  GP10: Audio

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/pwm.h"

#include "tusb.h"
#include "bsp/board.h"

#include "vga16_graphics.h"

#include "p6mk2keymap.h"
#include "Z80.h"
#include "p6mk2misc.h"

#include "lfs.h"

//#define USE_COMPATIBLE_ROM

//#define USE_EXT_ROM

#ifdef USE_COMPATIBLE_ROM
#include "p6mk2gokanrom.h"
#else
#include "p6mk2rom.h"
#endif

#ifdef USE_EXT_ROM
#include "p6mk2extrom.h"
uint8_t extbank=0;
uint8_t extram[0x10000];
#endif

// VGAout configuration

#define DOTCLOCK 25000
#define CLOCKMUL 9
// Pico does not work at CLOCKMUL=7 (175MHz).

#define VGA_PIXELS_X 320
#define VGA_PIXELS_Y 200

#define VGA_CHARS_X 40
#define VGA_CHARS_Y 25

#define VRAM_PAGE_SIZE (VGA_PIXELS_X*VGA_PIXELS_Y/8)

extern unsigned char vga_data_array[];
volatile uint8_t fbcolor,cursor_x,cursor_y,video_mode;

volatile uint32_t video_hsync,video_vsync,scanline,vsync_scanline;
// volatile uint32_t redraw_command=0;
// volatile uint32_t scroll_flag=0;

struct repeating_timer timer,timer2;

// MZ configuration

//static Z80Context cpu;
static Z80 cpu;
uint32_t cpu_clocks=0;
uint32_t cpu_ei=0;
uint32_t cpu_cycles=0;
uint32_t cpu_hsync=0;


uint8_t mainram[0x10000];
uint8_t ioport[0x100];

uint32_t colormode=0;

volatile uint32_t subcpu_enable_irq=0;
uint32_t subcpu_irq_processing=0;
uint32_t subcpu_command_processing=0;
uint8_t subcpu_ird=0;
uint8_t subcpu_irq_delay=0;

uint8_t timer_enable_irq=0;

volatile uint8_t keypressed=0;  //last pressed usbkeycode
volatile uint16_t p6keypressed=0;
volatile uint8_t p6keypad=0;
uint32_t key_repeat_flag=1;
uint32_t key_repeat_count=0;
uint32_t key_caps=0;
uint32_t key_kana=0;
uint32_t key_hirakata=0;
uint32_t lastmodifier=0; 

// BEEP & PSG

uint32_t pwm_slice_num;
volatile uint32_t sound_tick=0;

uint8_t psg_register_number=0;

uint8_t psg_register[16];
uint32_t psg_osc_interval[4];
uint32_t psg_osc_counter[4];

uint32_t psg_noise_interval;
uint32_t psg_noise_counter;
uint8_t psg_noise_output;
uint32_t psg_noise_seed;
uint32_t psg_envelope_interval;
uint32_t psg_envelope_counter;
uint32_t psg_master_clock = 1996800;
uint16_t psg_master_volume = 0;

uint8_t psg_tone_on[4], psg_noise_on[4];

const uint16_t psg_volume[] = { 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04,
       0x05, 0x06, 0x07, 0x08, 0x09, 0x0b, 0x0d, 0x10, 0x13, 0x17, 0x1b, 0x20,
       0x26, 0x2d, 0x36, 0x40, 0x4c, 0x5a, 0x6b, 0x80, 0x98, 0xb4, 0xd6, 0xff };

#define SAMPLING_FREQ 48000
//#define SAMPLING_FREQ 22050
#define TIME_UNIT 100000000                           // Oscillator calculation resolution = 10nsec
#define SAMPLING_INTERVAL (TIME_UNIT/SAMPLING_FREQ) 

// Tape

uint32_t tape_ready=0;
uint32_t tape_ptr=0;
uint32_t tape_phase=0;
uint32_t tape_count=0;

uint32_t tape_read_wait=0;
uint32_t tape_autoclose=0;

#define TAPE_WAIT 2000

#define TAPE_THRESHOLD 200000

uint8_t uart_rx[32];
uint8_t uart_nibble=0;
uint8_t uart_count=0;
volatile uint8_t uart_write_ptr=0;
volatile uint8_t uart_read_ptr=0;
uint32_t uart_cycle;

// UI

uint32_t menumode=0;
uint32_t menuitem=0;

// USB

hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released
extern void hid_app_task(void);

uint32_t usbcheck_count=0;
uint32_t kbhit=0;            // 4:Key pressed (timer stop)/3&2:Key depressed (timer running)/1:no key irq triggerd
uint8_t hid_dev_addr=255;
uint8_t hid_instance=255;
uint8_t hid_led;

#define USB_CHECK_INTERVAL 30 // 31.5us*30=1ms

// Define the flash sizes
// This is setup to read a block of the flash from the end 
#define BLOCK_SIZE_BYTES (FLASH_SECTOR_SIZE)
// for 1M flash pico
//#define HW_FLASH_STORAGE_BASE   (1024*1024 - HW_FLASH_STORAGE_BYTES) 
//#define HW_FLASH_STORAGE_BYTES  (512 * 1024)
// for 2M flash
// #define HW_FLASH_STORAGE_BYTES  (1024 * 1024)
#define HW_FLASH_STORAGE_BYTES  (1536 * 1024)
#define HW_FLASH_STORAGE_BASE   (PICO_FLASH_SIZE_BYTES - HW_FLASH_STORAGE_BYTES) 
// for 16M flash
//#define HW_FLASH_STORAGE_BYTES  (15872 * 1024)
//#define HW_FLASH_STORAGE_BASE   (1024*1024*16 - HW_FLASH_STORAGE_BYTES) 

lfs_t lfs;
lfs_file_t lfs_file;

#define FILE_THREHSOLD 20000000
#define LFS_LS_FILES 9

volatile uint32_t load_enabled=0;
volatile uint32_t save_enabled=0;
//uint32_t file_cycle=0;

unsigned char filename[16];
unsigned char tape_filename[16];

static inline unsigned char tohex(int);
static inline unsigned char fromhex(int);
static inline void video_print(uint8_t *);
uint8_t fdc_find_sector(void);


// Virtual H-Sync for emulation
bool __not_in_flash_func(hsync_handler)(struct repeating_timer *t) {

    if(scanline%262==0) {
        video_vsync=1;
    }

    scanline++;

    video_hsync=1;

    // Timer (0xf6+1) *0.5ms

    if((scanline%((ioport[0xf6]+1)*8))==0) {     

        if (((ioport[0xb0]&1)==0)&&((ioport[0xf3]&4)==0)) {
            timer_enable_irq=1;
        }        
    }

    return true;

}

// BEEP and PSG emulation
bool __not_in_flash_func(sound_handler)(struct repeating_timer *t) {

    uint16_t timer_diffs;
    uint32_t pon_count;
    uint16_t master_volume;
    uint8_t tone_output[3], noise_output[3], envelope_volume;

    pwm_set_chan_level(pwm_slice_num,PWM_CHAN_A,psg_master_volume);

    // PSG

    master_volume = 0;

    // Run Noise generator

        psg_noise_counter += SAMPLING_INTERVAL;
        if (psg_noise_counter > psg_noise_interval) {
            psg_noise_seed = (psg_noise_seed >> 1)
                    | (((psg_noise_seed << 14) ^ (psg_noise_seed << 16))
                            & 0x10000);
            psg_noise_output = psg_noise_seed & 1;
            psg_noise_counter -= psg_noise_interval;
        }
        if (psg_noise_output != 0) {
            noise_output[0] = psg_noise_on[0];
            noise_output[1] = psg_noise_on[1];
            noise_output[2] = psg_noise_on[2];
        } else {
            noise_output[0] = 0;
            noise_output[1] = 0;
            noise_output[2] = 0;
        }

    // Run Envelope

        envelope_volume = 0;

        switch (psg_register[13] & 0xf) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 9:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = 31
                        - psg_envelope_counter / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                envelope_volume = 0;
            }
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 15:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = psg_envelope_counter
                        / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                envelope_volume = 0;
            }
            break;
        case 8:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = 31
                        - psg_envelope_counter / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                psg_envelope_counter -= psg_envelope_interval * 32;
                envelope_volume = 31;
            }
            break;
        case 10:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = 31
                        - psg_envelope_counter / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else if (psg_envelope_counter
                    < psg_envelope_interval * 64) {
                envelope_volume = psg_envelope_counter
                        / psg_envelope_interval - 32;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                psg_envelope_counter -= psg_envelope_interval * 64;
                envelope_volume = 31;
            }
            break;
        case 11:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = 31
                        - psg_envelope_counter / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                envelope_volume = 31;
            }
            break;
        case 12:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = psg_envelope_counter
                        / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                psg_envelope_counter -= psg_envelope_interval * 32;
                envelope_volume = 0;
            }
            break;
        case 13:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = psg_envelope_counter
                        / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                envelope_volume = 31;
            }
            break;
        case 14:
            if (psg_envelope_counter < psg_envelope_interval * 32) {
                envelope_volume = psg_envelope_counter
                        / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else if (psg_envelope_counter
                    < psg_envelope_interval * 64) {
                envelope_volume = 63
                        - psg_envelope_counter / psg_envelope_interval;
                psg_envelope_counter += SAMPLING_INTERVAL;
            } else {
                psg_envelope_counter -= psg_envelope_interval * 64;
                envelope_volume = 0;
            }
            break;
        }


    // Run Oscillator

    for (int i = 0; i < 4 ; i++) {
        pon_count = psg_osc_counter[i] += SAMPLING_INTERVAL;
        if (pon_count < (psg_osc_interval[i] / 2)) {
            tone_output[i] = psg_tone_on[i];
        } else if (pon_count > psg_osc_interval[i]) {
            psg_osc_counter[i] -= psg_osc_interval[i];
            tone_output[i] = psg_tone_on[i];
        } else {
            tone_output[i] = 0;
        }
    }

    // Mixer

    master_volume = 0;

        for (int j = 0; j < 3; j++) {
            if ((tone_output[j] + noise_output[j]) > 0) {
                if ((psg_register[j + 8] & 0x10) == 0) {
                    master_volume += psg_volume[(psg_register[j + 8 ]
                            & 0xf) * 2 + 1];
                } else {
                    master_volume += psg_volume[envelope_volume];
                }
            }
        }

    psg_master_volume = master_volume / 3;    // Add beep

    if (psg_master_volume > 255)
        psg_master_volume = 255;

    return true;
}

// PSG virtual registers

void psg_write(uint32_t data) {

    uint32_t channel,freqdiv,freq;

    psg_register_number=ioport[0xa0];

    if(psg_register_number>15) return;

    psg_register[psg_register_number]=data;


    // printf("[PSG:%x,%x]",psg_register_number,data);

    switch(psg_register_number&0xf) {
        case 0:
        case 1:
            if((psg_register[0]==0)&&(psg_register[1]==0)) {
                psg_osc_interval[0]=UINT32_MAX;
                break;
            }
            freq = psg_master_clock / ( psg_register[0] + ((psg_register[1]&0x0f)<<8) );
            freq >>= 4;
            if(freq!=0) {
                psg_osc_interval[0] = TIME_UNIT / freq;
                psg_osc_counter[0]=0;
            } else {
                psg_osc_interval[0]=UINT32_MAX;
            }
            break;
        case 2:
        case 3:
            if((psg_register[2]==0)&&(psg_register[3]==0)) {
                psg_osc_interval[1]=UINT32_MAX;
                break;
            }
            freq = psg_master_clock / ( psg_register[2] + ((psg_register[3]&0x0f)<<8) );
            freq >>= 4;
            if(freq!=0) {
                psg_osc_interval[1] = TIME_UNIT / freq;
                psg_osc_counter[1]=0;
            } else {
                psg_osc_interval[1]=UINT32_MAX;
            }
            break;
        case 4:
        case 5:
            if((psg_register[4]==0)&&(psg_register[5]==0)) {
                psg_osc_interval[2]=UINT32_MAX;
                break;
            }
            freq = psg_master_clock / ( psg_register[4] + ((psg_register[5]&0x0f)<<8) );
            freq >>= 4;
            if(freq!=0) {
                psg_osc_interval[2] = TIME_UNIT / freq;
                psg_osc_counter[2]=0;
                } else {
                    psg_osc_interval[2]=UINT32_MAX;
                }
            break;
        case 6:
            if((psg_register[6]==0)&&(psg_register[7]==0)) {
                psg_noise_interval=UINT32_MAX;
                break;
            }
            freq = psg_master_clock / ( psg_register[6] & 0x1f );
            freq >>= 4;
            if(freq!=0) {
                psg_noise_interval = TIME_UNIT / freq;
                psg_noise_counter = 0;
            } else {
                psg_noise_interval=UINT32_MAX;
            }
            break;
        case 7:
            psg_tone_on[0]=((psg_register[7]&1)==0?1:0);
            psg_tone_on[1]=((psg_register[7]&2)==0?1:0);
            psg_tone_on[2]=((psg_register[7]&4)==0?1:0);
            psg_noise_on[0]=((psg_register[7]&8)==0?1:0);
            psg_noise_on[1]=((psg_register[7]&16)==0?1:0);
            psg_noise_on[2]=((psg_register[7]&32)==0?1:0);
            break;
        case 0xb:
        case 0xc:
            freq = psg_master_clock / ( psg_register[0xb] + (psg_register[0xc]<<8) );
            if(freq!=0) {
                psg_envelope_interval= TIME_UNIT / freq;
                psg_envelope_interval<<=5;
            } else {
                psg_envelope_interval=UINT32_MAX/2-1;
            }
            break;
        case 0xd:
            psg_envelope_counter=0;
            break;
//                        case 0xf:
//                        psg_reset(1,psg_no);
    }
}

void __not_in_flash_func(uart_handler)(void) {

    uint8_t ch;

    if(uart_is_readable(uart0)) {
        ch=uart_getc(uart0);
        if(uart_count==0) {
            uart_nibble=fromhex(ch)<<4;
            uart_count++;
        } else {
            ch=fromhex(ch)+uart_nibble;
            uart_count=0;

            if(uart_read_ptr==uart_write_ptr+1) {  // buffer full
                return;
            }
            if((uart_read_ptr==0)&&(uart_write_ptr==31)) {
                return;
            }

            uart_rx[uart_write_ptr]=ch;
            uart_write_ptr++;
            if(uart_write_ptr>31) {
                uart_write_ptr=0;
            }
        }
    }

}

uint8_t tapein() {

    static uint8_t tapebyte;

    if(load_enabled==0) {
        return 0;
    }

    lfs_file_read(&lfs,&lfs_file,&tapebyte,1);
    tape_ptr++;

//    printf("(%02x)",tapebyte);

    return tapebyte;

}

void tapeout(uint8_t data) {

    if(save_enabled) {

        lfs_file_write(&lfs,&lfs_file,&data,1);
        tape_ptr++;
//        printf("(%02x)",data);

    } else {
        printf("%02x",data);
    }

}



static inline void video_cls() {
    memset(vga_data_array, 0x0, (VGA_PIXELS_X*VGA_PIXELS_Y));
}

static inline void video_scroll() {

    memmove(vga_data_array, vga_data_array + VGA_PIXELS_X*10, (VGA_PIXELS_X*(VGA_PIXELS_X-10)));
    memset(vga_data_array + (VGA_CHARS_X*(VGA_PIXELS_X-10)), 0, VGA_PIXELS_X*10);

}

static inline void video_print(uint8_t *string) {

    int len;
    uint8_t fdata;

    len = strlen(string);

    for (int i = 0; i < len; i++) {

        for(int slice=0;slice<10;slice++) {

            uint8_t ch=string[i];

            fdata=cgrom2[ch*16+slice];

            uint32_t vramindex=cursor_x*8+VGA_PIXELS_X*(cursor_y*10+slice);

            for(int slice_x=0;slice_x<8;slice_x++){

                if(fdata&0x80) {
                    vga_data_array[vramindex+slice_x]=colors[(fbcolor&7) + 8];
                } else {
                    vga_data_array[vramindex+slice_x]=colors[((fbcolor&0x70)>>4) + 8];  
                }              

                fdata<<=1;

            }

        }

        cursor_x++;
        if (cursor_x >= VGA_CHARS_X) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= VGA_CHARS_Y) {
                video_scroll();
                cursor_y = VGA_CHARS_Y - 1;
            }
        }
    }

}

void draw_menu(void) {

    cursor_x=2;
    cursor_y=5;
    fbcolor=7;
      video_print("                                    ");
    for(int i=6;i<19;i++) {
        cursor_x=2;
        cursor_y=i;
        video_print("                                    ");
    }

    cursor_x=2;
    cursor_y=19;
    fbcolor=7;
    video_print("                                    ");

}

int draw_files(int num_selected,int page) {

    lfs_dir_t lfs_dirs;
    struct lfs_info lfs_dir_info;
    uint32_t num_entry=0;
    unsigned char str[16];

    int err= lfs_dir_open(&lfs,&lfs_dirs,"/");

    if(err) return -1;

    for(int i=0;i<LFS_LS_FILES;i++) {
        cursor_x=22;
        cursor_y=i+6;
        fbcolor=7;
        video_print("             ");
    }

    while(1) {

        int res= lfs_dir_read(&lfs,&lfs_dirs,&lfs_dir_info);
        if(res<=0) {
            break;
        }

        cursor_x=28;
        cursor_y=18;
        fbcolor=7;
        sprintf(str,"Page %02d",page+1);

        video_print(str);

        switch(lfs_dir_info.type) {

            case LFS_TYPE_DIR:
                break;
            
            case LFS_TYPE_REG:

                if((num_entry>=LFS_LS_FILES*page)&&(num_entry<LFS_LS_FILES*(page+1))) {

                    cursor_x=22;
                    cursor_y=num_entry%LFS_LS_FILES+6;

                    if(num_entry==num_selected) {
                        fbcolor=0x70;
                        memcpy(filename,lfs_dir_info.name,16);
                    } else {
                        fbcolor=7;
                    }

                    video_print(lfs_dir_info.name);

                }

                num_entry++;

                break;

            default:
                break; 

        }

    }

    lfs_dir_close(&lfs,&lfs_dirs);

    return num_entry;

}

int file_selector(void) {

    uint32_t num_selected=0;
    uint32_t num_files=0;
    uint32_t num_pages=0;

    num_files=draw_files(-1,0);

    if(num_files==0) {
         return -1;
    }

    while(1) {

        while(video_vsync==0) ;
        video_vsync=0;

        draw_files(num_selected,num_selected/LFS_LS_FILES);

        tuh_task();

        if(keypressed==0x52) { // up
            keypressed=0;
            if(num_selected>0) {
                num_selected--;
            }
        }

        if(keypressed==0x51) { // down
            keypressed=0;
            if(num_selected<num_files-1) {
                num_selected++;
            }
        }

        if(keypressed==0x4b) { // Pageup
            keypressed=0;
            if(num_selected>=LFS_LS_FILES) {
                num_selected-=LFS_LS_FILES;
            }
        }

        if(keypressed==0x4e) { // Pagedown
            keypressed=0;
            if(num_selected<num_files-LFS_LS_FILES) {
                num_selected+=LFS_LS_FILES;
            }
        }

        if(keypressed==0x28) { // Ret
            keypressed=0;

            return 0;
        }

        if(keypressed==0x29 ) {  // ESC

            return -1;

        }

    }
}

int enter_filename() {

    unsigned char new_filename[16];
    unsigned char str[32];
    uint8_t keycode;
    uint32_t pos=0;

    memset(new_filename,0,16);

    while(1) {

        sprintf(str,"Filename:%s  ",new_filename);
        cursor_x=3;
        cursor_y=18;
        video_print(str);

        while(video_vsync==0) ;
        video_vsync=0;

        tuh_task();

        if(keypressed!=0) {

            if(keypressed==0x28) { // enter
                keypressed=0;
                if(pos!=0) {
                    memcpy(filename,new_filename,16);
                    return 0;
                } else {
                    return -1;
                }
            }

            if(keypressed==0x29) { // escape
                keypressed=0;
                return -1;
            }

            if(keypressed==0x2a) { // backspace
                keypressed=0;

                cursor_x=3;
                cursor_y=18;
                video_print("Filename:          ");

                new_filename[pos]=0;

                if(pos>0) {
                    pos--;
                }
            }

            if(keypressed<0x4f) {
                keycode=usbhidcode[keypressed*2];
                keypressed=0;

                if(pos<7) {

                    if((keycode>0x20)&&(keycode<0x5f)&&(keycode!=0x2f)) {

                        new_filename[pos]=keycode;
                        pos++;

                    }

                }
            }


        }
    }

}

// PC-6001 mode
// 256     x 192
// 32pixel   4pixel pedestal

static void draw_framebuffer_p6(uint16_t addr) {
    uint32_t slice_x,slice_y;
    uint16_t baseaddr,offset;
    uint32_t vramindex;
    uint32_t ch,color,bitdata1,bitdata2,bitmask1,bitmask2;
    uint8_t attribute,graphicmode,graphiclines,font,col1,col2,col3;

    union bytemember {
         uint32_t w;
         uint8_t b[4];
    };

    union bytemember bitc1,bitc2;

    // check page number

    switch(ioport[0xb0]&0x6) {
        case 0:
            baseaddr=0xc000;
            break;
        case 0x2:
            baseaddr=0xe000;
            break;
        case 0x4:
            baseaddr=0x8000;
            break;
        case 0x6:
            baseaddr=0xa000;
            break;
        default:
            baseaddr=0;
    }

    // check on screen

    if(baseaddr==0) return;
    if(addr<baseaddr) return;
    if((addr-baseaddr)>0x1a00) return;

    // Attribute or Data

    if((addr-baseaddr)<512) {

        attribute=mainram[addr];
        if(attribute&0x80) {
            graphicmode=1;
            graphiclines=12;
        } else {
            graphicmode=0;
        }

        offset=addr-baseaddr+512;

    } else {

        attribute=mainram[addr&0xe1ff];
        offset=addr-baseaddr;

        if(attribute&0x80) {
            graphicmode=1;
            graphiclines=1;
        } else {
            graphicmode=0;
        }

    }

    slice_x=(offset-512)%32;
    slice_y=(offset-512)/32;

    uint32_t *vramptr=(uint32_t *)vga_data_array;

    if(graphicmode==0) { // charactor or semigraphic

        if(slice_y>15) return;

        vramindex= slice_x*8/4 + 32/4 + (slice_y*12 + 4) * VGA_PIXELS_X/4 ;  // draw offset

        ch=mainram[baseaddr+offset];

        for(uint8_t slice_yy=0;slice_yy<12;slice_yy++) {

            if(attribute&0x40) {
                font=p6semi[(ch&0x3f)*16+slice_yy];

                color=(attribute&2)*2 + ((ch&0xc0)>>6);
                col1=colors_mk2_mode3[color+16];
                col2=colors_mk2_mode3[0];

            } else {
                font=cgrom[ch*16+slice_yy];

                if(colormode==0) {
                    col1=colors_p6_mode1[(attribute&0x3)*2];
                    col2=colors_p6_mode1[(attribute&0x3)*2+1];
                } else {
                    col1=colors_p6_mode1_alt[(attribute&0x3)*2];
                    col2=colors_p6_mode1_alt[(attribute&0x3)*2+1];
                }
            }

                bitdata2=bitexpand[font*4];
                bitdata1=bitexpand[font*4+1];
                bitmask2=bitexpand[font*4+2];
                bitmask1=bitexpand[font*4+3];

                *(vramptr+vramindex + slice_yy*VGA_PIXELS_X/4)    = (bitdata1*col1) | (bitmask1*col2);
                *(vramptr+vramindex + slice_yy*VGA_PIXELS_X/4 +1) = (bitdata2*col1) | (bitmask2*col2);    
            
        }
    } else {  // graphic mode

        slice_x=(offset-512)%32;
        slice_y=(offset-512)/32;

        for(int slice_yy=0;slice_yy<graphiclines;slice_yy++) {

            if((attribute&0x1c)==0x0c) { // mode 3

                vramindex= slice_x*8/4 + 32/4 + (slice_y + slice_yy * 16 + 2) * VGA_PIXELS_X/4 ;  // draw offset

                font=mainram[baseaddr+offset+slice_yy*512];

                bitc1.w=bitexpand4[font*2];
                bitc2.w=bitexpand4[font*2+1];

                col3=(attribute&2)*2+16;

                bitc1.b[0]=colors_mk2_mode3[bitc1.b[0]+col3];
                bitc1.b[1]=colors_mk2_mode3[bitc1.b[1]+col3];   
                bitc1.b[2]=colors_mk2_mode3[bitc1.b[2]+col3];   
                bitc1.b[3]=colors_mk2_mode3[bitc1.b[3]+col3];                   

                bitc2.b[0]=colors_mk2_mode3[bitc2.b[0]+col3];
                bitc2.b[1]=colors_mk2_mode3[bitc2.b[1]+col3];   
                bitc2.b[2]=colors_mk2_mode3[bitc2.b[2]+col3];   
                bitc2.b[3]=colors_mk2_mode3[bitc2.b[3]+col3]; 

                *(vramptr+vramindex)   = bitc2.w;
                *(vramptr+vramindex+1) = bitc1.w;  

            } else if((attribute&0x1c)==0x1c) {  // mode 4

                vramindex= slice_x*8/4 + 32/4 + (slice_y + slice_yy * 16 + 2) * VGA_PIXELS_X/4 ;  // draw offset

                font=mainram[baseaddr+offset+slice_yy*512];

                if((colormode==2)&&(attribute&2)) {
                    // simulate NTSC Color mixing

                    bitc1.w=bitexpand4[font*2];
                    bitc2.w=bitexpand4[font*2+1];

                    bitc1.b[0]=colors_p6_mode4[bitc1.b[0]];
                    bitc1.b[1]=colors_p6_mode4[bitc1.b[1]];
                    bitc1.b[2]=colors_p6_mode4[bitc1.b[2]];
                    bitc1.b[3]=colors_p6_mode4[bitc1.b[3]];

                    bitc2.b[0]=colors_p6_mode4[bitc2.b[0]];
                    bitc2.b[1]=colors_p6_mode4[bitc2.b[1]];
                    bitc2.b[2]=colors_p6_mode4[bitc2.b[2]];
                    bitc2.b[3]=colors_p6_mode4[bitc2.b[3]];

                *(vramptr+vramindex)   = bitc2.w;
                *(vramptr+vramindex+1) = bitc1.w;  

                } else {

                    bitdata2=bitexpand[font*4];
                    bitdata1=bitexpand[font*4+1];
                    bitmask2=bitexpand[font*4+2];
                    bitmask1=bitexpand[font*4+3];

                    if(colormode==0) {
                        col1=colors_p6_mode1[(attribute&2)+9];
                        col2=colors_p6_mode1[(attribute&2)+8];
                    } else {
                        col1=colors_p6_mode1_alt[(attribute&2)+9];
                        col2=colors_p6_mode1_alt[(attribute&2)+8];                        
                    }

                *(vramptr+vramindex)   = (bitdata1 * col1) | (bitmask1 * col2);
                *(vramptr+vramindex+1) = (bitdata2 * col1) | (bitmask2 * col2);  

                }
            }
        }
    }
}

static void draw_framebuffer_mk2(uint16_t addr) {
    uint32_t slice_x,slice_y;
    uint16_t baseaddr,offset;
    uint32_t vramindex;
    uint32_t ch,color;
    uint32_t bitdata1,bitdata2,bitmask1,bitmask2;
    uint8_t attribute,graphicmode,graphiclines,font;
    uint8_t col1,col2,col3;

    union bytemember {
         uint32_t w;
         uint8_t b[4];
    };

    union bytemember bitc1,bitc2;

    // check page number

    switch(ioport[0xb0]&0x6) {
        case 0:
            baseaddr=0x8000;
            break;
        case 0x2:
            baseaddr=0xc000;
            break;
        case 0x4:
            baseaddr=0x0000;
            break;
        case 0x6:
            baseaddr=0x4000;
            break;
        default:
            baseaddr=0;
    }

    // check on screen

    if(addr<baseaddr) return;
    if((addr-baseaddr)>0x4000) return;

    uint32_t *vramptr=(uint32_t *)vga_data_array;

    if(ioport[0xc1]&4) { // charactor mode

        // Attribute or Data

        if((addr-baseaddr)<1024) {
            attribute=mainram[addr];
            offset=addr-baseaddr+1024;
        } else {
            attribute=mainram[addr-1024];
            offset=addr-baseaddr;
        }

        slice_x=(offset-1024)%40;
        slice_y=(offset-1024)/40;

        if(slice_y>19) return;

        vramindex= slice_x * 8 / 4 +  (slice_y*10) * VGA_PIXELS_X / 4;  

        ch=mainram[baseaddr+offset];

        col1=colors[(attribute&0xf)];
        col2=colors[((attribute&0x70)>>4)+(ioport[0xc0]&2)*4];

        for(uint8_t slice_yy=0;slice_yy<10;slice_yy++) {

            if(attribute&0x80) {
                font=cgrom2[ch*16+slice_yy+4096];
            } else {
                font=cgrom2[ch*16+slice_yy];
            }

            bitdata2=bitexpand[font*4];
            bitdata1=bitexpand[font*4+1];

            bitmask2=bitexpand[font*4+2];
            bitmask1=bitexpand[font*4+3];

            *(vramptr+vramindex + slice_yy*VGA_PIXELS_X/4)    = (bitdata1*col1) | (bitmask1*col2);
            *(vramptr+vramindex + slice_yy*VGA_PIXELS_X/4 +1) = (bitdata2*col1) | (bitmask2*col2);           

        }


    } else { // Graphic mode

        if(ioport[0xc1]&8) {  // Screen 3

            offset=(addr-baseaddr)&0x1fff;

            if(offset>=0x1f40) return;

            col1=mainram[baseaddr+offset];
            col2=mainram[baseaddr+offset+0x2000];

            slice_x=offset%40;
            slice_y=offset/40;

            bitdata1=bitexpand4[mainram[baseaddr+offset]*2];
            bitdata2=bitexpand4[mainram[baseaddr+offset+0x2000]*2]<<2;

            bitc1.w= bitdata1 | bitdata2 ;

            bitdata1=bitexpand4[mainram[baseaddr+offset]*2+1];
            bitdata2=bitexpand4[mainram[baseaddr+offset+0x2000]*2+1]<<2;

            bitc2.w= bitdata1 | bitdata2 ;

            if(ioport[0xc0]&4) {

                bitc1.b[0]=colors_mk2_mode3[16+ bitc1.b[0]];
                bitc1.b[1]=colors_mk2_mode3[16+ bitc1.b[1]];
                bitc1.b[2]=colors_mk2_mode3[16+ bitc1.b[2]];
                bitc1.b[3]=colors_mk2_mode3[16+ bitc1.b[3]];
                bitc2.b[0]=colors_mk2_mode3[16+ bitc2.b[0]];
                bitc2.b[1]=colors_mk2_mode3[16+ bitc2.b[1]];
                bitc2.b[2]=colors_mk2_mode3[16+ bitc2.b[2]];
                bitc2.b[3]=colors_mk2_mode3[16+ bitc2.b[3]];
            } else {

                bitc1.b[0]=colors_mk2_mode3[bitc1.b[0]];
                bitc1.b[1]=colors_mk2_mode3[bitc1.b[1]];
                bitc1.b[2]=colors_mk2_mode3[bitc1.b[2]];
                bitc1.b[3]=colors_mk2_mode3[bitc1.b[3]];
                bitc2.b[0]=colors_mk2_mode3[bitc2.b[0]];
                bitc2.b[1]=colors_mk2_mode3[bitc2.b[1]];
                bitc2.b[2]=colors_mk2_mode3[bitc2.b[2]];
                bitc2.b[3]=colors_mk2_mode3[bitc2.b[3]];
            }
            

            vramindex=slice_x*8/4 + slice_y*VGA_PIXELS_X /4;

            *(vramptr+vramindex)   = bitc2.w;
            *(vramptr+vramindex+1) = bitc1.w;  
//            *(vramptr+vramindex + VGA_PIXELS_X/4)    = bitc2.w;
//            *(vramptr+vramindex + VGA_PIXELS_X/4 +1) = bitc1.w;  

        } else {  // Screen 4

            offset=(addr-baseaddr)&0x1fff;

            if(offset>=0x1f40) return;

            slice_x=offset%40;
            slice_y=offset/40;

            bitdata1=bitexpand[mainram[baseaddr+offset]*4];
            bitdata2=bitexpand[mainram[baseaddr+offset+0x2000]*4]<<1;

            bitc1.w=bitdata1|bitdata2;

            bitdata1=bitexpand[mainram[baseaddr+offset]*4+1];
            bitdata2=bitexpand[mainram[baseaddr+offset+0x2000]*4+1]<<1;

            bitc2.w=bitdata1|bitdata2;

            vramindex=slice_x*8/4 + slice_y*VGA_PIXELS_X /4;

            col3=(ioport[0xc0]&3)<<2;
            if(ioport[0xc0]&4) col3+=16;

            bitc1.b[0]=colors_mk2_mode3[bitc1.b[0]+col3];
            bitc1.b[1]=colors_mk2_mode3[bitc1.b[1]+col3];
            bitc1.b[2]=colors_mk2_mode3[bitc1.b[2]+col3];
            bitc1.b[3]=colors_mk2_mode3[bitc1.b[3]+col3];
            bitc2.b[0]=colors_mk2_mode3[bitc2.b[0]+col3];
            bitc2.b[1]=colors_mk2_mode3[bitc2.b[1]+col3];
            bitc2.b[2]=colors_mk2_mode3[bitc2.b[2]+col3];
            bitc2.b[3]=colors_mk2_mode3[bitc2.b[3]+col3];
            
            *(vramptr+vramindex)   = bitc2.w;
            *(vramptr+vramindex+1) = bitc1.w;  
//            *(vramptr+vramindex + VGA_PIXELS_X/4)    = bitc2.w;
//            *(vramptr+vramindex + VGA_PIXELS_X/4 +1) = bitc1.w;  

        }
    }

}

static inline void redraw(void){

    uint16_t baseaddr;

    if(ioport[0xc1]&2) {

        switch(ioport[0xb0]&0x6) {
            case 0:
                baseaddr=0xc000;
                break;
            case 0x2:
                baseaddr=0xe000;
                break;
            case 0x4:
                baseaddr=0x8000;
                break;
            case 0x6:
                baseaddr=0xa000;
                break;
            default:
                return;
        }

        video_cls();

        for(uint16_t offset=0;offset<512;offset++) {
            draw_framebuffer_p6(baseaddr+offset);
        }
        
    } else {

        switch(ioport[0xb0]&0x6) {
            case 0:
                baseaddr=0x8000;
                break;
            case 0x2:
                baseaddr=0xc000;
                break;
            case 0x4:
                baseaddr=0x0000;
                break;
            case 0x6:
                baseaddr=0x4000;
                break;
           default:
                baseaddr=0;
        }

        if(ioport[0xc1]&4) {

            for(uint16_t offset=0;offset<800;offset++) {
                draw_framebuffer_mk2(baseaddr+offset);
            }

        } else if(ioport[0xc1]&8) {

            for(uint16_t offset=0;offset<8000;offset++) {
                draw_framebuffer_mk2(baseaddr+offset);
            }

        } else {

            for(uint16_t offset=0;offset<16000;offset++) {
                draw_framebuffer_mk2(baseaddr+offset);
            }

        }


    }

}

//----------------------------------------------------------------------------------------------

void psg_reset(int flag) {


    psg_noise_seed = 12345;

    if (flag == 0) {
        for (int i = 0; i < 16; i++) {
            psg_register[i] = 0;
        }
    } else {
        for (int i = 0; i < 15; i++) {
            psg_register[i] = 0;
        }
    }
    psg_register[7] = 0xff;

    psg_noise_interval = UINT32_MAX;
    psg_envelope_interval = UINT32_MAX / 2 - 1;

    for (int i = 0; i < 3; i++) {
        psg_osc_interval[i] = UINT32_MAX;
        psg_tone_on[i] = 0;
        psg_noise_on[i] = 0;
    }


}

//----------------------------------------------------------------------------------------------------

static inline unsigned char tohex(int b) {

    if(b==0) {
        return '0';
    } 
    if(b<10) {
        return b+'1'-1;
    }
    if(b<16) {
        return b+'a'-10;
    }

    return -1;

}

static inline unsigned char fromhex(int b) {

    if(b=='0') {
        return 0;
    } 
    if((b>='1')&&(b<='9')) {
        return b-'1'+1;
    }
    if((b>='a')&&(b<='f')) {
        return b-'a'+10;
    }

    return -1;

}

// LittleFS

int pico_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t fs_start = XIP_BASE + HW_FLASH_STORAGE_BASE;
    uint32_t addr = fs_start + (block * c->block_size) + off;
    
//    printf("[FS] READ: %p, %d\n", addr, size);
    
    memcpy(buffer, (unsigned char *)addr, size);
    return 0;
}

int pico_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t fs_start = HW_FLASH_STORAGE_BASE;
    uint32_t addr = fs_start + (block * c->block_size) + off;
    
//    printf("[FS] WRITE: %p, %d\n", addr, size);
        
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();     // pause another core
    flash_range_program(addr, (const uint8_t *)buffer, size);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
        
    return 0;
}

int pico_erase(const struct lfs_config *c, lfs_block_t block)
{           
    uint32_t fs_start = HW_FLASH_STORAGE_BASE;
    uint32_t offset = fs_start + (block * c->block_size);
    
//    printf("[FS] ERASE: %p, %d\n", offset, block);
        
    uint32_t ints = save_and_disable_interrupts();   
    multicore_lockout_start_blocking();     // pause another core
    flash_range_erase(offset, c->block_size);  
    multicore_lockout_end_blocking();
    restore_interrupts(ints);

    return 0;
}

int pico_sync(const struct lfs_config *c)
{
    return 0;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config PICO_FLASH_CFG = {
    // block device operations
    .read  = &pico_read,
    .prog  = &pico_prog,
    .erase = &pico_erase,
    .sync  = &pico_sync,

    // block device configuration
    .read_size = FLASH_PAGE_SIZE, // 256
    .prog_size = FLASH_PAGE_SIZE, // 256
    
    .block_size = BLOCK_SIZE_BYTES, // 4096
    .block_count = HW_FLASH_STORAGE_BYTES / BLOCK_SIZE_BYTES, // 352
    .block_cycles = 16, // ?
    
    .cache_size = FLASH_PAGE_SIZE, // 256
    .lookahead_size = FLASH_PAGE_SIZE,   // 256    
};

// Keyboard

static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}

static inline int16_t getkeycode(uint8_t modifier,uint8_t keycode) {

    uint16_t p6code;

    if(modifier&0x11) {  // Control

        p6code=p6usbcode[keycode*5];

        if(p6code==0) return -1;

        if(p6code==0x40) return 0;
        if((p6code>=0x61)&&(p6code<=0x7a)) return p6code-0x60;
        if((p6code>=0x5b)&&(p6code<=0x5f)) return p6code-0x40;

        return -1;

    } else if (modifier&0x44) { // ALT=>Graph

        p6code=p6usbcode[keycode*5+4];

        if(p6code!=0) return p6code;
        return -1;

    } else {

        if(key_kana) {
            if(modifier&0x22) {
                p6code=p6usbcode[keycode*5+3];
            } else {
                p6code=p6usbcode[keycode*5+2];
            }

            if (key_hirakata==0) { // Hiragana
                if((p6code>=0xc0)&&(p6code<0xde)) return p6code+0x20;
                if((p6code>=0xb1)&&(p6code<0xc0)) return p6code-0x20;
            } 

            if(p6code!=0) return p6code;
            return -1;

        } else {
            if(modifier&0x22) {
                p6code=p6usbcode[keycode*5+1];
            } else {
                p6code=p6usbcode[keycode*5];
            }

            if(p6code==0) return -1;

            return p6code;

        }
    }

    return -1;
}


void process_kbd_report(hid_keyboard_report_t const *report) {

    int usbkey;
    int16_t p6code;

    if(menumode==0) { // Emulator mode

        key_repeat_count=0;

        p6keypad=0;

        if(report->modifier!=lastmodifier) {  // stop auto repeat when modifier changed

            if(key_repeat_flag) {
                key_repeat_count=0;
            }
        }

        lastmodifier=report->modifier;


        if(report->modifier&0x22) {
            p6keypad|=0x1;
        }

        for(int i=0;i<6;i++) {

            if ( report->keycode[i] )
                {

                // GamePad
                keypressed=report->keycode[i];   
                if(keypressed==0x4f) { p6keypad|=0x10; } // Right
                if(keypressed==0x50) { p6keypad|=0x20; } // Left
                if(keypressed==0x51) { p6keypad|=0x08; } // Down
                if(keypressed==0x52) { p6keypad|=0x04; } // Up
                if(keypressed==0x2c) { p6keypad|=0x80; } // Space
                if(keypressed==0x48) { p6keypad|=0x02; } // STOP

                if ( find_key_in_report(&prev_report, report->keycode[i]) )
                {
                // exist in previous report means the current key is holding
                }else
                {


                keypressed=report->keycode[i];   
                p6code=getkeycode(report->modifier,report->keycode[i]);

                if(p6code!=-1) {

                    p6keypressed=p6code;

                    // Special Keys

                    if(p6code>0xff) {

                        if((subcpu_enable_irq==0)&&(subcpu_command_processing==0)) {
                            if((ioport[0xf3]&1)==0) {
                            subcpu_enable_irq=1;
                            subcpu_irq_processing=0;
                            subcpu_ird=0x14;

                            subcpu_irq_delay=2;

                            key_repeat_count=scanline;

                            }
                        } else {

                            if(p6code==0x1fa) {    // STOP in TAPE MODE

                                if(subcpu_command_processing==0x19) { // load command
                                    subcpu_enable_irq=1;
                                    subcpu_irq_processing=0;
                                    subcpu_ird=0x10;
                                    subcpu_irq_delay=2;
                                }
                                if(subcpu_command_processing==0x39) { // save command
                                    subcpu_enable_irq=1;
                                    subcpu_irq_processing=0;
                                    subcpu_ird=0x0e;
                                    subcpu_irq_delay=2;
                                }
                            }
                        }

                   } else {

                    // Send IRQ

                        if((subcpu_enable_irq==0)&&(subcpu_command_processing==0)) {
                            if((ioport[0xf3]&1)==0) {
                            subcpu_enable_irq=1;
                            subcpu_irq_processing=0;
                            subcpu_ird=0x2;
                            subcpu_irq_delay=20;
                            subcpu_irq_delay=2;

                            key_repeat_count=scanline;

                            }
                        }

                   }

                    if(key_repeat_flag) {
                        key_repeat_count=scanline;
                    }

                }

                // CapsLock

                if(keypressed==0x39) {
                    if(key_caps==0) {
                        key_caps=1;
                    } else {
                        key_caps=0;
                    }
//                    process_kbd_leds();
                }

                // Kana

                if(keypressed==0x88) {
                    if(key_kana==0) {
                        key_kana=1;
                    } else {
                        key_kana=0;
                    }
//                    process_kbd_leds();
                }

                // Shift-Page (Hira-Kata)

                if(keypressed==0x4b) {
                    if(report->modifier&0x22) {
                        if(key_hirakata==1) {
                            key_hirakata=0;
                        } else {
                            key_hirakata=1;
                        }

                    }
                }

            // Enter Menu
            if(report->keycode[i]==0x45) {
                prev_report=*report;
                menumode=1;
                keypressed=0;
            }  

                }
            }   

        }

    prev_report=*report;

} else {  // menu mode

    for(uint8_t i=0; i<6; i++)
    {
        if ( report->keycode[i] )
        {
        if ( find_key_in_report(&prev_report, report->keycode[i]) )
        {
            // exist in previous report means the current key is holding
        }else
        {
            keypressed=report->keycode[i];
        }
        }
    } 
    prev_report = *report;
    }

}


//
// Subcpu simulation
//

void subcpu_command(uint8_t command) {

    // printf("[!C:%x]",command);

    // if(subcpu_enable_irq) {
    //     return;
    // }

    switch(subcpu_command_processing) {

        case 0x19: // Tape In MODE
            if(command==0x1a) {
                subcpu_command_processing=0;
                if((load_enabled)&&(tape_autoclose)) {
                    load_enabled=0;
                    lfs_file_close(&lfs,&lfs_file);
                }
                return;
            }

            return;

        case 0x38: // Tape Out

            tapeout(command);
            subcpu_command_processing=0x39;

            return;

        case 0x39: // Tape Out MODE

            if(command==0x3a) {
                subcpu_command_processing=0;
                if((save_enabled)&&(tape_autoclose)) {
                    save_enabled=0;
                    lfs_file_close(&lfs,&lfs_file);
                }


                return;
            }
            if(command==0x38) {
                subcpu_command_processing=0x38;
                return;
            }

            return;

        default:
            break;

    }

    switch(command) {

        case 0x6:
            if((ioport[0xf3]&1)==0) {
            subcpu_command_processing=command;
            subcpu_enable_irq=1;
            subcpu_irq_processing=0;
            subcpu_ird=0x16;
            subcpu_irq_delay=20;
            }
            return;

        case 0x19:
        case 0x39:
            subcpu_command_processing=command;
            tape_read_wait=TAPE_WAIT;
            return;

        case 0x1d:
        case 0x1e:
        case 0x3d:
        case 0x3e:

            return;

        default:
            return;
    }
}

uint8_t subcpu_response() {

//    printf("[SUBCPU:%02x]",subcpu_ird);

    switch(subcpu_ird) {

        case 0x2: // Keyboard
        case 0x14:
            subcpu_ird=0xff;
            subcpu_irq_processing=0;
            subcpu_enable_irq=0;
            z80_int(&cpu,FALSE);
            subcpu_command_processing=0;
            return (p6keypressed&0xff);

        case 0x08: // Tape Read

            subcpu_ird=0xff;
            subcpu_irq_processing=0;
            subcpu_enable_irq=0;
            z80_int(&cpu,FALSE);
            subcpu_command_processing=0x19;
            tape_read_wait=TAPE_WAIT;
            return tapein();

        case 0x16: // Gamepad
            subcpu_ird=0xff;
            subcpu_irq_processing=0;
            subcpu_enable_irq=0;
            z80_int(&cpu,FALSE);
            subcpu_command_processing=0;
            return p6keypad;

    }

    return 0;

}

void subcpu_control(uint8_t command) {

}

uint8_t subcpu_status(void) {

    // bit3 Inttrupt clear (Ready)
    // bit5 Input data ready
    // bit7 Output ready

    return (ioport[0x92]|0xa8);

}

// Voice(dummy)

// void voice_control(uint8_t command) {

// }

// uint8_t voice_status(void) {

// }

//

static uint8_t mem_read(void *context,uint16_t address)
{

    uint8_t b;
    uint8_t bank;

    if((ioport[0x92]&4)==0){  // CGROM
        if((address>=0x6000)&&(address<0x8000)) {
            return cgrom2[address&0x1fff];
        }
    }

    if(address<0x4000) {
        bank=ioport[0xf0]&0xf;
    } else if(address<0x8000) {
        bank=(ioport[0xf0]&0xf0)>>4;
    } else if(address<0xc000) {
        bank=ioport[0xf1]&0xf;
    } else {
        bank=(ioport[0xf1]&0xf0)>>4;
    }

    switch(bank) {
        case 0:
        case 0xf:
            return 0xff;

        case 1: // Basic ROM
            return basicrom[address&0x7fff];

        case 2: // Voice/Kanji ROM

            if(ioport[0xc2]&1) {
                if(ioport[0xc2]&2) {
                    return kanjirom[(address&0x3fff)+0x4000];
                } else {
                    return kanjirom[(address&0x3fff)];
                }
            } else {
                return voicerom[address&0x3fff];
            }

        case 3: // ExtROM (dummy)
        case 8:

            return 0xff;

        case 4:  // ExtROM
        case 7:

#ifdef USE_EXT_ROM
            return extrom[(address&0x1fff) + extbank*0x2000];
#else

            return 0xff;
#endif

        case 5:  // Kanji/Voice - Basic

            if((address&0x3fff)>=0x2000) {
                return basicrom[address&0x7fff];
            } else {

                if(ioport[0xc2]&1) {
                    if(ioport[0xc2]&2) {
                        return kanjirom[(address&0x3fff)+0x4000];
                    } else {
                        return kanjirom[(address&0x3fff)];
                    }
                } else {
                    return voicerom[address&0x3fff];
                }
            }

        case 6:  // Basic-Kanji/Voice

            if((address&0x3fff)<0x2000) {
                return basicrom[address&0x7fff];
            } else {
                if(ioport[0xc2]&1) {
                    if(ioport[0xc2]&2) {
                        return kanjirom[(address&0x3fff)+0x4000];
                    } else {
                        return kanjirom[(address&0x3fff)];
                    }
                } else {
                    return voicerom[address&0x3fff];
                }
            }

        case 9:  // Ext-Basic

            if((address&0x3fff)>=0x2000) {
                return basicrom[address];
            } else {
                return 0xff;
            }

        case 0xa:  // Basic-Ext

            if((address&0x3fff)>=0x2000) {
                return 0xff;
            } else {
                return basicrom[address];
            }

        case 0xb:   // Ext-Kanji

        case 0xc:   // Kanji-Ext

            return 0xff;

        case 0xd:  // Main RAM
            return mainram[address];

        case 0xe: // Ext RAM
#ifdef USE_EXT_ROM
            return extram[address&0x3fff];
#else 
            return 0xff;
#endif
    }

}

static void mem_write(void *context,uint16_t address, uint8_t data)
{

    uint8_t bank,permit;

    if(address<0x4000) {
        permit=ioport[0xf2]&3;
    } else if(address<0x8000) {
        permit=(ioport[0xf2]&0xc)>>2;
    } else if(address<0xc000) {
        permit=(ioport[0xf2]&0x30)>>4;
    } else {
        permit=(ioport[0xf2]&0xc0)>>6;
    }


            if(permit&1) {
                mainram[address]=data;

                if(ioport[0xc1]&2) {
                    draw_framebuffer_p6(address);
                } else {
                    draw_framebuffer_mk2(address);
                }
            }

#ifdef USE_EXT_ROM
            else if (permit&2) {
                extram[address&0x3fff]=data;
            }
#endif

            return;

}

static uint8_t io_read(void *context, uint16_t address)
{
    uint8_t data = ioport[address&0xff];
    uint8_t b;
    uint32_t kanji_addr;


    // if((address&0xf0)==0xa0) {
    // printf("[IOR:%04x:%02x]",Z80_PC(cpu),address&0xff);
    // }

    switch(address&0xff) {

        // Sub CPU interface

        case 0x90:     // i8255 Port A to Sub CPU
        case 0x94:
        case 0x98:
        case 0x9C:

            return subcpu_response();

        case 0x92:     // i8255 Port C to Sub CPU
        case 0x96:
        case 0x9a:
        case 0x9e:

            return subcpu_status();

        case 0x93:      // i8255 contol word (may be 0xff ?)
        case 0x97:
        case 0x9b:
        case 0x9f:

            return 0xff;

        case 0xa2:    // PSG Read
        case 0xa6:
        case 0xaa:
        case 0xae:

            if(ioport[0xa0]<0x0e) {
                return psg_register[ioport[0xa0]&0xf];
            } 

            return 0x3f;

        case 0xa3:    // PSG inactive
        case 0xa7:
        case 0xab:
        case 0xaf:

            return 0xff;

        // Intelligent floppy interface

        case 0xd2:
        case 0xd6:
        case 0xda:
        case 0xde:

            return 0xff;

        // Voice Synth

        case 0xe0:
        case 0xe4:
        case 0xe8:
        case 0xec:

            return 0x40;

        // Additional KANJI ROM

        case 0xfc:
        case 0xfd:
        case 0xfe:
        case 0xff:

            return 0xff;

        default:
            return ioport[address&0xff];

    }

//   return 0xff;
}

static void io_write(void *context, uint16_t address, uint8_t data)
{

    uint8_t b;

    // if((address&0xf0)==0xa0) {
    // printf("[IOW:%04x:%02x:%02x]",Z80_PC(cpu),address&0xff,data);
    // }
    // if((address&0xf0)==0xf0) {
    // printf("[IOW:%04x:%02x:%02x]",Z80_PC(cpu),address&0xff,data);
    // }

    switch(address&0xff) {

#ifdef USE_EXT_ROM

        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7a:
        case 0x7b:
        case 0x7c:
        case 0x7d:
        case 0x7e:
        case 0x7f:

            extbank=data&0xf;
            return;

#endif

        case 0x90:     // i8255 Port A to Sub CPU
        case 0x94:
        case 0x98:
        case 0x9c:
            subcpu_command(data);
            return;

        case 0x92:     // i8255 Port C to Sub CPU
        case 0x96:
        case 0x9a:
        case 0x9e:

            ioport[0x92]=data;
            subcpu_control(data);
            return;

        case 0x93:     // i8255 control to Sub CPU
        case 0x97:
        case 0x9b:
        case 0x9f:

            if((data&0x80)==0) { // Bit operation

                b=(data&0x0e)>>1;

                if(data&1) { // set
                    ioport[0x92]|=(1<<b);
                } else { // reset
                    ioport[0x92]&=~(1<<b);

                }

                subcpu_control(ioport[0x92]);
                
                return;
            }

            return;

        case 0xa0: // PSG address
        case 0xa4:
        case 0xa8:
        case 0xac:

            ioport[0xa0]=data;

            return;

        case 0xa1: // PSG write data
        case 0xa5:
        case 0xa9:
        case 0xad:

            psg_write(data);

            return;

        case 0xa3: // PSG inactive
        case 0xa7:
        case 0xab:
        case 0xaf:

            return;

        case 0xb0:  // screen base address
        case 0xb4:    
        case 0xb8:
        case 0xbc:
        
            if((ioport[0xb0]&6)!=(data&6)) {
                ioport[0xb0]=data;
                redraw();
                return;
            }
            ioport[0xb0]=data;

            return;

        case 0xc0:  // CSS
        case 0xc4:
        case 0xca:
        case 0xcc:

            if((ioport[0xc0]&0x7)!=(data&0x7)) {
                ioport[0xc0]=data;
                redraw();
                return;
            }
            ioport[0xc0]=data;

            return;

        case 0xc1:  // screen mode
        case 0xc5: 
        case 0xc9: 
        case 0xcd: 

            if((ioport[0xc1]&0xe)!=(data&0xe)) {
                ioport[0xc1]=data;
                redraw();
                return;
            }
            ioport[0xc1]=data;

            return;

        default:
            ioport[address&0xff]=data;
            return;

    }


}

static uint8_t ird_read(void *context,uint16_t address) {

//  printf("INT:%d:%d:%d:%02x:%02x\n\r",cpu.im,pioa_enable_irq,pio_irq_processing,cpu.i,pioa[0]);

    if(cpu.im==2) { // mode 2

        if(timer_enable_irq) {
            timer_enable_irq=0;
            z80_int(&cpu,FALSE);
            return ioport[0xf7];    
        }

        if((subcpu_enable_irq)&&(subcpu_irq_processing==0)) {

            if(subcpu_ird==0) {  
                subcpu_ird=0xff;
                subcpu_irq_processing=0;
                subcpu_enable_irq=0;
                z80_int(&cpu,FALSE);
                return 0;
            }

//            printf("INT:%02x", subcpu_ird);
            z80_int(&cpu,FALSE); 
            subcpu_irq_processing=1;
            return subcpu_ird;
                    
        } 
        
        else {

            printf("[!INT:%d %x]",subcpu_command_processing,subcpu_ird);

        }
    } 

    return 0xff;

}

static void reti_callback(void *context) {

//    printf("RETI");

    // subcpu_enable_irq=0;
    // timer_enable_irq=0;
    // subcpu_irq_processing=0;
    // subcpu_command_processing=0;
    // subcpu_ird=0xff;

    // z80_int(&cpu,FALSE);

}

void main_core1(void) {

    uint32_t redraw_start,redraw_length;

    multicore_lockout_victim_init();

    scanline=0;

    // set virtual Hsync timer

    add_repeating_timer_us(63,hsync_handler,NULL  ,&timer);

    // // set PSG timer

    // add_repeating_timer_us(1000000/SAMPLING_FREQ,sound_handler,NULL  ,&timer2);

    while(1) { // Wait framebuffer redraw command


    }
}

int main() {

    uint32_t menuprint=0;
    uint32_t filelist=0;
    uint32_t subcpu_wait;

    static uint32_t hsync_wait,vsync_wait;

    set_sys_clock_khz(DOTCLOCK * CLOCKMUL ,true);

    stdio_init_all();

    uart_init(uart0, 115200);

    initVGA();

    gpio_set_function(12, GPIO_FUNC_UART);
    gpio_set_function(13, GPIO_FUNC_UART);

    // gpio_set_slew_rate(0,GPIO_SLEW_RATE_FAST);
    // gpio_set_slew_rate(1,GPIO_SLEW_RATE_FAST);
    // gpio_set_slew_rate(2,GPIO_SLEW_RATE_FAST);
    // gpio_set_slew_rate(3,GPIO_SLEW_RATE_FAST);
    // gpio_set_slew_rate(4,GPIO_SLEW_RATE_FAST);

    gpio_set_drive_strength(2,GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(3,GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(4,GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(5,GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(6,GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(7,GPIO_DRIVE_STRENGTH_2MA);

    // Beep & PSG

    gpio_set_function(10,GPIO_FUNC_PWM);
 //   gpio_set_function(11,GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(10);

    pwm_set_wrap(pwm_slice_num, 256);
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, 0);
//    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_B, 0);
    pwm_set_enabled(pwm_slice_num, true);

    // set PSG timer

    add_repeating_timer_us(1000000/SAMPLING_FREQ,sound_handler,NULL  ,&timer2);

    tuh_init(BOARD_TUH_RHPORT);

    video_cls();

    video_hsync=0;
    video_vsync=0;

    // video_mode=0;
    // fbcolor=0x7;

// uart handler

    // irq_set_exclusive_handler(UART0_IRQ,uart_handler);
    // irq_set_enabled(UART0_IRQ,true);
    // uart_set_irq_enables(uart0,true,false);

    multicore_launch_core1(main_core1);

    multicore_lockout_victim_init();

    sleep_ms(1);

// mount littlefs
    if(lfs_mount(&lfs,&PICO_FLASH_CFG)!=0) {
       cursor_x=0;
       cursor_y=0;
       fbcolor=7;
       video_print("Initializing LittleFS...");
       // format
       lfs_format(&lfs,&PICO_FLASH_CFG);
       lfs_mount(&lfs,&PICO_FLASH_CFG);
   }

    menumode=1;  // Pause emulator

//  setup emulator 

    // Initial Bank selection
    ioport[0xf0]=0x71;
    ioport[0xf1]=0xdd;
    ioport[0xf2]=0xff;

    key_kana=0;
    key_caps=0;
    key_hirakata=0;
//

    psg_reset(0);

    tape_ready=0;

    cpu.read = mem_read;
    cpu.write = mem_write;
    cpu.in = io_read;
    cpu.out = io_write;
	cpu.fetch = mem_read;
    cpu.fetch_opcode = mem_read;
    cpu.reti = reti_callback;
    cpu.inta = ird_read;

    z80_power(&cpu,true);
    z80_instant_reset(&cpu);

    cpu_hsync=0;
    cpu_cycles=0;

    // start emulator
    
    menumode=0;

    while(1) {

        if(menumode==0) { // Emulator mode

        cpu_cycles += z80_run(&cpu,1);
        cpu_clocks++;

        //         printf("[%04x]",Z80_PC(cpu));


            // cursor_x=0;
            // cursor_y=19;
            // uint8_t str[64];
            // sprintf(str,"%04x %04x %04x %04x %02x",Z80_PC(cpu),Z80_BC(cpu),Z80_DE(cpu),Z80_HL(cpu),mainram[Z80_DE(cpu)]);
            // video_print(str);

        if(timer_enable_irq) {
            if((cpu.iff1)&&(cpu.im==2)) {
                z80_int(&cpu,TRUE);
            }
        }


        if((subcpu_enable_irq)&&(subcpu_irq_processing==0)) {

                // check 0xf3 ?

            subcpu_irq_delay--;

            if(subcpu_irq_delay==0) {

            if((cpu.iff1)&&(cpu.im==2)) {
                z80_int(&cpu,TRUE);
            }

            }
    
        }

        if((load_enabled)&&(subcpu_enable_irq==0)&&(subcpu_command_processing==0x19)) {

            if(tape_read_wait>1) {
                tape_read_wait--;
            } else {

                if((cpu.iff1)&&(cpu.im==2)) {
                    subcpu_enable_irq=1;
                    subcpu_irq_processing=0;
                    subcpu_ird=0x08;
                    subcpu_irq_delay=1;
                    z80_int(&cpu,TRUE);
                }

            }
        }

        // Wait

//        if((cpu_cycles-cpu_hsync)>82 ) { // 63us * 3.58MHz = 227
        if((cpu_cycles-cpu_hsync)>113 ) { // 63us * 3.58MHz = 227

            while(video_hsync==0) ;
            cpu_hsync=cpu_cycles;
            video_hsync=0;
        }

        // if(video_hsync==1) {
        //     hsync_wait++;
        //     if(hsync_wait>30) {
        //         video_hsync=0;
        //         hsync_wait=0;
        //     }
        // }
        
        if((video_vsync)==1) { // Timer
            tuh_task();
            video_vsync=2;
            vsync_scanline=scanline;

            if((key_repeat_flag)&&(key_repeat_count!=0)) {
                if((scanline-key_repeat_count)==40*262) {

                    if(p6keypressed>0xff) {
                        if((subcpu_enable_irq==0)&&(subcpu_command_processing==0)) {
                            if((ioport[0xf3]&1)==0) {
                            subcpu_enable_irq=1;
                            subcpu_irq_processing=0;
                            subcpu_ird=0x14;
                            subcpu_irq_delay=2;
                            }
                        } 

                   } else {

                    // Send IRQ

                        if((subcpu_enable_irq==0)&&(subcpu_command_processing==0)) {
                            if((ioport[0xf3]&1)==0) {
                            subcpu_enable_irq=1;
                            subcpu_irq_processing=0;
                            subcpu_ird=0x2;
                            subcpu_irq_delay=20;
                            subcpu_irq_delay=2;
                            }
                        }
                   }



                } else if (((scanline-key_repeat_count)>40*262)&&((scanline-key_repeat_count)%(4*262)==0)) {


                    if(p6keypressed>0xff) {
                        if((subcpu_enable_irq==0)&&(subcpu_command_processing==0)) {
                            if((ioport[0xf3]&1)==0) {
                            subcpu_enable_irq=1;
                            subcpu_irq_processing=0;
                            subcpu_ird=0x14;
                            subcpu_irq_delay=2;

                            }
                        } 

                   } else {

                    // Send IRQ

                        if((subcpu_enable_irq==0)&&(subcpu_command_processing==0)) {
                            if((ioport[0xf3]&1)==0) {
                            subcpu_enable_irq=1;
                            subcpu_irq_processing=0;
                            subcpu_ird=0x2;
                            subcpu_irq_delay=20;
                            subcpu_irq_delay=2;
                            }
                        }
                   }

                }
            }            



        }

        if(video_vsync==2) {
            if(scanline>(vsync_scanline+60)) {
                video_vsync=0;
            }
        }

        } else { // Menu Mode


            unsigned char str[80];

            fbcolor=7;
            
            if(menuprint==0) {

                draw_menu();
                menuprint=1;
                filelist=0;
            }

            cursor_x=3;
            cursor_y=6;
            video_print("MENU");

            uint32_t used_blocks=lfs_fs_size(&lfs);
            sprintf(str,"Free:%d Blocks",(HW_FLASH_STORAGE_BYTES/BLOCK_SIZE_BYTES)-used_blocks);
            cursor_x=3;
            cursor_y=7;
            video_print(str);

            sprintf(str,"TAPE:%x",tape_ptr);
            cursor_x=3;
            cursor_y=8;
            video_print(str);

            cursor_x=3;            
            cursor_y=9;
            if(menuitem==0) { fbcolor=0x70; } else { fbcolor=7; } 
            if(save_enabled==0) {
                video_print("SAVE: empty");
            } else {
                sprintf(str,"SAVE: %8s",tape_filename);
                video_print(str);
            }
            cursor_x=3;
            cursor_y=10;

            if(menuitem==1) { fbcolor=0x70; } else { fbcolor=7; } 
            if(load_enabled==0) {
                video_print("LOAD: empty");
            } else {
                sprintf(str,"LOAD: %8s",tape_filename);
                video_print(str);
            }

            cursor_x=3;
            cursor_y=11;

            if(menuitem==2) { fbcolor=0x70; } else { fbcolor=7; } 
            if(tape_autoclose) {
                 video_print("AutoClose: On");
            } else {
                 video_print("AutoClose: Off");
            }

            cursor_x=3;
            cursor_y=12;

            if(menuitem==3) { fbcolor=0x70; } else { fbcolor=7; } 
            if(colormode==0) {
                video_print("Color: mk2  ");
            } else if (colormode==1) {
                video_print("Color: P6RGB");
            } else {
                video_print("Color: P6TV ");
            }


            cursor_x=3;
            cursor_y=13;

            if(menuitem==4) { fbcolor=0x70; } else { fbcolor=7; } 
            video_print("DELETE File");

            cursor_x=3;
            cursor_y=16;

            if(menuitem==5) { fbcolor=0x70; } else { fbcolor=7; } 
            video_print("Reset");


            cursor_x=3;
            cursor_y=17;

            if(menuitem==6) { fbcolor=0x70; } else { fbcolor=7; } 
            video_print("PowerCycle");

// TEST

            // cursor_x=3;
            //  cursor_y=17;
            //      sprintf(str,"%04x %x %04x %04x %x %04x %04x",Z80_PC(cpu),i8253[1],i8253_counter[1],i8253_preload[1],i8253[2],i8253_counter[2],i8253_preload[2]);
            //      video_print(str);

            // cursor_x=3;
            //  cursor_y=17;
            //      sprintf(str,"%04x %04x %04x %04x %04x",Z80_PC(cpu),Z80_AF(cpu),Z80_BC(cpu),Z80_DE(cpu),Z80_HL(cpu));
            //      video_print(str);

            // cursor_x=3;
            //  cursor_y=18;
            //  uint16_t sp=Z80_SP(cpu);
            //      sprintf(str,"%04x %04x %04x",mainram[sp]+256*mainram[sp+1],mainram[sp+2]+256*mainram[sp+3],mainram[sp+4]+256*mainram[sp+5]);
            //      video_print(str);


            if(filelist==0) {
                draw_files(-1,0);
                filelist=1;
            }
     
            while(video_vsync==0);

            video_vsync=0;

                tuh_task();

                if(keypressed==0x52) { // Up
                    keypressed=0;
                    if(menuitem>0) menuitem--;
                }

                if(keypressed==0x51) { // Down
                    keypressed=0;
                    if(menuitem<6) menuitem++; 
                }

                if(keypressed==0x28) {  // Enter
                    keypressed=0;

                    if(menuitem==0) {  // SAVE

                        if((load_enabled==0)&&(save_enabled==0)) {

                            uint32_t res=enter_filename();

                            if(res==0) {
                                memcpy(tape_filename,filename,16);
                                lfs_file_open(&lfs,&lfs_file,tape_filename,LFS_O_RDWR|LFS_O_CREAT);
                                save_enabled=1;
                                // tape_phase=0;
                                tape_ptr=0;
                                // tape_count=0;
                            }

                        } else if (save_enabled!=0) {
                            lfs_file_close(&lfs,&lfs_file);
                            save_enabled=0;
                        }
                        menuprint=0;
                    }

                    if(menuitem==1) { // LOAD

                        if((load_enabled==0)&&(save_enabled==0)) {

                            uint32_t res=file_selector();

                            if(res==0) {
                                memcpy(tape_filename,filename,16);
                                lfs_file_open(&lfs,&lfs_file,tape_filename,LFS_O_RDONLY);
                                load_enabled=1;
                                // tape_phase=0;
                                tape_ptr=0;
                                // tape_count=0;
//                                file_cycle=cpu.PC;
                            }
                        } else if(load_enabled!=0) {
                            lfs_file_close(&lfs,&lfs_file);
                            load_enabled=0;
                        }
                        menuprint=0;
                    }

                    if(menuitem==2) { // Tape Autoclose

                        if(tape_autoclose) {
                            tape_autoclose=0;
                        } else {
                            tape_autoclose=1;
                        }

                        menuprint=0;

                    }

                    if(menuitem==3) { // colormode

                        colormode++;

                        if(colormode>2) {
                            colormode=0;
                        }

                        menuprint=0;

                    }


                    if(menuitem==4) { // Delete

                        if((load_enabled==0)&&(save_enabled==0)) {
                            uint32_t res=enter_filename();

                            if(res==0) {
                                lfs_remove(&lfs,filename);
                            }
                        }

                        menuprint=0;

                    }

                    if(menuitem==5) { // Reset
                        menumode=0;
                        menuprint=0;
                        redraw();
                    
                        key_caps=0;
                        key_kana=0;
                        key_hirakata=0;

#ifdef USE_EXT_ROM
                        extbank=0;
#endif

                        psg_reset(0);

                        z80_instant_reset(&cpu);

                    }

                    if(menuitem==6) { // PowerCycle
                        menumode=0;
                        menuprint=0;

                        memset(mainram,0,0x10000);
                        memset(ioport,0,0x100);

                        ioport[0xf0]=0x71;
                        ioport[0xf1]=0xdd;
                        ioport[0xf2]=0xff;

                        key_caps=0;
                        key_kana=0;
                        key_hirakata=0;

#ifdef USE_EXT_ROM
                        extbank=0;
#endif

                        redraw();

                        psg_reset(0);

//                        z80_instant_reset(&cpu);
                        z80_power(&cpu,true);

                    }



                }

                if(keypressed==0x45) {
                    keypressed=0;
                    menumode=0;
                    menuprint=0;
                    redraw();
                //  break;     // escape from menu
                }

        }


    }

}
