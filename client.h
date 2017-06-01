#ifndef CLIENT_H
#define CLIENT_H

#define MIN_NAME_LENGTH 1
#define MAX_NAME_LENGTH 64
#define MIN_NAME_CHAR 33
#define MAX_NAME_CHAR 126
#define MAX_HOST_LENGTH 60

struct client_config {
	char player_name[MAX_NAME_LENGTH];
	char game_server[MAX_HOST_LENGTH];
	int game_server_port;
	char ui_server[MAX_HOST_LENGTH];
	int ui_server_port;
};

#endif /* CLIENT_H */
