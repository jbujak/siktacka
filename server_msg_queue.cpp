#include <deque>
#include <cstdlib>

#include "server_msg_queue.h"
#include "server.h"

extern "C" {
	static  std::deque<struct server_msg_internal*> messages;

	void message_queue_new() {
		for(struct server_msg_internal *msg: messages) {
			free(msg);
		}
		messages.clear();
	}

	void message_insert(struct server_msg_internal* event) {
		messages.push_back(event);
	}

	struct server_msg_internal* message_get(int i) {
		return messages[i];
	}

	int message_queue_size() {
		return messages.size();
	}

	void message_remove() {
		messages.pop_front();
	}
}
