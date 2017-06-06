#ifndef SERVER_H
#define SERVER_H

#define MAX_PORT_NUM 65535

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "common.h"

struct server_config {
	int width;
	int height;
	int port;
	int rounds_per_sec;
	int turning_speed;
	uint64_t seed;
};

struct server_msg_internal {
	struct sockaddr_in client_address;
	struct server_msg message;
	bool ready;
	int len;
};

#endif /* SERVER_H */
