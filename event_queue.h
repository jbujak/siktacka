#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	void event_queue_new();
	void event_insert(struct event* event);
	struct event* event_get(int i);
	int event_queue_size();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EVENT_QUEUE_H */
