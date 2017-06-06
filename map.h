#ifndef MAP_H
#define MAP_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	void map_new();
	void map_insert(int x, int y);
	bool map_contains(int x, int y);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAP_H */
