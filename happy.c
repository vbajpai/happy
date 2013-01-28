/*
 * happy.c --
 *
 * A simple TCP happy eyeballs probing tool. It uses non-blocking
 * connect() calls to establish connections concurrently to a number
 * of possible endpoints. This tool is particularly useful to
 * determine whether happy eyeball applications will use IPv4 or IPv6
 * if both are available.
 *
 * Juergen Schoenwaelder <j.schoenwaelder@jacobs-university.de>
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

typedef struct endpoint {
    int family;
    int socktype;
    int protocol;
    struct sockaddr_storage addr;
    socklen_t addrlen;

    int socket;
    struct timeval tvs;

    unsigned int sum;
    unsigned int idx;
    unsigned int cnt;
    unsigned int *values;
} endpoint_t;

typedef struct target {
    char *host;
    char *port;
    int num_endpoints;
    endpoint_t *endpoints;
    struct target *next;
} target_t;

static target_t *targets = NULL;

static int smode = 0;
static int skmode = 0;
static int nqueries = 3;
static int timeout = 2000;		/* in ms */
static unsigned int delay = 25;		/* in ms */

#define INVALID 0xffffffff

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
 * Resolve the host name and if successful establish a new target and
 * create the vector of endpoints we are going to probe subsequently.
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
	fprintf(stderr, "%s: %s port %s: %s (skipping)\n",
		progname, host, port, gai_strerror(n));
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
 * For all endpoints, create a socket and start a non-blocking
 * connect(). In order to avoid creating bursts of TCP SYN packets, we
 * take a short nap before each connect() call.
 */

static void
prepare(target_t *targets)
{
    int flags;
    target_t *tp;
    endpoint_t *ep;

    assert(targets);

    if (! targets->num_endpoints) {
        return;
    }

    for (tp = targets; target_valid(tp); tp = tp->next) {
	for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
	    ep->socket = socket(ep->family, ep->socktype, ep->protocol);
	    if (ep->socket < 0) {
		switch (errno) {
		case EAFNOSUPPORT:
		case EPROTONOSUPPORT:
		    continue;
		    
		default:
		    fprintf(stderr, "%s: socket: %s (skipping)\n",
			    progname, strerror(errno));
		    ep->socket = 0;
		    continue;
		}
	    }
	    
	    flags = fcntl(ep->socket, F_GETFL, 0);
	    if (fcntl(ep->socket, F_SETFL, flags | O_NONBLOCK) == -1) {
		fprintf(stderr, "%s: fcntl: %s (skipping)\n",
			progname, strerror(errno));
		(void) close(ep->socket);
		continue;
	    }

	    if (delay) {
		struct timeval timeout;
		timeout.tv_sec = delay / 1000;
		timeout.tv_usec = delay % 1000;
		(void) select(0, NULL, NULL, NULL, &timeout);
	    }
	    
	    if (connect(ep->socket, 
			(struct sockaddr *) &ep->addr,
			ep->addrlen) == -1) {
		if (errno != EINPROGRESS) {
		    (void) close(ep->socket);
		    fprintf(stderr, "%s: connect: %s (skipping)\n",
			    progname, strerror(errno));
		    continue;
		}
	    }
	    
	    (void) gettimeofday(&ep->tvs, NULL);
	}
    }
}


/*
 * Wait in a select() loop for the connect() requests to complete. If
 * the connect() was successful, collect basic timing statistics.
 */

static void
collect(target_t *targets)
{
    int rc, max;
    fd_set fdset;
    struct timeval tv, td, to;
    int soerror;
    socklen_t soerrorlen = sizeof(soerror);
    target_t *tp;
    endpoint_t *ep;

    assert(targets);

    while (1) {

	FD_ZERO(&fdset);
	for (tp = targets, max = -1; target_valid(tp); tp = tp->next) {
	    for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
		if (ep->socket) {
		    FD_SET(ep->socket, &fdset);
		    if (ep->socket > max) max = ep->socket;
		}
	    }
	}
	if (max == -1) {
	    break;
	}
	
	if (timeout) {
	    to.tv_sec = timeout / 1000;
	    to.tv_usec = timeout % 1000;
	}
	
	rc = select(1 + max, NULL, &fdset, NULL, timeout ? &to : NULL);
	if (rc == -1) {
	    fprintf(stderr, "%s: select failed: %s\n",
		    progname, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	
	if (rc == 0) {
	    /* timeout occured - fail all pending attempts and quit */
	    for (tp = targets; target_valid(tp); tp = tp->next) {
		for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
		    if (ep->socket) {
			ep->values[ep->idx] = INVALID;
			ep->idx++;
			(void) close(ep->socket);
			ep->socket = 0;
		    }
		}
	    }
	    return;
	}
    
	(void) gettimeofday(&tv, NULL);
	
	for (tp = targets; target_valid(tp); tp = tp->next) {
	    for (ep = tp->endpoints; endpoint_valid(ep); ep++) {
		if (ep->socket && FD_ISSET(ep->socket, &fdset)) {
		    
		    if (-1 == getsockopt(ep->socket, SOL_SOCKET, SO_ERROR,
					 &soerror, &soerrorlen)) {
			fprintf(stderr, "%s: getsockopt: %s\n",
				progname, strerror(errno));
			exit(EXIT_FAILURE);
		    }
		    (void) close(ep->socket);
		    ep->socket = 0;
		    if (! soerror) {
			/* calculate stats */
			timersub(&tv, &ep->tvs, &td);
			ep->values[ep->idx] = td.tv_sec*1000000 + td.tv_usec;
			ep->sum += ep->values[ep->idx];
			ep->cnt++;
			ep->idx++;
		    } else {
			ep->values[ep->idx] = INVALID;
			ep->idx++;
		    }
		}
	    }
	}
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

    if (! pa->cnt || ! pb->cnt) {
	return 0;
    }

    if (pa->sum/pa->cnt < pb->sum/pb->cnt) {
	return -1;
    } else if (pa->sum/pa->cnt > pb->sum/pb->cnt) {
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
		if (ep->values[i] != INVALID) {
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
		if (ep->values[i] != INVALID) {
		    printf(";%u", ep->values[i]);
		} else {
		    printf(";");
		}
	    }
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
 * Here is where the fun starts. Parse the options and run the
 * program in the requested mode.
 */

int
main(int argc, char *argv[])
{
    int i, j, c, p = 0;
    char *def_ports[] = { "80", 0 };
    char **usr_ports = NULL;
    char **ports = def_ports;

    while ((c = getopt(argc, argv, "d:p:q:f:hmst:")) != -1) {
	switch (c) {
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
		    "[-t timeout] [-d delay ] [-f file] [-s] [-m] "
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
	lock(stdout);
	if (skmode) {
	    report_sk(targets);
	} else {
	    report(targets);
	}
	unlock(stdout);
	cleanup(targets);
    }
	
    if (usr_ports) {
	(void) free(usr_ports);
    }
    
    return EXIT_SUCCESS;
}
