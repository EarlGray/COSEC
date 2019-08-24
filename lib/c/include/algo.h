#ifndef __COSEC_ALGO_H__
#define __COSEC_ALGO_H__

#include <stdint.h>

uint32_t digital_update_crc32(uint32_t crc, const uint8_t *data, size_t len);
uint32_t digital_crc32(const uint8_t *buf, size_t len);
uint32_t ethernet_crc32(uint8_t *b, size_t n);

#endif  // __COSEC_ALGO_H__
