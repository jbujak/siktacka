#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/* Limits */
#define MAX_HOST_LENGTH 60
#define MAX_PORT_NUM 65535

#define MIN_NAME_LENGTH 0
#define MAX_NAME_LENGTH 64
#define MIN_NAME_CHAR 33
#define MAX_NAME_CHAR 126

/* Directions */
#define LEFT (-1)
#define RIGHT 1
#define STRAIGHT 0

/* Event types */
#define NEW_GAME 0
#define PIXEL 1
#define PLAYER_ELIMINATED 2
#define GAME_OVER 3

#define UNUSED(x) x __attribute__((unused))

struct __attribute__((__packed__)) client_msg {
	uint64_t session_id;
	char turn_direction;
	uint32_t next_expected_event_no;
	char player_name[MAX_NAME_LENGTH];
};

struct __attribute__((__packed__)) server_msg {
	uint32_t game_id;
};


#endif /* COMMON_H */
