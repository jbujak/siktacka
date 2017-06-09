#define _DEFAULT_SOURCE
#include <errno.h>
#include <getopt.h>
#include <zlib.h>
#include <endian.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/socket.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"
#include "event_queue.h"
#include "timer.h"
#include "server_msg_queue.h"
#include "server.h"
#include "parser.h"
#include "rng.h"
#include "err.h"
#include "map.h"

#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600
#define DEFAULT_PORT 12345
#define DEFAULT_ROUDS_PER_SEC 50
#define DEFAULT_TURNING_SPEED 6

#define DISCONNECT_TIMOUT_US 2000000

static struct server_config config = (struct server_config) {
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.port = DEFAULT_PORT,
	.rounds_per_sec = DEFAULT_ROUDS_PER_SEC,
	.turning_speed = DEFAULT_TURNING_SPEED,
};

struct client_data {
	struct sockaddr_in address;
	uint64_t session_id;
	char player_name[MAX_NAME_LENGTH];
	bool ready;
	bool observer;
	bool alive;
	bool connected;
	double x;
	double y;
	double direction;
	long long int last_message;
	int turn_direction;
};

static enum game_state {
	WAITING_FOR_START,
	RUNNING
} state = WAITING_FOR_START;
static int sock;
static uint32_t game_id = -1;
static uint32_t event_no = 0;
static struct client_data *clients[MAX_PLAYERS];
static int number_of_clients = 0;
static int number_of_active_players = 0;
static int number_of_ready_players = 0;
static int number_of_alive_players = 0;
static bool new_turn = false;

static void init();
static void main_loop();
static void start_game();
static struct event* create_new_game_event(uint32_t maxx, uint32_t maxy, int players_num,
		const char *players[]);
static struct event* create_pixel_event(char player_number, uint32_t x, uint32_t y);
static struct event* create_player_eliminated_event(char player_number);
static struct event* create_game_over_event();
static void create_messages_for_event(struct event* event);
static void create_messages_for_events(struct sockaddr_in address, int beg, int end);
static void handler(int sig, siginfo_t *si, void *uc);
static bool addresses_equal(struct sockaddr_in *addr_1, struct sockaddr_in *addr_2);
static void handle_client_message(struct sockaddr_in *address, socklen_t addr_len, struct client_msg *msg);
static int connect_client(struct sockaddr_in *address, socklen_t addr_len);
static void disconnect_client(struct sockaddr_in *address);
static void send_message();
static void sort_clients();
static int cmp_players(struct client_data *player_1, struct client_data *player_2);
static void process_turn();
static void end_game();
static void disonnect_inactive();
static void eliminate_player(int player_number);

int main(int argc, char * const argv[]) {
	config.seed = time(NULL);
	parse_server_arguments(argc, argv, &config);
	random_init(config.seed);
	init();
	main_loop();

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

	timer_prepare(1000000000.0 / config.rounds_per_sec, handler);
}	

static void main_loop() {
	struct client_msg msg;
	struct sockaddr_in client;
	socklen_t client_len = sizeof(client);
	int msg_len;
	int ret;
	int flags = 0;
	struct pollfd fds[1];
	fds[0].fd = sock;
	fds[0].events = POLLIN;

	memset(&client, 0, sizeof(client));
	
	while (true) {
		if(new_turn) 
			process_turn();
		if (message_queue_size() > 0)
			fds[0].events |= POLLOUT;
		else
			fds[0].events &= ~POLLOUT;
		fds[0].revents = 0;
		ret = poll(fds, 1, -1);
		if(ret > 0) {
			if (fds[0].revents & POLLIN) {
				msg_len = recvfrom(sock, &msg, sizeof(msg), flags,
						(struct sockaddr*)&client, &client_len);
				if(msg_len == sizeof(msg)) {
					handle_client_message(&client, client_len, &msg);
				}
			}
			if (fds[0].revents & POLLOUT) {
				send_message();
			}
		}
	}
}

