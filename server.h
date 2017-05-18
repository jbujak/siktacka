#ifndef SERVER_H
#define SERVER_H

#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600
#define DEFAULT_PORT 12345
#define DEFAULT_ROUDS_PER_SEC 50
#define DEFAULT_TURNING_SPEED 6

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
