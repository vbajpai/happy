/*
 * happy.c --
 *
 * Copyright (c) 2013, Juergen Schoenwaelder, Jacobs University Bremen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and
 * documentation are those of the authors and should not be
 * interpreted as representing official policies, either expressed or
 * implied, of the Leone Project or Jacobs University Bremen.
 */

#define _POSIX_C_SOURCE 2
#define _BSD_SOURCE
#define _DARWIN_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>

static const char *progname = "happy";

#ifndef NI_MAXHOST
#define NI_MAXHOST	1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV	32
#endif

#define EP_STATE_NEW		0x00
#define EP_STATE_CONNECTING	0x01
#define EP_STATE_CONNECTED	0x02
#define EP_STATE_TIMEDOUT	0x04
#define EP_STATE_FAILED		0x08

typedef struct endpoint {
    int family;
    int socktype;
    int protocol;
    struct sockaddr_storage addr;
    socklen_t addrlen;

    int socket;
    struct timeval tvs;
    int state;

    unsigned int sum;
    unsigned int tot;
    unsigned int idx;
    unsigned int cnt;
    int *values;

    unsigned int send;
    unsigned int rcvd;
} endpoint_t;

typedef struct target {
    char *host;
    char *port;
    int num_endpoints;
    endpoint_t *endpoints;
    struct target *next;
} target_t;

static target_t *targets = NULL;

static int pmode = 0;
static int smode = 0;
static int skmode = 0;
static int nqueries = 3;
static int timeout = 2000;		/* in ms */
static unsigned int delay = 25;		/* in ms */

static int pump_timeout = 2000;		/* in ms */

static int target_valid(target_t *tp) {
    return (tp && tp->host && tp->port && tp->endpoints);
}

static int endpoint_valid(endpoint_t *ep) {
    return (ep && ep->addrlen);
}

/*
 * A calloc() that exits if we run out of memory.
 */

static void*
xcalloc(size_t nmemb, size_t size)
{
    void *p = calloc(nmemb, size);
    if (!p) {
	fprintf(stderr, "%s: memory allocation failure\n", progname);
	exit(EXIT_FAILURE);
    }
    return p;
}

/*
 * Trim trailing and leading white space from a string. Note, this
 * version modifies the original string.
 */

static char *
trim(char * s)
{
    char * p = s;
    int l = strlen(p);

    while(isspace(p[l-1]) && l) p[--l] = 0;
    while(*p && isspace(*p) && l) ++p, --l;

    return p;
}

/*
 * If the file stream is associated with a regular file, lock the file
 * in order coordinate writes to a common file from multiple happy
 * instances. This is useful if, for example, multiple happy instances
 * try to append results to a common file.
 */

static void
lock(FILE *f)
{
    int fd;
    struct stat buf;
    static struct flock lock;

    assert(f);

    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_END;
    lock.l_len = 0;
    lock.l_pid = getpid();

    fd = fileno(f);
    if ((fstat(fd, &buf) == 0) && S_ISREG(buf.st_mode)) {
	if (fcntl(fd, F_SETLKW, &lock) == -1) {
	    fprintf(stderr, "%s: fcntl: %s (ignored)\n",
		    progname, strerror(errno));
	}
    }
}

/*
 * If the file stream is associated with a regular file, unlock the
 * file (which presumably has previously been locked).
 */

static void
unlock(FILE *f)
{
    int fd;
    struct stat buf;
    static struct flock lock;

    assert(f);

    lock.l_type = F_UNLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_END;
    lock.l_len = 0;
    lock.l_pid = getpid();

    fd = fileno(f);
    if ((fstat(fd, &buf) == 0) && S_ISREG(buf.st_mode)) {
	if (fcntl(fd, F_SETLKW, &lock) == -1) {
	    fprintf(stderr, "%s: fcntl: %s (ignored)\n",
		    progname, strerror(errno));
	}
    }
}

/*
 * Append a new target to our list of targets. We keep track of
 * the last target added so that we do not have to search for the
 * end of the list.
 */

static void
append(target_t *target)
{
    static target_t *last_target = NULL;

    if (target) {
	if (! targets) {
	    targets = target;
	} else {
	    last_target->next = target;
	}
	last_target = target;
    }
}

