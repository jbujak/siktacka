#include <stdint.h>

#include "rng.h"

#define RNG_FACTOR 279470273
#define RNG_MODULO 4294967291

static uint64_t state;

void random_init(uint64_t seed) {
	state = seed;
}

uint64_t random_get() {
	uint64_t result = state;
	state = state % RNG_MODULO;
	state = (state * RNG_FACTOR) % RNG_MODULO;
	return result;
}
