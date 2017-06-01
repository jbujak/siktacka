#include <stdio.h>
#include <unistd.h>

#include "client.h"
#include "parser.h"
#include "gui_client.h"

#define DEFAULT_GAME_SERVER_PORT 12345
#define DEFAULT_UI_SERVER_NAME "localhost"
#define DEFAULT_UI_SERVER_PORT 12346


static struct client_config config = (struct client_config) {
	.player_name = "",
	.game_server = "",
	.game_server_port = DEFAULT_GAME_SERVER_PORT,
	.ui_server = DEFAULT_UI_SERVER_NAME,
	.ui_server_port = DEFAULT_UI_SERVER_PORT
};

static int gui_socket;

static void init();

int main(int argc, char * const argv[]) {
	parse_client_arguments(argc, argv, &config);
	init();

	return 0;
}

static void init() {
	gui_socket = gui_init(config.ui_server_port, config.ui_server);
}
