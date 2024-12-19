#include "fdc.h"

uint8_t fdc_command_buffer[10];
uint8_t fdc_result_buffer[8];
uint8_t fdc_command_write_index=0;
uint8_t fdc_command_read_index=0;
uint8_t fdc_command_read_length=0;
uint8_t fdc_interrupt_flag=0;
uint8_t fdc_command_drive=0;
uint8_t fdc_command_cylinder[4];
uint8_t fdc_command_head[4];
uint8_t fdc_command_eot;
uint8_t fdc_dma_flag=0;
uint32_t fdc_read_count;
uint32_t fdc_read_sector_size;
uint32_t fdc_write_count;
uint32_t fdc_write_sector_size;
uint8_t fdc_exec_phase_finish;
uint8_t *fdc_dma_buffer;
uint32_t fdc_dma_datasize;
uint8_t fdc_phase_flag=0;      // 0. Write 1. Execute 2. Read
const uint8_t fdc_command_length[]={ 1,9,9,3,2,9,9,2,1,9,2,1,9,6,1,3};
const uint32_t fdc_sector_count[]={128,256,512,1024,2048,4096};

lfs_t lfs_handler;
lfs_file_t fd_drive[4];
uint8_t fd_drive_status[4];

void fdc_init(uint8_t *buffer) {

    fdc_command_drive=0;
    for(int i=0;i<4;i++) {
        fdc_command_cylinder[0]=0;
    }
    fdc_dma_flag=0;
    fdc_exec_phase_finish=0;

    fdc_dma_buffer=buffer;

    return;
}

uint8_t fdc_status(void) {

    uint8_t fdc_status;

    fdc_status=0;

    if(fdc_command_write_index!=0) {
        fdc_status|=0x10;
    }
    if(fdc_command_read_index!=0) {
        fdc_status|=0x10;
    }
    // if(fdc_phase_flag==1) {
    //     fdc_status|=0x10;
    //     if(fdc_exec_phase_finish==1) {
    //         fdc_phase_flag=2;
    //     }
    // } else
     if(fdc_phase_flag==2) {
        fdc_status|=0x40;
    }

    fdc_status|=0x80;

    // printf("[FS:%x]",fdc_status);

    return fdc_status;

}

// check first posision of sector

int32_t fdc_find_sector(uint8_t driveno,uint8_t track,uint8_t head,uint8_t sector,uint8_t number) {

    uint8_t count;
    uint8_t sector_info[16];
    uint32_t sector_ptr;

    lfs_file_seek(&lfs_handler,&fd_drive[driveno],0x20,LFS_SEEK_SET);

    // find track top

    for(int i=0;i<=track*2;i++) {
        lfs_file_read(&lfs_handler,&fd_drive[driveno],&sector_ptr,4);
    }

//printf("[D88T:%d,%x]",driveno,sector_ptr);

    lfs_file_seek(&lfs_handler,&fd_drive[driveno],sector_ptr,LFS_SEEK_SET);

    while(1) {
        sector_ptr+=0x10;
        lfs_file_read(&lfs_handler,&fd_drive[driveno],sector_info,16);

//printf("[D88S:%x,%x,%x,%x]",sector_info[0],sector_info[1],sector_info[2],sector_info[3]);


        if((sector_info[2]==sector)&&(sector_info[1]==head)&&(sector_info[0]==track)) {
            // if(sector_info[3]==0) {
            //     fd_sector_size=128;
            // } else if(sector_info[3]==1) {
            //     fd_sector_size=256;
            // } else if(sector_info[3]==2) {
            //     fd_sector_size=512;
            // } else if(sector_info[3]==3) {
            //     fd_sector_size=1024;
            // }
            // break;
            if(sector_info[3]==number) {
                break;
            }
        }
        if(sector_info[3]==0) {
            lfs_file_seek(&lfs_handler,&fd_drive[driveno],128,LFS_SEEK_CUR);
            sector_ptr+=128;
        } else if(sector_info[3]==1) {
            lfs_file_seek(&lfs_handler,&fd_drive[driveno],256,LFS_SEEK_CUR);
            sector_ptr+=256;
        } else if(sector_info[3]==2) {
            lfs_file_seek(&lfs_handler,&fd_drive[driveno],512,LFS_SEEK_CUR);
            sector_ptr+=512;
        } else if(sector_info[3]==3) {
            lfs_file_seek(&lfs_handler,&fd_drive[driveno],1024,LFS_SEEK_CUR);
            sector_ptr+=1024;
        }

        count++;
        if(count>40) {

            return -1;

            break;
        } // error        
    }

//    fd_ptr=sector_ptr;
//    fd_sector_bytes=0;

    return sector_ptr;

}

