#define _DEFAULT_SOURCE
#include <errno.h>
#include <getopt.h>
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
static uint32_t game_id = 0;
static struct client_data *clients[MAX_PLAYERS];
static int number_of_clients = 0;
static int number_of_active_players = 0;
static int number_of_ready_players = 0;
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
static void handler(int sig, siginfo_t *si, void *uc);
static bool addresses_equal(struct sockaddr_in *addr_1, struct sockaddr_in *addr_2);
static void handle_client_message(struct sockaddr_in *address, socklen_t addr_len, struct client_msg *msg);
static int connect_client(struct sockaddr_in *address, socklen_t addr_len);
static void disconnect_client(struct sockaddr_in *address);
static void send_message();
static void sort_clients();
static int cmp_players(struct client_data *player_1, struct client_data *player_2);
static void process_turn();

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

	timer_prepare(100000000, handler);
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

		printf("%d message\n", message_queue_size());
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
					printf("received %s(%lu) from %d\n", msg.player_name, msg.session_id, client.sin_port);
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
	int client_index;
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if (clients[i] != NULL && addresses_equal(&clients[i]->address, address)) {
			already_connected = true;
			client_index = i;
			if (clients[i]->session_id > msg->session_id)
				return; /* ignore message if session_id is smaller than current */
		} else if (clients[i] != NULL && strcmp(clients[i]->player_name, msg->player_name) == 0)
				return; /* ignore message if existing name is received from another socket */
	}
	if (already_connected && clients[client_index]->session_id < msg->session_id) {
		printf("%s < %s\n", clients[client_index]->player_name, msg->player_name);
		printf("%d < %d\n", clients[client_index]->address.sin_port, address->sin_port);
		printf("%llu < %llu\n", clients[client_index]->session_id, msg->session_id);
		exit(1);
		disconnect_client(address);
		client_index = connect_client(address, addr_len);
	} else if (!already_connected && number_of_clients < MAX_PLAYERS) {
		client_index = connect_client(address, addr_len);
		strcpy(clients[client_index]->player_name, msg->player_name);
		if(*msg->player_name)
			number_of_active_players++;
		number_of_clients++;
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	clients[client_index]->session_id = msg->session_id;
	clients[client_index]->turn_direction = msg->turn_direction;
	clients[client_index]->last_message = tv.tv_sec * 1000000;
	clients[client_index]->last_message += tv.tv_usec;

	if (!clients[client_index]->ready && msg->turn_direction) {
		clients[client_index]->ready = true;
		number_of_ready_players++;

		if(state == WAITING_FOR_START && number_of_active_players >= 2 &&
				number_of_active_players == number_of_ready_players) {
			start_game();
		}
	}
}

static int connect_client(struct sockaddr_in *address, socklen_t addr_len) {
	number_of_clients++;
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] == NULL) {
			struct client_data *new_client;
			new_client = malloc(sizeof(struct client_data));
			memcpy(&(new_client->address), address, addr_len);
			new_client->observer = true;
			new_client->ready = false;
			clients[i] = new_client;
			return i;
		}
	}
	return -1;
}

static void disconnect_client(struct sockaddr_in *address) {
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] != NULL && addresses_equal(&clients[i]->address, address)) {
			clients[i]->connected = false;
			number_of_clients--;
		}
	}
}

static void start_game() {
	state = RUNNING;
	sort_clients();
	const char *names[MAX_PLAYERS];
	for (int i = 0; i < number_of_active_players; i++) {
		names[i] = clients[i]->player_name;
	}
	create_new_game_event(config.width, config.height, number_of_active_players, names);
	for (int i = 0; i < number_of_active_players; i++) {
		clients[i]->x = random_get() % config.width + 0.5;
		clients[i]->y = random_get() % config.height + 0.5;
		clients[i]->direction = random_get() % 360;
		clients[i]->alive = true;
		clients[i]->observer = false;
		printf("player %d: %s\n", i, clients[i]->player_name);
		if (map_contains(clients[i]->x, clients[i]->y)) {
			create_player_eliminated_event(i);
		} else {
			map_insert(clients[i]->x, clients[i]->y);
			create_pixel_event(i, clients[i]->x, clients[i]->y);
		}
	}
}

