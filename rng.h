#ifndef RNG_H
#define RNG_H
/* Deterministic pseudo-random number generator */

#include <stdint.h>

void init_random(uint64_t seed);

uint64_t get_random();

#endif /* RNG_H */