uint8_t fdc_read(uint8_t driveno,int32_t fd_ptr) {

    uint8_t data;

    if(fd_drive_status[driveno]==0) return 0xff;

// finish read
//    if(fdc_read_count==fd_sector_size) {
//        fdc_status=0;
//    }

    lfs_file_read(&lfs_handler,&fd_drive[driveno],&data,1);

    fdc_read_count++;

    return data;

}

uint8_t fdc_read_dma(uint8_t driveno,int32_t fd_ptr,uint8_t *buffer) {

    uint8_t data;

    if(fd_drive_status[driveno]==0) return 0xff;

    lfs_file_read(&lfs_handler,&fd_drive[driveno],buffer,fdc_read_sector_size);

    fdc_read_count+=fdc_read_sector_size;

    return data;

}


void fdc_command_write(uint8_t data) {

    uint8_t fdc_command;
    int32_t fdc_read_ptr;
    uint8_t fdc_st3;
    uint32_t fdc_dma_offset;

    fdc_command_buffer[fdc_command_write_index++]=data;

    if(fdc_command_length[fdc_command_buffer[0]&0xf]<=fdc_command_write_index) {

        fdc_command_write_index=0;

        // execute command

//        printf("[FDC:%x]\n",fdc_command_buffer[0]&0xf);

        switch(fdc_command_buffer[0]&0xf) {

            case 0x3: // SPECIFY

                // Set DMA flag

                if(fdc_command_buffer[2]&1) {
                    fdc_dma_flag=0;
                } else {
                    fdc_dma_flag=1;
                }

                fdc_phase_flag=0;
                return;

            case 0x5: // WRITE DATA 
            // Return Write Protected

            if(fdc_dma_flag) {

                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                // Result status

                fdc_command_read_length=7;
                fdc_command_read_index=0;

                fdc_result_buffer[0]=0x40 | fdc_command_drive;  // Abort
                fdc_result_buffer[1]=0x2;                       // Write Protected
                fdc_result_buffer[2]=0;

                fdc_result_buffer[3]=fdc_command_buffer[2];
                fdc_result_buffer[4]=fdc_command_buffer[3];
                fdc_result_buffer[5]=fdc_command_buffer[4];
                fdc_result_buffer[6]=fdc_command_buffer[5];

                return;

            }

            return;


            case 0x6: // READ DATA 

                fdc_command_eot=fdc_command_buffer[6];
                fdc_read_sector_size=fdc_sector_count[fdc_command_buffer[5]];
                fdc_dma_offset=0;

                if(fdc_dma_flag) {

                    while(1) {

                        fdc_read_ptr=fdc_find_sector(fdc_command_buffer[1]&3,fdc_command_buffer[2],fdc_command_buffer[3],fdc_command_buffer[4],fdc_command_buffer[5]);

    // printf("[READ:%x,%d,%d,%d,%d,%d]",fdc_read_ptr,fdc_command_buffer[1],fdc_command_buffer[2],fdc_command_buffer[3],fdc_command_buffer[4],fdc_command_buffer[5]);

                         if(fdc_read_ptr==-1) {
                    // READ Error


                        } else {


                            fdc_read_dma(fdc_command_buffer[1]&3,fdc_read_ptr,fdc_dma_buffer+fdc_dma_offset);

                           if(fdc_command_buffer[4]==fdc_command_eot) {    // Finish by end of track

                                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                        // Result status

                                fdc_command_read_length=7;
                                fdc_command_read_index=0;

                                fdc_result_buffer[0]=0x00 | fdc_command_drive;   // Normal end
                                fdc_result_buffer[1]=0;
                                fdc_result_buffer[2]=0;

                                fdc_result_buffer[3]=fdc_command_buffer[2];
                                fdc_result_buffer[4]=fdc_command_buffer[3];
                                fdc_result_buffer[5]=fdc_command_buffer[4];
                                fdc_result_buffer[6]=fdc_command_buffer[5];

                                return;

                            }

                            fdc_command_buffer[4]++;
                            fdc_dma_offset+=fdc_read_sector_size;

                            if(fdc_dma_offset>=fdc_dma_datasize) {  // DMA TC

                                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                        // Result status

                                fdc_command_read_length=7;
                                fdc_command_read_index=0;

                                fdc_result_buffer[0]=0x00 | fdc_command_drive;   // Normal end
                                fdc_result_buffer[1]=0;
                                fdc_result_buffer[2]=0;

                                fdc_result_buffer[3]=fdc_command_buffer[2];
                                fdc_result_buffer[4]=fdc_command_buffer[3];
                                fdc_result_buffer[5]=fdc_command_buffer[4];
                                fdc_result_buffer[6]=fdc_command_buffer[5];

                                return;

                            }
                        }
                    }
                }

                return;

            case 0x7: // RECALIBRATE

                // Seek to track 0

                fdc_command_drive=fdc_command_buffer[1]&3;
                fdc_command_cylinder[fdc_command_drive]=0;

                fdc_phase_flag=0;
                return;

            case 0x8: // SENSE INTERRUPT STATUS

                fdc_phase_flag=2;
                fdc_command_read_length=2;
                fdc_command_read_index=0;

                // check disk status

                // printf("[SENSE:%d]",fdc_command_drive);

//                if(fdc_command_drive==0) {
                if(fd_drive_status[fdc_command_drive]!=0) {

                    fdc_result_buffer[0]=0x20 | fdc_command_drive;   // Seek end
                    fdc_result_buffer[1]=fdc_command_cylinder[fdc_command_drive];    


                } else {

//                    fdc_result_buffer[0]=0x28 | fdc_command_drive;   // Drive not ready
                    fdc_result_buffer[0]=0xa0 | fdc_command_drive;   // Ivalid
                    fdc_result_buffer[1]=0xff;    


                }

                return;

            case 0xd: // WRITE ID (Format)

                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                // Result status

                fdc_command_read_length=7;
                fdc_command_read_index=0;

                fdc_result_buffer[0]=0x40 | fdc_command_drive;  // Abort
                fdc_result_buffer[1]=0x2;                       // Write Protected
                fdc_result_buffer[2]=0;

                return;


            case 0xf: // SEEK

                fdc_command_drive=fdc_command_buffer[1]&3;
                fdc_command_cylinder[fdc_command_drive]=fdc_command_buffer[2];
                
                // printf("[SEEK%d:%d]",fdc_command_drive,fdc_command_cylinder[fdc_command_drive]);

                fdc_phase_flag=0;
                return;


            default:   // Error
                fdc_phase_flag=0;
                fdc_command_read_length=0;
                fdc_result_buffer[0]=0x80;

                return;
        }
    }

    fdc_phase_flag=1;

    return;

}

