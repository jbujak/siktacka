#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "err.h"

void die(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	printf("\n");
	exit(EXIT_FAIL);
}
