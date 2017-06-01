#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "err.h"

#define EXIT_FAIL 1

void die(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	printf("\n");
	exit(EXIT_FAIL);
}

void handle_error(int ret, const char *format, ...) {
	int errno_copy = errno;
	if(ret >= 0) return;

	va_list args;
	char *buf = malloc(1000);
	va_start(args, format);
	vsprintf(buf, format, args);
	errno = errno_copy;
	perror(buf);

	exit(EXIT_FAIL);
}
