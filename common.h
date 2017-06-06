#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/* Limits */
#define MAX_HOST_LENGTH 60
#define MAX_PORT_NUM 65535

#define MAX_NAME_LENGTH 64
#define MIN_NAME_CHAR 33
#define MAX_NAME_CHAR 126

#define MAX_DATAGRAM_LENGTH 512
#define MAX_EVENTS_LENGTH (MAX_DATAGRAM_LENGTH - 4)

#define MAX_PLAYERS 42

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
	char player_name[MAX_NAME_LENGTH + 1];
};

struct __attribute__((__packed__)) server_msg {
	uint32_t game_id;
	char events[MAX_EVENTS_LENGTH + 1];
};

struct __attribute__((__packed__)) new_game_event {
	uint32_t maxx;
	uint32_t maxy;
	char players[MAX_PLAYERS * (MAX_NAME_LENGTH + 1)];
};

struct __attribute__((__packed__)) pixel_event {
	char player_number;
	uint32_t x;
	uint32_t y;
};

struct __attribute__((__packed__)) player_eliminated_event {
	char player_number;
};

struct __attribute__((__packed__)) event {
	uint32_t len;
	uint32_t event_no;
	char event_type;
	union {
		struct new_game_event new_game;
		struct pixel_event pixel;
		struct player_eliminated_event player_eliminated;
	} event_data;
	uint32_t crc32; /* Just padding; will not be accessed directly */
};

#endif /* COMMON_H */
