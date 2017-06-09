#define _DEFAULT_SOURCE
#include <stdbool.h>

#include "common.h"
#include "string.h"
#include "validator.h"

bool port_valid(int port) {
	if (port <= 0)
		return false;
	if (port > MAX_PORT_NUM)
		return false;
	return true;
}

bool player_name_valid(const char *name) {
	int len = strnlen(name, MAX_NAME_LENGTH + 1);
	if (len > MAX_NAME_LENGTH)
		return false;
	for (int i = 0; i < len; i++) {
		if(name[i] < MIN_NAME_CHAR || name[i] > MAX_NAME_CHAR)
			return false;
	}
	return true;
}
