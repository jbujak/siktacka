#include <vector>

#include "event_queue.h"

extern "C" {
	static  std::vector<struct event*> events;

	void event_queue_new() {
		events.clear();
	}

	void event_insert(struct event* event) {
		events.push_back(event);
	}

	struct event* event_get(int i) {
		return events[i];
	}

	int event_queue_size() {
		return events.size();
	}
}