static void handle_client_message(struct sockaddr_in *address, socklen_t addr_len, struct client_msg *msg) {
	bool already_connected = false;
	msg->next_expected_event_no = be32toh(msg->next_expected_event_no);
	msg->session_id = be64toh(msg->session_id);
	int client_index = -1;
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if (clients[i] != NULL && addresses_equal(&clients[i]->address, address)) {
			already_connected = true;
			client_index = i;
			if (clients[i]->session_id > msg->session_id) {
				printf("Smaller session_id\n");
				return; /* ignore message if session_id is smaller than current */
			}
		} else if (clients[i] != NULL && strcmp(clients[i]->player_name, msg->player_name) == 0) {
			return; /* ignore message if existing name is received from another socket */
		}
	}
	if (already_connected && clients[client_index]->session_id < msg->session_id) {
		printf("Reconnect\n");
		disconnect_client(address);
		client_index = connect_client(address, addr_len);
	} else if (!already_connected && number_of_clients < MAX_PLAYERS) {
		printf("Connect\n");
		client_index = connect_client(address, addr_len);
		strcpy(clients[client_index]->player_name, msg->player_name);
		if(*msg->player_name) {
			number_of_active_players++;
		}
		clients[client_index]->observer = (*msg->player_name == 0 || state != WAITING_FOR_START);
		number_of_clients++;
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	clients[client_index]->session_id = msg->session_id;
	clients[client_index]->turn_direction = msg->turn_direction;
	clients[client_index]->last_message = tv.tv_sec * 1000000;
	clients[client_index]->last_message += tv.tv_usec;

	if (!clients[client_index]->ready && !clients[client_index]->observer &&
			msg->turn_direction && state == WAITING_FOR_START) {
		clients[client_index]->ready = true;
		number_of_ready_players++;

		if (number_of_active_players >= 2 && number_of_active_players == number_of_ready_players) {
			start_game();
		}
	}

	create_messages_for_events(*address, msg->next_expected_event_no, event_queue_size());
}

static int connect_client(struct sockaddr_in *address, socklen_t addr_len) {
	number_of_clients++;
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] == NULL) {
			struct client_data *new_client;
			new_client = malloc(sizeof(struct client_data));
			memcpy(&(new_client->address), address, addr_len);
			new_client->ready = false;
			new_client->alive = false;
			new_client->connected = true;
			clients[i] = new_client;
			return i;
		}
	}
	return -1;
}

static void disconnect_client(struct sockaddr_in *address) {
	printf("Disconnect\n");
	int index = -1;
	struct client_data *client = NULL;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (clients[i] != NULL && addresses_equal(&clients[i]->address, address)) {
			client = clients[i];
			index = i;
			break;
		}
	}
	if(client == NULL)
		die("Client to disconnect not found");

	client->connected = false;
	number_of_clients--;
	if (client->ready)
		number_of_ready_players--;
	if (*client->player_name)
		number_of_active_players--;
	if (!client->alive) {
		free(client);
		clients[index] = NULL;
	}
}

static void start_game() {
	printf("Start game\n");
	state = RUNNING;
	event_no = 0;
	game_id++;
	map_new();
	sort_clients();
	const char *names[MAX_PLAYERS];
	for (int i = 0; i < number_of_active_players; i++) {
		names[i] = clients[i]->player_name;
	}
	number_of_alive_players = number_of_active_players;
	create_new_game_event(config.width, config.height, number_of_active_players, names);
	for (int i = 0; i < number_of_active_players; i++) {
		clients[i]->x = random_get() % config.width + 0.5;
		clients[i]->y = random_get() % config.height + 0.5;
		clients[i]->direction = random_get() % 360;
		clients[i]->alive = true;
		if (map_contains(clients[i]->x, clients[i]->y)) {
			printf("Died during start\n");
			eliminate_player(i);
		} else {
			map_insert(clients[i]->x, clients[i]->y);
			create_pixel_event(i, clients[i]->x, clients[i]->y);
		}
	}
}

static void end_game() {
	printf("End game\n");
	create_game_over_event();
	event_queue_new();
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] != NULL) {
			clients[i]->observer = (*clients[i]->player_name == 0);
			clients[i]->ready = false;
		}
	}
	state = WAITING_FOR_START;
	number_of_ready_players = 0;
}

