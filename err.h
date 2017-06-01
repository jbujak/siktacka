#ifndef ERR_H
#define ERR_H

#define EXIT_FAIL 1

void die(const char *format, ...);

void handle_error(int ret, const char *format, ...);

#endif /* ERR_H */
