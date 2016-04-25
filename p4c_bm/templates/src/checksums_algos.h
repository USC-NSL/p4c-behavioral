/*
Copyright 2013-present Barefoot Networks, Inc. 

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/*
 * Modified by Yuliang Li liyuliang001@gmail.com
 */

#ifndef _RMT_CHECKSUMS_ALGOS_H
#define _RMT_CHECKSUMS_ALGOS_H

#include <stdint.h>
#include "crc_tables.h"

static inline void csum16(uint8_t *buf, int len, uint8_t *result) {
  uint64_t sum = 0;
  uint64_t *b = (uint64_t *) buf;

  uint32_t t1, t2;
  uint16_t t3, t4;
  
  /* Main loop - 8 bytes at a time */
  while (len >= sizeof(uint64_t))
    {
      uint64_t s = *b++;
      sum += s;
      if (sum < s) sum++;
      len -= 8;
    }

  /* Handle tail less than 8-bytes long */
  buf = (uint8_t *) b;
  if (len & 4)
    {
      uint32_t s = *(uint32_t *)buf;
      sum += s;
      if (sum < s) sum++;
      buf += 4;
    }

  if (len & 2)
    {
      uint16_t s = *(uint16_t *) buf;
      sum += s;
      if (sum < s) sum++;
      buf += 2;
    }

  if (len & 1)
    {
      uint8_t s = *(uint8_t *) buf;
      sum += s;
      if (sum < s) sum++;
    }
  
  /* Fold down to 16 bits */
  t1 = sum;
  t2 = sum >> 32;
  t1 += t2;
  if (t1 < t2) t1++;
  t3 = t1;
  t4 = t1 >> 16;
  t3 += t4;
  if (t3 < t4) t3++;
  
  *(uint32_t *) result =  ~t3;
}

static inline void csum16_slow(uint8_t *buf, int len, uint8_t *result) {
  uint32_t sum = 0;
  while(len > 1) {
    sum += *((uint16_t *) buf);
    buf += 2;
    if (sum & (1 << 31))
      sum = (sum & 0xFFFF) + (sum >> 16);
    len -= 2;
  }

  if(len)
    sum += (uint16_t) *(uint8_t *) buf;

  while(sum >> 16)
    sum = (sum >> 16) + (sum & 0xFFFF);

  *(uint32_t *) result = (uint16_t) (~sum); 
}

static inline void xor16(uint8_t *buf, int len, uint8_t *result) {
  uint16_t xor = 0;
  while(len > 1) {
    xor ^= *((uint16_t *) buf);
    buf += 2;
    len -= 2;
  }

  if(len)
    xor ^= (uint16_t) *(uint8_t *) buf;

  *(uint16_t *) result = xor;
}

/* This code was adapted from:
   http://www.barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code */

static inline uint32_t reflect(uint32_t data, int nBits) {
  uint32_t  reflection = 0x00000000;
  int bit;

  /*
   * Reflect the data about the center bit.
   */
  for (bit = 0; bit < nBits; ++bit) {
    /*
     * If the LSB bit is set, set the reflection of it.
     */
    if (data & 0x01) {
      reflection |= (1 << ((nBits - 1) - bit));
    }
    data = (data >> 1);
  }

  return reflection;
}

/* TODO: try to implement something a bit more generic that will cover
   programmable CRCs */

static inline void crc16(uint8_t *buf, int len, uint8_t *result) {
  int byte;
  uint16_t remainder = 0x0000;
  uint16_t final_xor_value = 0x0000;
  int data;
  for(byte = 0; byte < len; byte++) {
    data = reflect(buf[byte], 8) ^ (remainder >> 8);
    remainder = table_crc16[data] ^ (remainder << 8);
  }
  *(uint16_t *) result = (reflect(remainder, 16) ^ final_xor_value);
}

static inline void crcCCITT(uint8_t *buf, int len, uint8_t *result) {
  int byte;
  uint16_t remainder = 0xFFFF;
  uint16_t final_xor_value = 0x0000;
  int data;
  for(byte = 0; byte < len; byte++) {
    data = buf[byte] ^ (remainder >> 8);
    remainder = table_crcCCITT[data] ^ (remainder << 8);
  }
  *(uint16_t *) result = (remainder ^ final_xor_value);
}

static inline void crc32(uint8_t *buf, int len, uint8_t *result) {
  int byte;
  uint32_t remainder = 0xFFFFFFFF;
  uint32_t final_xor_value = 0xFFFFFFFF;
  int data;
  for(byte = 0; byte < len; byte++) {
    data = reflect(buf[byte], 8) ^ (remainder >> 24);
    remainder = table_crc16[data] ^ (remainder << 8);
  }
  *(uint32_t *) result = (reflect(remainder, 32) ^ final_xor_value);
}

static inline void identity(uint8_t *buf, int len, uint8_t *result) {
  int byte;
  int max_len = (len > 4) ? 4 : len;
  for(byte = 0; byte < max_len; byte++) {
    result[byte] = buf[byte];
  }
}

/*
 * The following murmur3_32 code is copied from wikipedia: https://en.wikipedia.org/wiki/MurmurHash
 */

#define ROT32(x, y) ((x << y) | (x >> (32 - y))) // avoid effort
static inline uint32_t murmur3_32(const uint8_t *key, uint32_t len, uint32_t seed) {
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	static const uint32_t r1 = 15;
	static const uint32_t r2 = 13;
	static const uint32_t m = 5;
	static const uint32_t n = 0xe6546b64;

	uint32_t hash = seed;

	const int nblocks = len / 4;
	const uint32_t *blocks = (const uint32_t *) key;
	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];
		k *= c1;
		k = ROT32(k, r1);
		k *= c2;

		hash ^= k;
		hash = ROT32(hash, r2) * m + n;
	}

	const uint8_t *tail = (const uint8_t *) (key + nblocks * 4);
	uint32_t k1 = 0;

	switch (len & 3) {
		case 3:
			k1 ^= tail[2] << 16;
		case 2:
			k1 ^= tail[1] << 8;
		case 1:
			k1 ^= tail[0];

			k1 *= c1;
			k1 = ROT32(k1, r1);
			k1 *= c2;
			hash ^= k1;
	}

	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);

	return hash;
}

//:: for i in range(100):
static inline void my_hash${i}(uint8_t *buf, int len, uint8_t *result){
	*(uint32_t*)result = murmur3_32(buf, len, ${i});
}
//:: #endfor

#endif