static void process_turn() {
	new_turn = 0;
	disonnect_inactive();
	if (state != RUNNING) return;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (clients[i] == NULL || !clients[i]->alive) continue;
		clients[i]->direction += clients[i]->turn_direction * config.turning_speed;
		double old_x = clients[i]->x;
		double old_y = clients[i]->y;
		clients[i]->x += sin((clients[i]->direction * M_PI) / 180);
		clients[i]->y += cos((clients[i]->direction * M_PI) / 180);
		if ((int)old_x == (int)clients[i]->x && (int)old_y == (int)clients[i]->y) continue;
		if (map_contains(clients[i]->x, clients[i]->y) || clients[i]->x < 0 ||
				clients[i]->x >= config.width || clients[i]->y >= config.height ||
				clients[i]->y < 0) {
			printf("Died during game\n");
			eliminate_player(i);
		} else {
			map_insert(clients[i]->x, clients[i]->y);
			create_pixel_event(i, clients[i]->x, clients[i]->y);
		}
	}
}

static void disonnect_inactive() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	for(int i  = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] == NULL || !clients[i]->connected) continue;
		if(tv.tv_sec * 1000000 + tv.tv_usec > clients[i]->last_message + DISCONNECT_TIMOUT_US)
			disconnect_client(&clients[i]->address);
	}
}

static struct event* create_new_game_event(uint32_t maxx, uint32_t maxy, int players_num,
		const char *players[]) {
	printf("Creating new game event\n");
	struct event *result = malloc(sizeof(struct event));
	if(result == NULL) die("Insufficient memory");
	int ret;

	result->event_no = htobe32(event_no++);
	result->event_type = NEW_GAME;
	result->event_data.new_game.maxx = htobe32(maxx);
	result->event_data.new_game.maxy = htobe32(maxy);

	char *buf_end = result->event_data.new_game.players;
	int players_len = 0;
	for (int i = 0;  i < players_num; i++) {
		ret = sprintf(buf_end, "%s", players[i]);
		buf_end += ret + 1;
		players_len += ret + 1;
	}
	uint32_t len = sizeof(result->event_no) +
		sizeof(result->event_type) +
		sizeof(result->event_data.new_game.maxy) +
		sizeof(result->event_data.new_game.maxy) +
		players_len;
	result->len = htobe32(len);
	int len_to_checksum = len + sizeof(result->len);
	*(int*)((void*)result + len_to_checksum) = htobe32(crc32(0, (void*)result, len_to_checksum));
	printf("checksum %lu\n", crc32(0, (void*)result, len_to_checksum));

	create_messages_for_event(result);
	event_insert(result);
	return result;
}

static struct event* create_pixel_event(char player_number, uint32_t x, uint32_t y) {
	struct event *result = malloc(sizeof(struct event));

	result->event_no = htobe32(event_no++);
	result->event_type = PIXEL;
	result->event_data.pixel.player_number = player_number;
	result->event_data.pixel.x = htobe32(x);
	result->event_data.pixel.y = htobe32(y);
	uint32_t len = sizeof(result->event_no) +
		sizeof(result->event_type) +
		sizeof(result->event_data.pixel.player_number) +
		sizeof(result->event_data.pixel.x) +
		sizeof(result->event_data.pixel.y);
	result->len = htobe32(len);
	int len_to_checksum = len + sizeof(result->len);
	*(int*)((void*)result + len_to_checksum) = htobe32(crc32(0, (void*)result, len_to_checksum));

	event_insert(result);
	create_messages_for_event(result);
	return result;
}

static struct event* create_player_eliminated_event(char player_number) {
	struct event *result = malloc(sizeof(struct event));

	result->event_no = htobe32(event_no++);
	result->event_type = PLAYER_ELIMINATED;
	result->event_data.player_eliminated.player_number = player_number;
	uint32_t len = sizeof(result->event_no) +
		sizeof(result->event_type) +
		sizeof(result->event_data.player_eliminated.player_number);
	result->len = htobe32(len);
	int len_to_checksum = len + sizeof(result->len);
	*(int*)((void*)result + len_to_checksum) = htobe32(crc32(0, (void*)result, len_to_checksum));

	event_insert(result);
	create_messages_for_event(result);
	return result;
}

