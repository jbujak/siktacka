#include <deque>

#include "event_queue.h"
#include "server.h"

extern "C" {
	static  std::deque<struct server_msg_internal*> events;

	void message_queue_new() {
		events.clear();
	}

	void message_insert(struct server_msg_internal* event) {
		events.push_back(event);
	}

	struct server_msg_internal* message_get(int i) {
		return events[i];
	}

	int message_queue_size() {
		return events.size();
	}

	void message_remove() {
		events.pop_front();
	}
}
