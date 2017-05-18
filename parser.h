#ifndef PARSER_H
#define PARSER_H

#include "client.h"
#include "server.h"

void parse_server_arguments(int argc, char * const argv[], struct server_config *config);
void parse_client_arguments(int argc, char * const argv[], struct client_config *config);

#endif /* PARSER_H */
