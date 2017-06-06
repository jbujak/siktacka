#ifndef TIMER_H
#define TIMER_H
#include <signal.h>

void timer_prepare(long long int timeout_ns,
		void (*handler)(int, siginfo_t *, void *));

#endif /* TIMER_H */
