#ifndef RNG_H
#define RNG_H
/* Deterministic pseudo-random number generator */

#include <stdint.h>

void random_init(uint64_t seed);

uint64_t random_get();

#endif /* RNG_H */
