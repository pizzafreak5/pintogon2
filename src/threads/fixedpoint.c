
#include "fixedpoint.h"

fixed convertToFixed(int a) {
	fixed c;
	c.num = a * ONES_PLACE;
	return c;
}

int floorFixed(fixed a) {
	int b = a.num / ONES_PLACE;
	return b;
}

int roundFixed(fixed a) {
	int b = 0;
	if (a.num < 0)
		b = (a.num - ONES_PLACE/2) / ONES_PLACE;
	else
		b = (a.num + ONES_PLACE/2) / ONES_PLACE;
	return b;
}

fixed addFixed(fixed a, fixed b) {
	fixed c;
	c.num = a.num + b.num;
	return c;
}

fixed subFixed(fixed a, fixed b) {
	fixed c;
	c.num = a.num - b.num;
	return c;
}

fixed addFixedInt(fixed a, int b) {
	return addFixed(a, convertToFixed(b));
}

fixed subFixedInt(fixed a, int b) {
	return subFixed(a, convertToFixed(b));
}

fixed mulFixed(fixed a, fixed b) {
	fixed c;
	c.num = (((long long) a.num) * b.num / ONES_PLACE);
	return c;
}

fixed mulFixedInt(fixed a, int b) {
	fixed c;
	c.num = a.num * b;
	return c;
}

fixed divFixed(fixed a, fixed b) {
	fixed c;
	c.num = (((long long) a.num) * ONES_PLACE / b.num);
	return c;
}

fixed divFixedInt(fixed a, int b) {
	fixed c;
	c.num = a.num / b;
	return c;
}
//G's canver'
//Divides two integers as fixed structs so it can be used as fractions in BSD implementation
//where the fraction is a / b 
fixed fractionInt(int numerator, int denominator) {
	fixed c;
	fixed af;
	fixed bf;
	af.num = numerator * ONES_PLACE;
	bf.num = denominator * ONES_PLACE;
	c.num = (((long long) af.num) * ONES_PLACE / bf.num);
	return c;
}


