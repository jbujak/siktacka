#define _DEFAULT_SOURCE
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "parser.h"
#include "err.h"
#include "server.h"

static void get_host_port(const char *str, char *host, int *port);
static int get_port(const char *str);
static int get_positive(const char *str, const char *what);
static int get_number(const char *str, const char *what);
static uint64_t get_number64(const char *str, const char *what);

void parse_server_arguments(int argc, char * const argv[], struct server_config *config)
{
	int opt;
	while ((opt = getopt(argc, argv, "W:H:p:s:t:r:")) != -1) {
		switch (opt) {
		case 'W':
			config->width = get_positive(optarg, "width");
			break;
		case 'H':
			config->height = get_positive(optarg, "height");
			break;
		case 'p':
			config->port= get_port(optarg);
			break;
		case 's':
			config->rounds_per_sec = get_positive(optarg, "speed");
			break;
		case 't':
			config->turning_speed = get_positive(optarg, "turning speed");
			break;
		case 'r':
			config->seed = get_number64(optarg, "seed");
			break;
		default:
			die("Unknown parameter");
		}
	}
}

void parse_client_arguments(int argc, char * const argv[], struct client_config *config) 
{
	if(argc < 3 || argc > 4)
		die("Invalid number of parameters\nUsage: %s player_name game_server_host[:port] [ui_server_host[:port]]", argv[0]);
	if(strlen(argv[2]) > MAX_HOST_LENGTH)
		die("Game server host name too long");

	get_host_port(argv[2], config->game_server, &(config->game_server_port));
	if(argc == 4)
		get_host_port(argv[3], config->ui_server, &(config->ui_server_port));
}

static void get_host_port(const char *str, char *host, int *port)
{
	printf("get_host_port from %s\n", str);
	char *buf = malloc(MAX_HOST_LENGTH);
	char *buf_ptr = buf;
	char *tmp;
	if(strnlen(str, MAX_HOST_LENGTH + 1) > MAX_HOST_LENGTH)
		die("Host name too long");
	strcpy(buf, str);
	tmp = strsep(&buf, ":");
	strcpy(host, tmp);
	if(buf != NULL)
		*port = get_port(buf);

	free(buf_ptr);
}

static int get_port(const char *str)
{
	int port = get_positive(str, "port");
	if(port > MAX_PORT_NUM)
		die("Port number too big");
	return port;
}

static int get_positive(const char *str, const char *what)
{
	int number = get_number(str, what);
	if (number <= 0)
		die("Incorrect %s value", what);
	return number;
}

static int get_number(const char *str, const char *what)
{
	char *endptr;
	errno = 0;
	int number = strtol(str, &endptr, 10);
	if (*endptr != '\0' || errno != 0)
		die("Incorrect %s value", what);
	return number;

}

static uint64_t get_number64(const char *str, const char *what)
{
	char *endptr;
	errno = 0;
	uint64_t number = strtoll(str, &endptr, 10);
	if (*endptr != '\0' || errno != 0)
		die("Incorrect %s value", what);
	return number;

}
