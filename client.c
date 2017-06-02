#define _DEFAULT_SOURCE
#include <arpa/inet.h>
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

#define DEFAULT_GAME_SERVER_PORT 12345
#define DEFAULT_UI_SERVER_NAME "localhost"
#define DEFAULT_UI_SERVER_PORT 12346

#define TIMEOUT_NS 3000000000
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

static void init();
static void prepare_server();
static void handler(int sig, siginfo_t *si, void *uc);
static void prepare_timer();
static void receive_messages();
static void handle_gui_message(char *buf);

int main(int argc, char * const argv[]) {
	parse_client_arguments(argc, argv, &config);
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

	gui_socket = gui_init(config.ui_server_port, config.ui_server);
	prepare_server();
	//prepare_timer();
}

static void prepare_server() {
	struct addrinfo addr_hints;
	struct addrinfo *addr_result;
	int ret;

	/* 'converting' host/port in string to struct addrinfo */
	(void) memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_INET; /* IPv4 */
	addr_hints.ai_socktype = SOCK_DGRAM;
	addr_hints.ai_protocol = IPPROTO_UDP;
	addr_hints.ai_flags = 0;
	addr_hints.ai_addrlen = 0;
	addr_hints.ai_addr = NULL;
	addr_hints.ai_canonname = NULL;
	addr_hints.ai_next = NULL;
	ret = getaddrinfo(config.game_server, NULL, &addr_hints, &addr_result);
	if(ret != 0) {
		fprintf(stderr, "Unknown host\n");
		exit(EXIT_FAILURE);
	}

	server.sin_family = AF_INET; /* IPv4 */
	server.sin_addr.s_addr =
		((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; /* address IP */
	server.sin_port = htons((uint16_t) config.game_server_port);

	freeaddrinfo(addr_result);
	server_socket = socket(PF_INET, SOCK_DGRAM, 0);
	handle_error(server_socket, "client socket");
}

static void prepare_timer() {
	timer_t timerid;
	struct sigevent sev;
	struct itimerspec its;
	sigset_t mask;
	struct sigaction sa;
	int ret;

	/* Establish handler for timer signal */

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset(&sa.sa_mask);
	ret = sigaction(SIGRTMIN, &sa, NULL);
	handle_error(ret, "sigaction");

	/* Block timer signal temporarily */

	printf("Blocking signal %d\n", SIGRTMIN);
	sigemptyset(&mask);
	sigaddset(&mask, SIGRTMIN);
	if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
		die("sigprocmask");

	/* Create the timer */

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &timerid;
	if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
		die("timer_create");

	/* Start the timer */

	its.it_value.tv_sec = TIMEOUT_NS / 1000000000;
	its.it_value.tv_nsec = TIMEOUT_NS % 1000000000;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	if (timer_settime(timerid, 0, &its, NULL) == -1)
		die("timer_settime");

	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
		die("sigprocmask");
}

static void handler(int UNUSED(sig), siginfo_t UNUSED(*si), void UNUSED(*uc)) {
	printf("%ld\n", time(NULL));
	printf("Timer fired!\n");
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
		handle_error(ret, "client poll");
		if (ret > 0) {
			if (fds[0].revents & POLLIN) {
				read(fds[0].fd, buf, 100000);
				printf("Received message from server: %s\n", buf);
			}
			if (fds[1].revents & POLLIN) {
				ret = read(fds[1].fd, buf, 100000);
				buf[ret] = 0;
				if(ret == 0) {
					printf("GUI disconnected\n");
					break;
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
