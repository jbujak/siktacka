#ifndef STRDEQUE_H
#define STRDEQUE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" namespace jnp1 {
#endif //__cplusplus

	void map_new();
	void map_insert(int x, int y);
	bool map_contains(int x, int y);


#ifdef __cplusplus
}
#endif //__cplusplus

#endif
