#ifndef FLASH_H
#define FLASH_H

#define BUFFER1		0x50000
#define BUFFER2		0x70000
#define FLASHSIZE	0x20000		// 128KB

#define PAGESIZE	1024
#define FLASHPAGES	128
#define FLASHSTART	0x0
#define BLOCKSIZE   16384

#include <stdint.h>

extern void enableFlashKeyRegister(void);
extern void lockFlashKeyRegister(void);
extern void fastmemcpy(uint24_t destination, uint24_t source, uint24_t size);
extern void reset(void);
extern void startVDPupdate(uint8_t filehandle, uint24_t filesize);

#endif //FLASH_H
