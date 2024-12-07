#include "lfs.h"

void fdc_init(void);
uint8_t fdc_status(void);
void fdc_command_write(uint8_t data);
uint8_t fdc_command_read();
uint8_t fdc_mount(uint8_t driveno,lfs_t lfs,lfs_file_t file);
uint8_t fdc_unmount(uint8_t driveno);
