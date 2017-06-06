#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <endian.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "err.h"
#include "timer.h"

void timer_prepare(long long int timeout_ns,
		void (*handler)(int, siginfo_t *, void *)) {
	timer_t timerid;
	struct sigevent sev;
	struct itimerspec its;
	sigset_t mask;
	struct sigaction sa;
	int ret;

	/* Establish handler for timer signal */

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset(&sa.sa_mask);
	ret = sigaction(SIGRTMIN, &sa, NULL);
	handle_error(ret, "sigaction");

	/* Block timer signal temporarily */

	sigemptyset(&mask);
	sigaddset(&mask, SIGRTMIN);
	if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
		die("sigprocmask");

	/* Create the timer */

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &timerid;
	if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
		die("timer_create");

	/* Start the timer */

	its.it_value.tv_sec = timeout_ns / 1000000000;
	its.it_value.tv_nsec = timeout_ns % 1000000000;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	if (timer_settime(timerid, 0, &its, NULL) == -1)
		die("timer_settime");

	/* Unblock signal */
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
		die("sigprocmask");
}