/*
 * Resolve the host and port name and if successful establish a new
 * target and create the vector of endpoints we are going to probe
 * subsequently.
 */

static target_t*
expand(const char *host, const char *port)
{
    struct addrinfo hints, *ai_list, *ai;
    int n;
    target_t *tp;
    endpoint_t *ep;

    assert(host && port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    n = getaddrinfo(host, port, &hints, &ai_list);
    if (n != 0) {
	fprintf(stderr, "%s: %s (skipping %s port %s)\n",
		progname, gai_strerror(n), host, port);
	return NULL;
    }

    tp = xcalloc(1, sizeof(target_t));
    tp->host = strdup(host);
    tp->port = strdup(port);

    for (ai = ai_list, tp->num_endpoints = 0;
	 ai; ai = ai->ai_next, tp->num_endpoints++) ;
    tp->endpoints = xcalloc(1 + tp->num_endpoints, sizeof(endpoint_t));

    for (ai = ai_list, ep = tp->endpoints; ai; ai = ai->ai_next, ep++) {
	ep->family = ai->ai_family;
	ep->socktype = ai->ai_socktype;
	ep->protocol = ai->ai_protocol;
	memcpy(&ep->addr, ai->ai_addr, ai->ai_addrlen);
	ep->addrlen = ai->ai_addrlen;
	ep->values = xcalloc(nqueries, sizeof(unsigned int));
    }
    
    freeaddrinfo(ai_list);

    return tp;
}

/*
 * Generate the file descriptor set for all sockets with a pending
 * asynchronous connect(). If the struct timeval argument is a valid
 * pointer, leave the smallest timestamp of a socket with a pending
 * asynchronous connect in the struct timeval.
 */

static int
generate_fdset(target_t *targets, fd_set *fdset, struct timeval *to)
{
    int max;
    target_t *tp;
    endpoint_t *ep;

    if (to) {
	timerclear(to);
    }
    FD_ZERO(fdset);
    for (tp = targets, max = -1; target_valid(tp); tp = tp->next) {
	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	    if (ep->state == EP_STATE_CONNECTING) {
		FD_SET(ep->socket, fdset);
		if (ep->socket > max) {
		    max = ep->socket;
		}
		if (to) {
		    if (! timerisset(to) || timercmp(&ep->tvs, to, <)) {
			*to = ep->tvs;
		    }
		}
	    }
	}
    }

    return max;
}

/*
 * Go through all endpoints and check which ones have timed out, for
 * which ones the asynchronous connect() has finished and update the
 * stats accordingly.
 */

static void
update(target_t *targets, fd_set *fdset)
{
    struct timeval tv, td;
    int soerror;
    socklen_t soerrorlen = sizeof(soerror);
    target_t *tp;
    endpoint_t *ep;
    unsigned int us;

    assert(targets && fdset);
    
    (void) gettimeofday(&tv, NULL);
    
    for (tp = targets; target_valid(tp); tp = tp->next) {
	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	    /* calculate time since we started the connect */
	    timersub(&tv, &ep->tvs, &td);
	    us = td.tv_sec*1000000 + td.tv_usec;
	    if (ep->state == EP_STATE_CONNECTING && us >= timeout * 1000) {
		ep->values[ep->idx] = -us;
		ep->idx++;
		ep->cnt++;
		(void) close(ep->socket);
		ep->socket = 0;
		ep->state = EP_STATE_TIMEDOUT;
		continue;
	    }
	    if (ep->state == EP_STATE_CONNECTING
		&& FD_ISSET(ep->socket, fdset)) {
		if (-1 == getsockopt(ep->socket, SOL_SOCKET, SO_ERROR,
				     &soerror, &soerrorlen)) {
		    fprintf(stderr, "%s: getsockopt: %s\n",
			    progname, strerror(errno));
		    exit(EXIT_FAILURE);
		}
		if (! soerror) {
		    ep->values[ep->idx] = us;
		    ep->sum += us;
		    ep->tot++;
		    ep->cnt++;
		    ep->idx++;
		} else {
		    ep->values[ep->idx] = -us;
		    ep->cnt++;
		    ep->idx++;
		}
		if (! pmode) {
		    (void) close(ep->socket);
		    ep->socket = 0;
		}
		ep->state = EP_STATE_CONNECTED;
	    }
	}
    }
}

