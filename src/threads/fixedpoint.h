#include <stddef.h>

#ifndef FIXED_POINT_H
#define FIXED_POINT_H

typedef struct _fixed {
	unsigned long num;
} fixed;

#define FRAC_SIZE		17
#define ONES_PLACE		0x00020000

fixed convertToFixed(int a);

int floorFixed(fixed a);

int roundFixed(fixed a);

fixed addFixed(fixed a, fixed b);

fixed subFixed(fixed a, fixed b);

fixed addFixedInt(fixed a, int b);

fixed subFixedInt(fixed a, int b);

fixed mulFixed(fixed a, fixed b);

fixed mulFixedInt(fixed a, int b);

fixed divFixed(fixed a, fixed b);

fixed divFixedInt(fixed a, int b);
//G's canver'
//Divides two integers as fixed structs so it can be used as fractions in BSD implementation
//where the fraction is a / b 
fixed fractionInt(int numerator, int denominator) ;
#endif

