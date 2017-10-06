#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <zlib.h>
#include <ctype.h>
#include <endian.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "err.h"
#include "gui_client.h"
#include "parser.h"
#include "timer.h"

#define DEFAULT_GAME_SERVER_PORT 12345
#define DEFAULT_UI_SERVER_NAME "localhost"
#define DEFAULT_UI_SERVER_PORT 12346

#define TIMEOUT_NS 30000000
#define USECS_PER_SEC 1000000

#define LEFT_KEY_DOWN "LEFT_KEY_DOWN\n"
#define LEFT_KEY_UP "LEFT_KEY_UP\n"
#define RIGHT_KEY_DOWN "RIGHT_KEY_DOWN\n"
#define RIGHT_KEY_UP "RIGHT_KEY_UP\n"

static struct client_config config = (struct client_config) {
	.player_name = "",
	.game_server = "",
	.game_server_port = DEFAULT_GAME_SERVER_PORT,
	.ui_server = DEFAULT_UI_SERVER_NAME,
	.ui_server_port = DEFAULT_UI_SERVER_PORT
};

static struct client_msg state = (struct client_msg) {
	.session_id = 0,
	.turn_direction = 0,
	.next_expected_event_no = 0,
	.player_name = ""
};

static int gui_socket;
static int server_socket;
struct sockaddr_in server;
static char buf[1000000];
static uint32_t current_game_id = 0;
static uint32_t finished_game_id = 0; static uint32_t maxx;
static uint32_t maxy;
static char *players[MAX_PLAYERS];
static char *server_str;

static void init();
static void prepare_server();
static void handler(int sig, siginfo_t *si, void *uc);
static void receive_messages();
static void handle_gui_message(char *buf);
static void handle_server_message(void *buf, int len);
static int process_event(struct event *event, void *end);
static void process_new_game_event(struct new_game_event *event, int event_data_len);
static void process_pixel_event(struct pixel_event *event);
static void process_player_eliminated_event(struct player_eliminated_event *event);
static void process_game_over_event();
static void cleanup();

int main(int argc, char * const argv[]) {
	parse_client_arguments(argc, argv, &config);
	server_str = argv[2];
	init();
	receive_messages();

	return 0;
}

static void init() {
	struct timeval tv;

	gettimeofday(&tv, NULL);
	state.session_id = tv.tv_usec;
	state.session_id += tv.tv_sec * USECS_PER_SEC;
	strcpy(state.player_name, config.player_name);

	for(int i = 0; i < MAX_PLAYERS; i++) {
		players[i] = malloc(MAX_NAME_LENGTH);
	}

	gui_socket = gui_init(config.ui_server_port, config.ui_server);
	prepare_server();
	timer_prepare(TIMEOUT_NS, handler);
}

