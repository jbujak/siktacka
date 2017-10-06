#define _DEFAULT_SOURCE

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include "err.h"
#include "gui_client.h"

#define BUF_SIZE 5000

static int sock;
static char buf[BUF_SIZE];

static void send_buffer();

int gui_init(int port_num, const char *host) {
	struct addrinfo addr_hints;
	struct addrinfo *addr_result;
	char port_str[20];
	int ret;

	memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_STREAM;
	addr_hints.ai_protocol = IPPROTO_TCP;
	sprintf(port_str, "%d", port_num);

	ret = getaddrinfo(host, port_str, &addr_hints, &addr_result);
	handle_error(ret, "gui_prepare getaddrinfo");

	sock = socket(addr_result->ai_family,
			addr_result->ai_socktype, addr_result->ai_protocol);
	handle_error(sock, "gui_prepare socket");

	 int flag = 1;
         ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	 handle_error(ret, "gui_prepare setsockopt");

	ret = connect(sock, addr_result->ai_addr, addr_result->ai_addrlen);
	handle_error(ret, "gui_prepare connect");

	freeaddrinfo(addr_result);
	return sock;
}

void gui_new_game(int maxx, int maxy, int players_num, char *players[]) {
	char *buf_end = buf;
	buf_end += sprintf(buf, "NEW_GAME %d %d ", maxx, maxy);
	for(int i = 0; i < players_num; i++) {
		buf_end += sprintf(buf_end, "%s ", players[i]);
	}
	buf_end += sprintf(buf_end, "\n");
	send_buffer();
}

void gui_pixel(int x, int y, char *player) {
	sprintf(buf, "PIXEL %d %d %s\n", x, y, player);
	send_buffer();
}

void gui_eliminated(char *player) {
	sprintf(buf, "PLAYER_ELIMINATED %s\n", player);
	send_buffer();
}

static void send_buffer() {
	int ret;
	ret = write(sock, buf, strlen(buf));
	handle_error(ret, "gui_client write");
}
