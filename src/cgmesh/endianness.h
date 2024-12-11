#ifndef ENDIANNESS_H
#define ENDIANNESS_H

#include <stdint.h>

#define L_ENDIAN 0
#define B_ENDIAN 1

int get_endianness();

uint64_t swpd(double d);
double uswpd(uint64_t d);

float swapf(float);
double swapd(double);
void swaps(uint16_t *x);
void swapi(uint32_t *x);
void swapl(uint64_t *x);

// LITTLE AND BIG ENDIAN CONVERSION
inline void swap_endian_2 (void *val)
{
	unsigned short *ival = (unsigned short*) val;
	*ival = ((*ival >> 8) & 0x000000ff) |
		((*ival << 8) & 0x0000ff00);
}

inline void swap_endian_4 (void *val)
{
	unsigned long *ival = (unsigned long*) val;
	*ival = ((*ival >> 24) & 0x000000ff) |
		((*ival >>  8) & 0x0000ff00) |
		((*ival <<  8) & 0x00ff0000) |
		((*ival << 24) & 0xff000000);
}

#endif
