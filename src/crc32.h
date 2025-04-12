#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

void crc32(const char *s, uint24_t length);
void crc32_initialize(void);
uint32_t crc32_finalize(void);
#endif //CRC32_H