/*
 * For all endpoints, create a socket and start a non-blocking
 * connect(). In order to avoid creating bursts of TCP SYN packets,
 * we take a short nap before each connect() call. Note that
 * asynchronous connect()s might actually succeed during the
 * short nap.
 */

static void
prepare(target_t *targets)
{
    int rc, flags;
    fd_set fdset;
    target_t *tp;
    endpoint_t *ep;
    struct timeval dts, dtn, dtd, dd;

    assert(targets);

    if (! targets->num_endpoints) {
        return;
    }

    dd.tv_sec = delay / 1000;
    dd.tv_usec = (delay % 1000) * 1000;

    for (tp = targets; target_valid(tp); tp = tp->next) {
	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {

	    if (delay) {
		int max;
		struct timeval to;
		
		(void) gettimeofday(&dts, NULL);
		
		while (1) {
		    max = generate_fdset(targets, &fdset, NULL);
		    
		    (void) gettimeofday(&dtn, NULL);
		    timersub(&dtn, &dts, &dtd);
		    if (timercmp(&dd, &dtd, <)) {
			break;
		    }
		    
		    timeradd(&dts, &dd, &to);
		    timersub(&to, &dtn, &to);
		    rc = select(1 + max, NULL, &fdset, NULL, &to);
		    if (rc == -1) {
			fprintf(stderr, "%s: select failed: %s\n",
				progname, strerror(errno));
			exit(EXIT_FAILURE);
		    }
		    update(targets, &fdset);
		}
	    }

	    ep->socket = socket(ep->family, ep->socktype, ep->protocol);
	    if (ep->socket < 0) {
		switch (errno) {
		case EAFNOSUPPORT:
		case EPROTONOSUPPORT:
		    continue;
		    
		default:
		    fprintf(stderr, "%s: socket: %s (skipping %s port %s)\n",
			    progname, strerror(errno), tp->host, tp->port);
		    ep->socket = 0;
		    ep->state = EP_STATE_FAILED;
		    continue;
		}
	    }
	    
	    flags = fcntl(ep->socket, F_GETFL, 0);
	    if (fcntl(ep->socket, F_SETFL, flags | O_NONBLOCK) == -1) {
		fprintf(stderr, "%s: fcntl: %s (skipping %s port %s)\n",
			progname, strerror(errno), tp->host, tp->port);
		(void) close(ep->socket);
		ep->socket = 0;
		ep->state = EP_STATE_FAILED;
		continue;
	    }

	    if (connect(ep->socket, 
			(struct sockaddr *) &ep->addr,
			ep->addrlen) == -1) {
		if (errno != EINPROGRESS) {
		    fprintf(stderr, "%s: connect: %s (skipping %s port %s)\n",
			    progname, strerror(errno), tp->host, tp->port);
		    (void) close(ep->socket);
		    ep->socket = 0;
		    ep->state = EP_STATE_FAILED;
		    continue;
		}
	    }

	    ep->state = EP_STATE_CONNECTING;	    
	    (void) gettimeofday(&ep->tvs, NULL);
	}
    }
}


/*
 * Wait in a select() loop for any pending connect() requests to
 * complete. If the connect() was successful, collect basic timing
 * statistics.
 */

