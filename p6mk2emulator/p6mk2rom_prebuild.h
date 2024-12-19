// Dummy ROM file for Prebuild binary

#ifndef USE_SR

// ROM Data                 at
// BASICROM.62(66) 32KiB    0x10060000
// VOICEROM.62(66) 16KiB    0x10070000
// CGROM60.62(66)   8KiB    0x10074000
// CGROM60m.62      8KiB    0x10078000
// (CGROM66.66)
// KANJIROM.62(66) 32KiB　  0x10006800

#define ROMBASE 0x10060000u

uint8_t *basicrom=(uint8_t *)(ROMBASE);
uint8_t *voicerom=(uint8_t *)(ROMBASE+0x10000);
uint8_t *cgrom   =(uint8_t *)(ROMBASE+0x14000);
uint8_t *cgrom2  =(uint8_t *)(ROMBASE+0x18000);
uint8_t *kanjirom=(uint8_t *)(ROMBASE+0x08000);
uint8_t *extrom  =(uint8_t *)(ROMBASE-0x20000);

#else 

// ROM Data                 at
// SYSYEMROM1.64(68) 64KiB    0x10058000
// SYSTEMROM2.64(68) 64KiB    0x10068000
// CGROM66.64(68)    16KiB    0x10078000
// EXTROM           128KiB    0x10080000

#define ROMBASE 0x10058000u

uint8_t *system1rom=(uint8_t *)(ROMBASE);
uint8_t *system2rom=(uint8_t *)(ROMBASE+0x10000);
uint8_t *cgrom     =(uint8_t *)(ROMBASE+0x20000);
uint8_t *extrom    =(uint8_t *)(ROMBASE+0x28000);

#endif