static void process_turn() {
	new_turn = 0;
	printf("Process turn\n");
	for (int i = 0; i < number_of_active_players; i++) {
		printf("%s check for alive\n", clients[i]->player_name);
		if (!clients[i]->alive) continue;
		clients[i]->direction += clients[i]->turn_direction * config.turning_speed;
		printf("%s calculate position\n", clients[i]->player_name);
		printf("%s direction: %lf\n", clients[i]->player_name, clients[i]->direction);
		printf("%s offset: (%lf, %lf)\n", clients[i]->player_name,
			sin((clients[i]->direction * M_PI) / 180), cos((clients[i]->direction * M_PI) / 180));
		double old_x = clients[i]->x;
		double old_y = clients[i]->y;
		clients[i]->x += sin((clients[i]->direction * M_PI) / 180);
		clients[i]->y += cos((clients[i]->direction * M_PI) / 180);
		printf("%s check position changed\n", clients[i]->player_name);
		if ((int)old_x == (int)clients[i]->x && (int)old_y == (int)clients[i]->y) continue;
		if (map_contains(clients[i]->x, clients[i]->y) || clients[i]->x < 0 ||
				clients[i]->x >= config.width || clients[i]->y >= config.height ||
				clients[i]->y < 0) {
			create_player_eliminated_event(i);
			clients[i]->alive = false;
		} else {
			map_insert(clients[i]->x, clients[i]->y);
			create_pixel_event(i, clients[i]->x, clients[i]->y);
		}
	}
}

static struct event* create_new_game_event(uint32_t maxx, uint32_t maxy, int players_num,
		const char *players[]) {
	struct event *result = malloc(sizeof(struct event));
	if(result == NULL) die("Insufficient memory");
	int ret;

	result->event_type = NEW_GAME;
	result->event_data.new_game.maxx = maxx;
	result->event_data.new_game.maxy = maxy;

	char *buf_end = result->event_data.new_game.players;
	int players_len = 0;
	for (int i = 0;  i < players_num; i++) {
		ret = sprintf(buf_end, "%s", players[i]);
		buf_end += ret + 1;
		players_len += ret + 1;
	}
	result->len = sizeof(result->event_no) +
		sizeof(result->event_type) +
		sizeof(result->event_data.new_game.maxy) +
		sizeof(result->event_data.new_game.maxy) +
		players_len;

	create_messages_for_event(result);
	event_insert(result);
	return result;
}

static struct event* create_pixel_event(char player_number, uint32_t x, uint32_t y) {
	struct event *result = malloc(sizeof(struct event));

	result->event_type = PIXEL;
	result->event_data.pixel.player_number = player_number;
	result->event_data.pixel.x = x;
	result->event_data.pixel.y = y;
	result->len =sizeof(result->event_no) +
		sizeof(result->event_type) +
		sizeof(result->event_data.pixel.player_number) +
		sizeof(result->event_data.pixel.x) +
		sizeof(result->event_data.pixel.y);

	event_insert(result);
	create_messages_for_event(result);
	printf("Pixel\n");
	return result;
}

static struct event* create_player_eliminated_event(char player_number) {
	struct event *result = malloc(sizeof(struct event));

	result->event_type = PLAYER_ELIMINATED;
	result->event_data.player_eliminated.player_number = player_number;
	result->len = sizeof(result->event_no) +
		sizeof(result->event_type) +
		sizeof(result->event_data.player_eliminated.player_number);

	event_insert(result);
	create_messages_for_event(result);
	return result;
}

static struct event* create_game_over_event() {
	struct event *result = malloc(sizeof(struct event));

	result->event_type = GAME_OVER;
	result->len = sizeof(result->event_no) +
		sizeof(result->event_type);

	event_insert(result);
	create_messages_for_event(result);
	return result;
}

static void create_messages_for_event(struct event* event) {
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(clients[i] != NULL) {
			struct server_msg_internal *message =
				malloc(sizeof(struct server_msg_internal));
			if(message == NULL) die("Insufficient memory");
			message->client_address = clients[i]->address;
			message->message.game_id = game_id;
			uint32_t event_len = sizeof(event->len) +
				event->len +
				sizeof(event->crc32);
			memcpy(message->message.events, event, event_len);
			message->len = sizeof(message->message.game_id) + event_len;
			message_insert(message);
		}
	}
}

static uint32_t calculate_checksum(int len, void* data) {
	/* TODO */
	data += len;
	return 1234567890;
}

static void handler(int UNUSED(sig), siginfo_t UNUSED(*si), void UNUSED(*uc)) {
	printf("Timer fired\n");
	if(state == RUNNING)
		new_turn = true;
}

static bool addresses_equal(struct sockaddr_in *addr_1, struct sockaddr_in *addr_2) {
	if (addr_1->sin_addr.s_addr != addr_2->sin_addr.s_addr) return false;
	if (addr_1->sin_port != addr_2->sin_port) return false;
	return true;
}

static void send_message() {
	struct server_msg_internal *msg = message_get(0);
	int ret;
	int flags = 0;
	ret = sendto(sock, &msg->message, msg->len, flags,
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