uint8_t fdc_command_read() {

    uint8_t result;

    if(fdc_phase_flag==2) {
        if(fdc_command_read_length>fdc_command_read_index) {
            result=fdc_result_buffer[fdc_command_read_index++];
        }
        if(fdc_command_read_length<=fdc_command_read_index) {
            fdc_phase_flag=0;
            fdc_command_read_index=0;
        }
        // printf("[!%x:%d/%d]",result,fdc_command_read_index,fdc_command_read_length);
        return result;
    }

    return 0x80;

}

// check inserted disk

void fdc_check(uint8_t driveno) {

    uint8_t flags;

    if(lfs_file_seek(&lfs_handler,&fd_drive[driveno],0x1a,LFS_SEEK_SET)) {
        fd_drive_status[driveno]=0;
    }

    lfs_file_read(&lfs_handler,&fd_drive[driveno],&flags,1);

    if(flags==0) {
        fd_drive_status[driveno]=1;
    } else {
        fd_drive_status[driveno]=3;        
    }

}



#if 0
// void fdc_write(uint8_t data) {

//     uint8_t driveno;

//     driveno=mainioport[0x1d]&3;


//     if(fd_status[driveno]!=1) return;
//     if(fdc_command==0) return;

//     if(fd_sector_bytes==fd_sector_size) {
//         mainioport[0x1f]=0x40;
//         fdc_status=0;
//     }

//     lfs_file_write(&lfs,&fd_drive[driveno],&data,1);

//     fd_sector_bytes++;
//     mainioport[0x1f]=0x80;

//     // find next sector

//     if(fd_sector_bytes==fd_sector_size) {

//         lfs_file_sync(&lfs,&fd_drive[driveno]);

//         if(fdc_command&0x10) {
//             mainioport[0x1a]++;
//             fdc_find_sector();

//             mainioport[0x1f]=0x80;

//         } else {

//         mainioport[0x1f]=0x40;
//         fdc_status=0;
//         fdc_command=0;

//         mfd_irq=1;

//         if(mainioport[2]&0x40) {
//             main_cpu.irq=true;
//         }

//         }
//     }



//     return;



// }
#endif


uint8_t fdc_mount(uint8_t driveno,lfs_t lfs,lfs_file_t file) {

}

uint8_t fdc_unmount(uint8_t driveno) {

}