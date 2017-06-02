#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "parser.h"
#include "rng.h"
#include "map.h"
#include "err.h"

#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600
#define DEFAULT_PORT 12345
#define DEFAULT_ROUDS_PER_SEC 50
#define DEFAULT_TURNING_SPEED 6

static struct server_config config = (struct server_config) {
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.port = DEFAULT_PORT,
	.rounds_per_sec = DEFAULT_ROUDS_PER_SEC,
	.turning_speed = DEFAULT_TURNING_SPEED,
};

static void init();

static int sock;

int main(int argc, char * const argv[])
{
	config.seed = time(NULL);
	parse_server_arguments(argc, argv, &config);
	random_init(config.seed);
	init();

	return 0;
}

static void init() {
	struct sockaddr_in server_address;
	int ret;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	handle_error(sock, "server socket");

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
	server_address.sin_port = htons(config.port); 

	
	ret = bind(sock, (struct sockaddr *) &server_address,
			(socklen_t) sizeof(server_address));
	handle_error(ret, "server bind");

	char buf[100];
	read(sock, buf, 100);
	printf("received %s\n", buf);
}	
