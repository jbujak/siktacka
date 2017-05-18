#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "server.h"
#include "parser.h"

static struct server_config config = (struct server_config) {
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.port = DEFAULT_PORT,
	.rounds_per_sec = DEFAULT_ROUDS_PER_SEC,
	.turning_speed = DEFAULT_TURNING_SPEED,
	.seed = 0
};

int main(int argc, char * const argv[])
{
	parse_server_arguments(argc, argv, &config);

	return 0;
}
