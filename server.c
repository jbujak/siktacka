#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "server.h"
#include "parser.h"
#include "rng.h"
#include "map.h"

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

int main(int argc, char * const argv[])
{
	config.seed = time(NULL);
	parse_server_arguments(argc, argv, &config);
	init_random(config.seed);

	return 0;
}
