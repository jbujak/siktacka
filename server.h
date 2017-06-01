#ifndef SERVER_H
#define SERVER_H

#define MAX_PORT_NUM 65535

#include <stdint.h>

struct server_config {
	int width;
	int height;
	int port;
	int rounds_per_sec;
	int turning_speed;
	uint64_t seed;
};

#endif /* SERVER_H */
