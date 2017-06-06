#ifndef SERVER_MSG_QUEUE_H
#define SERVER_MSG_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


	void message_queue_new();
	void message_insert(struct server_msg_internal* event);
	struct server_msg_internal* message_get(int i);
	int message_queue_size();
	void message_remove();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SERVER_MSG_QUEUE_H */