static void
collect(target_t *targets)
{
    int rc, max;
    fd_set fdset;
    struct timeval to, ts, tn;

    assert(targets);

    while (1) {

	if (timeout) {
	    to.tv_sec = timeout / 1000;
	    to.tv_usec = (timeout % 1000) * 1000;
	}
	
	max = generate_fdset(targets, &fdset, timeout ? &ts : NULL);
	if (max == -1) {
	    break;
	}

	if (timeout) {
	    (void) gettimeofday(&tn, NULL);
	    timeradd(&ts, &to, &to);
	    timersub(&to, &tn, &to);
	}
	
	rc = select(1 + max, NULL, &fdset, NULL, timeout ? &to : NULL);
	if (rc == -1) {
	    fprintf(stderr, "%s: select failed: %s\n",
		    progname, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	
	update(targets, &fdset);
    }
}
    
/*
 * Sort the results for each target. This is in particular useful for
 * interactive usage.
 */

static int
cmp(const void *a, const void *b)
{
    endpoint_t *pa = (endpoint_t *) a;
    endpoint_t *pb = (endpoint_t *) b;

    if (! pa->tot || ! pb->tot) {
	return 0;
    }

    if (pa->sum/pa->tot < pb->sum/pb->tot) {
	return -1;
    } else if (pa->sum/pa->tot > pb->sum/pb->tot) {
	return 1;
    }
    return 0;
}

static void
sort(target_t *targets)
{
    target_t *tp;
    endpoint_t *ep;

    assert(targets);

    for (tp = targets; target_valid(tp); tp = tp->next) {
	if (tp->endpoints) {
	    qsort(tp->endpoints, tp->num_endpoints, sizeof(*ep), cmp);
	}
    }
}

/*
 * Report the results. For each endpoint of a target, we show the time
 * measured to establish a connection. This default format is intended
 * primarily for human readers.
 */

static void
report(target_t *targets)
{
    int i, n, len;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    target_t *tp;
    endpoint_t *ep;

    assert(targets);

    for (tp = targets; target_valid(tp); tp = tp->next) {

	printf("%s%s:%s\n",
	       (tp != targets) ? "\n" : "", tp->host, tp->port);

	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	
	    n = getnameinfo((struct sockaddr *) &ep->addr,
			    ep->addrlen,
			    host, sizeof(host), serv, sizeof(serv),
			    NI_NUMERICHOST | NI_NUMERICSERV);
	    if (n) {
		fprintf(stderr, "%s: getnameinfo: %s\n",
			progname, gai_strerror(n));
		continue;
	    }
	    printf(" %s%n", host, &len);
	    printf("%*s", (42-len), "");
	    for (i = 0; i < ep->idx; i++) {
		if (ep->values[i] >= 0) {
		    printf(" %4u.%03u",
			   ep->values[i] / 1000,
			   ep->values[i] % 1000);
		} else {
		    printf("     *   ");
		}
	    }
	    printf("\n");
	}
    }
}

/*
 * Report the pump results. For each endpoint of a target, we show the
 * bytes/seconds send and received.
 */

static void
report_pump(target_t *targets)
{
    int n, len;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    target_t *tp;
    endpoint_t *ep;

    assert(targets);

    if (!pmode) {
	return;
    }

    for (tp = targets; target_valid(tp); tp = tp->next) {

	printf("%s%s:%s\n",
	       (tp != targets) ? "\n" : "", tp->host, tp->port);

	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	    n = getnameinfo((struct sockaddr *) &ep->addr,
			    ep->addrlen,
			    host, sizeof(host), serv, sizeof(serv),
			    NI_NUMERICHOST | NI_NUMERICSERV);
	    if (n) {
		fprintf(stderr, "%s: getnameinfo: %s\n",
			progname, gai_strerror(n));
		continue;
	    }
	    printf(" %s%n", host, &len);
	    printf("%*s", (42-len), "");
	    printf(" %4u.%03u [sent]",
		   ep->send / pump_timeout * 1000 / 1000,
		   ep->send / pump_timeout * 1000 % 1000);
	    printf(" %4u.%03u [rcvd]",
		   ep->rcvd / pump_timeout * 1000 / 1000,
		   ep->rcvd / pump_timeout * 1000 % 1000);
	    printf("\n");
	}
    }
}

/*
 * Report the results. This function produces a more compact semicolon
 * separated output format intended for consumption by other programs.
 */

static void
report_sk(target_t *targets)
{
    int i, n;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    target_t *tp;
    endpoint_t *ep;
    time_t now;

    assert(targets);

    now = time(NULL);

    for (tp = targets; target_valid(tp); tp = tp->next) {

	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {

	    n = getnameinfo((struct sockaddr *) &ep->addr,
			    ep->addrlen,
			    host, sizeof(host), serv, sizeof(serv),
			    NI_NUMERICHOST | NI_NUMERICSERV);
	    if (n) {
		fprintf(stderr, "%s: getnameinfo: %s\n",
			progname, gai_strerror(n));
		continue;
	    }

	    printf("HAPPY.0;%lu;%s;%s;%s;%s",
		   now, ep->cnt ? "OK" : "FAIL", tp->host, tp->port, host);
	    for (i = 0; i < ep->idx; i++) {
		printf(";%d", ep->values[i]);
	    }
	    printf("\n");
	}
    }
}