static void prepare_server() {
	struct addrinfo addr_hints;
	struct addrinfo *addr_result;
	int ret;

	/* 'converting' host/port in string to struct addrinfo */
	(void) memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_DGRAM;
	addr_hints.ai_protocol = IPPROTO_UDP;
	addr_hints.ai_flags = 0;
	addr_hints.ai_addrlen = 0;
	addr_hints.ai_addr = NULL;
	addr_hints.ai_canonname = NULL;
	addr_hints.ai_next = NULL;
	char port_str[10];
	sprintf(port_str, "%d", config.game_server_port);
	ret = getaddrinfo(config.game_server, port_str, &addr_hints, &addr_result);
	if(ret != 0) {
		fprintf(stderr, "Error in getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr =
		((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; /* address IP */
	server.sin_port = htons((uint16_t) config.game_server_port);

	freeaddrinfo(addr_result);
	server_socket = socket(PF_INET, SOCK_DGRAM, 0);
	handle_error(server_socket, "client socket");
}

static void handler(int UNUSED(sig), siginfo_t UNUSED(*si), void UNUSED(*uc)) {
	static struct client_msg msg;
	int flags = 0;
	int ret;
	msg.session_id = htobe64(state.session_id);
	msg.turn_direction = state.turn_direction;
	msg.next_expected_event_no = htobe32(state.next_expected_event_no);
	strcpy(msg.player_name, state.player_name);
	ret = sendto(server_socket, &msg, sizeof(msg.session_id) + sizeof(msg.turn_direction) +
			sizeof(msg.next_expected_event_no) + strlen(msg.player_name),
			flags, (struct sockaddr*)&server, sizeof(server));
	handle_error(ret, "client handler\n");
}

static void receive_messages() {
	struct pollfd fds[2];
	int ret;

	fds[0].fd = server_socket;
	fds[0].events = POLLIN | POLLHUP;
	fds[1].fd = gui_socket;
	fds[1].events = POLLIN;

	while (true) {
		fds[0].revents = 0;
		fds[1].revents = 0;
		ret = poll(fds, 2, -1);
		if (ret > 0) {
			if (fds[0].revents & POLLIN) {
				ret = read(fds[0].fd, buf, sizeof(buf));
				handle_server_message(buf, ret);
				buf[ret] = 0;
			}
			if (fds[1].revents & POLLIN) {
				ret = read(fds[1].fd, buf, 100000);
				buf[ret] = 0;
				if(ret == 0) {
					die("GUI disconnected\n");
				}
				handle_gui_message(buf);
			}
		}
	}
}

static void handle_gui_message(char *buf) {
	if (!strcmp(buf, LEFT_KEY_UP) || !strcmp(buf, RIGHT_KEY_UP))
		state.turn_direction = 0;
	else if (!strcmp(buf, LEFT_KEY_DOWN))
		state.turn_direction = -1;
	else if (!strcmp(buf, RIGHT_KEY_DOWN))
		state.turn_direction = 1;
}

static void handle_server_message(void *buf, int len) {
	struct server_msg *msg = (struct server_msg*) buf;
	void *event_ptr = msg->events;
	int ret;
	msg->game_id = be32toh(msg->game_id);
	if (current_game_id != 0 && msg->game_id != current_game_id && ((struct event*)event_ptr)->event_type != NEW_GAME) {
		printf("Ignoring -- incorrect game_id %d (expected %d)\n", msg->game_id, current_game_id);
		return; /* Ignore incorrect game_id */
	} else if(current_game_id != msg->game_id) {
		state.next_expected_event_no = 0;
		current_game_id = msg->game_id;
	}
	while (event_ptr < buf + len) {
		ret = process_event(event_ptr, buf + len);
		if(ret == -1) return;
		event_ptr += ret;
	}
}

static int process_event(struct event *event, void *end) {
	int len_to_checksum = be32toh(event->len) + sizeof(event->len);
	uint32_t expected_checksum = crc32(0, (void*)event, len_to_checksum);
	event->len = be32toh(event->len);
	event->event_no = be32toh(event->event_no);
	event->crc32 = be32toh(*(int*)((void*)event + sizeof(event->len) + event->len));
	int total_event_len = event->len + sizeof(event->len) + sizeof(event->crc32);
	printf("len %d\n", event->len);
	if(total_event_len + (void*)event > end)
		die("Incorrect event");
	int event_data_len = event->len - sizeof(event->event_no) - sizeof(event->event_type);
	uint32_t checksum = *(int*)((void*)event + event->len + sizeof(event->len));
	checksum = be32toh(checksum);
	
	if(event->event_type == NEW_GAME) {
		printf("checksum %lu\n", (unsigned long)checksum);
	}
	if(checksum != expected_checksum) {
		printf("Invalid checksum");
		return -1;
	}
	if (event->event_no != state.next_expected_event_no)
		return total_event_len;
	state.next_expected_event_no++;
	if ((void*)event + total_event_len > end)
		return total_event_len;
	switch (event->event_type) {
		case NEW_GAME:
			process_new_game_event(&event->event_data.new_game, event_data_len);
			break;
		case PIXEL:
			process_pixel_event(&event->event_data.pixel);
			break;
		case PLAYER_ELIMINATED:
			process_player_eliminated_event(&event->event_data.player_eliminated);
			break;
		case GAME_OVER:
			process_game_over_event();
			break;
	}
	return total_event_len;
}

static void process_new_game_event(struct new_game_event *event, int event_data_len) {
	maxx = be32toh(event->maxx);
	maxy = be32toh(event->maxy);
	if(maxx == 0 || maxy == 0) die("Incorrect dimensions");
	int i = 0;
	char *player = event->players;
	while((void*)player < (void*)event + event_data_len) {
		player += sprintf(players[i], "%s", player);
		player++;
		i++;
	}
	if((void*)player > (void*)event + event_data_len) die("Incorrect players");
	gui_new_game(maxx, maxy, i, players);
}

static void process_pixel_event(struct pixel_event *event) {
	if(event->player_number > MAX_PLAYERS || *players[(int)event->player_number] == 0)
		die("Incorrect player number");
	gui_pixel(be32toh(event->x), be32toh(event->y), players[(int)event->player_number]);
}

static void process_player_eliminated_event(struct player_eliminated_event *event) {
	if(event->player_number > MAX_PLAYERS || *players[(int)event->player_number] == 0)
		die("Incorrect player number");
	gui_eliminated(players[(int)event->player_number]);
}

static void process_game_over_event() {
	finished_game_id = current_game_id;
}

static void cleanup() {
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(players[i] != NULL) free(players[i]);
	}
}
