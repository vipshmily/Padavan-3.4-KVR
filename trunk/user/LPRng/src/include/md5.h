#ifndef MD5_H
#define MD5_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#else
#define uint32_t unsigned int
#warning make sure to find a uint32_t that is 32 bit and unsigned
#endif
#endif

typedef struct MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	unsigned char in[64];
} MD5_CONTEXT;

#define MD5_KEY_LENGTH 16

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, const unsigned char *buf, unsigned int len);
void MD5Final(struct MD5Context *context, unsigned char* digest);
#endif