/*
 * Report the pump results. This function produces a more compact
 * semicolon separated output format intended for consumption by other
 * programs.
 */

static void
report_pump_sk(target_t *targets)
{
    int n;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    target_t *tp;
    endpoint_t *ep;
    time_t now;

    assert(targets);

    if (! pmode) {
	return;
    }

    now = time(NULL);

    for (tp = targets; target_valid(tp); tp = tp->next) {

	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {

	    n = getnameinfo((struct sockaddr *) &ep->addr,
			    ep->addrlen,
			    host, sizeof(host), serv, sizeof(serv),
			    NI_NUMERICHOST | NI_NUMERICSERV);
	    if (n) {
		fprintf(stderr, "%s: getnameinfo: %s\n",
			progname, gai_strerror(n));
		continue;
	    }

	    printf("PUMP.0;%lu;%s;%s;%s;%s",
		   now, ep->cnt ? "OK" : "FAIL", tp->host, tp->port, host);
	    printf(";%u.%03u",
		   ep->send / pump_timeout * 1000 / 1000,
		   ep->send / pump_timeout * 1000 % 1000);
	    printf(";%u.%03u",
		   ep->rcvd / pump_timeout * 1000 / 1000,
		   ep->rcvd / pump_timeout * 1000 % 1000);
	    printf("\n");
	}
    }
}

/*
 * Cleanup targets and release all target data structures.
 */

static void
cleanup(target_t *targets)
{
    target_t *tp, *np;
    endpoint_t *ep;

    assert(targets);

    for (tp = targets; target_valid(tp); tp = np) {
	np = tp->next;
	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	    if (ep->socket) {
		(void) close(ep->socket);
	    }
	    if (ep->values) {
		(void) free(ep->values);
	    }
	}
	if (tp->endpoints) (void) free(tp->endpoints);
	if (tp->host) (void) free(tp->host);
	if (tp->port) (void) free(tp->port);
	(void) free(tp);
    }
}

/*
 * Read a list of targets from a file or standard input if the
 * filename is '-'.
 */