static struct event* create_game_over_event() {
	struct event *result = malloc(sizeof(struct event));

	result->event_no = htobe32(event_no++);
	result->event_type = GAME_OVER;
	uint32_t len = sizeof(result->event_no) +
		sizeof(result->event_type);
	result->len = htobe32(len);
	int len_to_checksum = len + sizeof(result->len);
	*(int*)((void*)result + len_to_checksum) = htobe32(crc32(0, (void*)result, len_to_checksum));

	event_insert(result);
	create_messages_for_event(result);
	return result;
}

static void create_messages_for_event(struct event* event) {
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] != NULL) {
			struct server_msg_internal *message =
				calloc(1, sizeof(struct server_msg_internal));
			if(message == NULL) die("Insufficient memory");
			message->client_address = clients[i]->address;
			message->message.game_id = htobe32(game_id);
			uint32_t event_len = sizeof(event->len) +
				be32toh(event->len) +
				sizeof(event->crc32);
			memcpy(message->message.events, event, event_len);
			message->len = sizeof(message->message.game_id) + event_len;
			message_insert(message);
		}
	}
}

static void create_messages_for_events(struct sockaddr_in address, int beg, int end) {
	struct server_msg_internal *current_msg = calloc(1, sizeof(struct server_msg_internal));
	current_msg->client_address  = address;
	struct event *current_event;
	int current_msg_len = 0;
	int current_event_len;
	for (int i = beg; i < end; i++) {
		current_event = event_get(i);
		current_event_len = be32toh(current_event->len) +
			sizeof(current_event->len) + sizeof(current_event->crc32);

		if(current_msg_len + current_event_len > MAX_EVENTS_LENGTH) {
			current_msg->client_address = address;
			current_msg->message.game_id = htobe32(game_id);
			current_msg->len = sizeof(current_msg->message.game_id) + current_msg_len;
			message_insert(current_msg);

			current_msg = calloc(1, sizeof(struct server_msg_internal));
			current_msg_len = 0;
		}
		memcpy(current_msg->message.events + current_msg_len, current_event, current_event_len);
		current_msg_len += current_event_len;
	}
	if (current_msg_len > 0) {
		current_msg->client_address = address;
		current_msg->message.game_id = htobe32(game_id);
		current_msg->len = sizeof(current_msg->message.game_id) + current_msg_len;
		message_insert(current_msg);
	} else {
		free(current_msg);
	}
}

static void handler(int UNUSED(sig), siginfo_t UNUSED(*si), void UNUSED(*uc)) {
	new_turn = true;
}

static bool addresses_equal(struct sockaddr_in *addr_1, struct sockaddr_in *addr_2) {
	if (addr_1->sin_addr.s_addr != addr_2->sin_addr.s_addr) return false;
	if (addr_1->sin_port != addr_2->sin_port) return false;
	return true;
}

static void send_message() {
	struct server_msg_internal *msg = message_get(0);
	int flags = 0;
	sendto(sock, &msg->message, msg->len, flags,
			(struct sockaddr*)&msg->client_address, sizeof(msg->client_address));
	message_remove();
}

static void sort_clients() {
	for(int i = 0;  i < MAX_PLAYERS; i++) {
		for(int j = 0; j < MAX_PLAYERS - 1; j++) {
			if(cmp_players(clients[j], clients[j+1]) > 0) {
				struct client_data *tmp = clients[j];
				clients[j] = clients[j+1];
				clients[j+1] = tmp;
			}
		}
	}
}

static int cmp_players(struct client_data *player_1, struct client_data *player_2) {
	if(player_1 == NULL && player_2 == NULL) return 0;
	if(player_1 == NULL && player_2 != NULL) return 1;
	if(player_1 != NULL && player_2 == NULL) return -1;
	if(player_1->observer && !player_2->observer) return 1;
	if(!player_1->observer && player_2->observer) return -1;
	return strcmp(player_1->player_name, player_2->player_name);
}

static void eliminate_player(int player_number) {
	create_player_eliminated_event(player_number);
	clients[player_number]->alive = 0;
	number_of_alive_players--;
	if(number_of_alive_players == 0)
		end_game();
}