static void
import(const char *filename, char **ports)
{
    FILE *in;
    char line[512], *host;
    int j;

    if (! filename || strcmp(filename, "-") == 0) {
        clearerr(stdin);
        in = stdin;
    } else {
        in = fopen(filename, "r");
        if (! in) {
	    fprintf(stderr, "%s: fopen: %s\n",
		    progname, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    
    while (fgets(line, sizeof(line), in)) {
	host = trim(line);
	if (*host) {
	    for (j = 0; ports[j]; j++) {
		append(expand(host, ports[j]));
	    }
	}
    }
    
    if (ferror(in)) {
	fprintf(stderr, "%s: ferror: %s\n",
		progname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (in != stdin) {
        fclose(in);
    }

}

/*
 * Pump connections with HTTP GET requests and measure the datarate
 * (throughput) of the stream of responses.
 */

static void
pump(target_t *targets)
{
    static char const template[] =
	"GET / HTTP/1.1\r\n"
	"Host: %s\r\n"
	"User-Agent: pump/0.1\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: Keep-Alive\r\n"
	"\r\n";

    static char *msg;
    target_t *tp, *np;
    endpoint_t *ep;
    struct timeval ts, tn, td;
    fd_set rfds, wfds;
    char buffer[8192];
    unsigned int us;
    int rc;

    assert(targets);

    for (tp = targets; target_valid(tp); tp = np) {
	np = tp->next;
	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	    if (ep->state != EP_STATE_CONNECTED) {
		continue;
	    }
	    msg = malloc(strlen(template)+strlen(tp->host));
	    if (! msg) {
		fprintf(stderr, "%s: malloc failed for %s\n",
			progname, tp->host);
		continue;
	    }
	    snprintf(msg, strlen(template)+strlen(tp->host), template, tp->host);

	    (void) gettimeofday(&ts, NULL);
	    us = 0;
	    while (us < pump_timeout * 1000) {
		FD_ZERO(&rfds);
		FD_SET(ep->socket, &rfds);
		FD_ZERO(&wfds);
		FD_SET(ep->socket, &wfds);
		rc = select(1 + ep->socket, &rfds, &wfds, NULL, NULL);
		if (rc == -1) {
		    fprintf(stderr, "%s: select failed: %s\n",
			    progname, strerror(errno));
		    exit(EXIT_FAILURE);
		}
		if (FD_ISSET(ep->socket, &rfds)) {
		    ep->rcvd += read(ep->socket, buffer, sizeof(buffer));
		}
		if (FD_ISSET(ep->socket, &wfds)) {
		    ep->send += write(ep->socket, msg, strlen(msg));
		}
		(void) gettimeofday(&tn, NULL);
		timersub(&tn, &ts, &td);
		us = td.tv_sec*1000000 + td.tv_usec;
	    }

	    free(msg);
	}
    }
}

/*
 * Here is where the fun starts. Parse the command line options and
 * run the program in the requested mode.
 */

int
main(int argc, char *argv[])
{
    int i, j, c, p = 0;
    char *def_ports[] = { "80", 0 };
    char **usr_ports = NULL;
    char **ports = def_ports;

    while ((c = getopt(argc, argv, "bd:p:q:f:hmst:")) != -1) {
	switch (c) {
	case 'b':
	    pmode = 1;
	    break;
	case 'd':
	    {
	        char *endptr;
		int num = strtol(optarg, &endptr, 10);
		if (num >= 0 && *endptr == '\0') {
		    delay = num;
		} else {
		    fprintf(stderr, "%s: invalid argument '%s' "
			    "for option -d\n", progname, optarg);
		    exit(EXIT_FAILURE);
		}
	    }
	    break;
	case 'p':
	    if (! usr_ports) {
		usr_ports = xcalloc(argc, sizeof(char *));
		ports = usr_ports;
	    }
	    usr_ports[p++] = optarg;
	    break;
	case 'q':
	    {
	        char *endptr;
		int num = strtol(optarg, &endptr, 10);
		if (num > 0 && *endptr == '\0') {
		    nqueries = num;
		} else {
		    fprintf(stderr, "%s: invalid argument '%s' "
			    "for option -q\n", progname, optarg);
		    exit(EXIT_FAILURE);
		}
	    }
	    break;
	case 'f':
	    import(optarg, ports);
	    break;
	case 'm':
	    skmode = 1;
	    break;
	case 's':
	    smode = 1;
	    break;
	case 't':
	    {
		char *endptr;
		int num = strtol(optarg, &endptr, 10);
		if (num > 0 && *endptr == '\0') {
		    timeout = num;
		} else {
		    fprintf(stderr, "%s: invalid argument '%s' "
			    "for option -t\n", progname, optarg);
		    exit(EXIT_FAILURE);
		}
	    }
	    break;
	case 'h':
	default: /* '?' */
	    fprintf(stderr,
		    "Usage: %s [-p port] [-q nqueries] "
		    "[-t timeout] [-d delay ] [-f file] [-s] [-m] [-b] "
		    "hostname...\n", progname);
	    exit(EXIT_FAILURE);
	}
    }
    argc -= optind;
    argv += optind;
    
    for (i = 0; i < argc; i++) {
	for (j = 0; ports[j]; j++) {
	    append(expand(argv[i], ports[j]));
	}
    }

    if (targets) {
	for (i = 0; i < nqueries; i++) {
	    prepare(targets);
	    collect(targets);
	}
	if (smode) {
	    sort(targets);
	}
	if (pmode) {
	    pump(targets);
	}
	lock(stdout);
	if (skmode) {
	    report_sk(targets);
	} else {
	    report(targets);
	}
	if (pmode) {
	    if (skmode) {
		report_pump_sk(targets);
	    } else {
		printf("\n");
		report_pump(targets);
	    }
	}
	unlock(stdout);
	cleanup(targets);
    }
	
    if (usr_ports) {
	(void) free(usr_ports);
    }
    
    return EXIT_SUCCESS;
}
