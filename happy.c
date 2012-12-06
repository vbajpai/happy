/*
 * happy.c --
 *
 * A simple TCP happy eye balls probing tool. It uses non-blocking
 * connect() calls to establish connections concurrently to a number
 * of possible endpoints. This tool is particularly useful to
 * determine whether happy eye ball applications will use IPv4 or IPv6
 * if both are available.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>

static const char *progname = "happy";

#ifndef NI_MAXHOST
#define NI_MAXHOST	1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV	32
#endif

typedef struct endpoint {
    const char *host;
    const char *port;
    int family;
    int socktype;
    int protocol;
    struct sockaddr_storage addr;
    socklen_t addrlen;

    int socket;
    struct timeval tvs;

    unsigned int cur;
    unsigned int min;
    unsigned int max;
    unsigned int sum;
    unsigned int cnt;
} endpoint_t;

static endpoint_t *endpoints = NULL;

static int smode = 0;

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
 * Resolve the host name and create the vector of endpoints we are
 * going to probe subsequently.
 */

static void
expand(const char *host, const char *port)
{
    struct addrinfo hints, *ai_list, *ai;
    int i = 0, n;
    long max;
    endpoint_t *ep;

    assert(host && port);
    
    max = sysconf(_SC_OPEN_MAX);
    if (max == -1) {
	fprintf(stderr, "%s: sysconf: %s\n",
		progname, strerror(errno));
	exit(EXIT_FAILURE);
    }
    
    if (! endpoints) {
	endpoints = xcalloc(sizeof(endpoint_t), max);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    n = getaddrinfo(host, port, &hints, &ai_list);
    if (n != 0) {
	fprintf(stderr, "%s: getaddrinfo: %s (skipping)\n",
		progname, gai_strerror(n));
	return;
    }

    for (ai = ai_list; ai; ai = ai->ai_next) {
	
	while (i < max && endpoints[i].host) i++;
	if (i == max) {
	    fprintf(stderr, "%s: out of socket descriptors\n", 
		    progname);
	    exit(EXIT_FAILURE);
	}

	ep = &endpoints[i];
	ep->host = host;
	ep->port = port;
	ep->family = ai->ai_family;
	ep->socktype = ai->ai_socktype;
	ep->protocol = ai->ai_protocol;
	memcpy(&ep->addr, ai->ai_addr, ai->ai_addrlen);
	ep->addrlen = ai->ai_addrlen;
    }
    
    freeaddrinfo(ai_list);
}

/*
 * For all endpoints, create a socket and start a non-blocking
 * connect().
 */

static void
prepare()
{
    int flags;
    endpoint_t *ep;

    if (! endpoints) {
        return;
    }

    for (ep = endpoints; ep->host; ep++) {
        ep->socket = socket(ep->family, ep->socktype, ep->protocol);
        if (ep->socket < 0) {
	    switch (errno) {
	    case EAFNOSUPPORT:
	    case EPROTONOSUPPORT:
		continue;
		
	    default:
		fprintf(stderr, "%s: socket: %s (skipping)\n",
			progname, strerror(errno));
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


/*
 * Wait in a select() loop for the connect() requests to complete. If
 * the connect() was successful, collect basic timing statistics.
 */

static void
collect(void)
{
    int max;
    fd_set fdset;
    struct timeval tv, td;
    int soerror;
    socklen_t soerrorlen = sizeof(soerror);
    endpoint_t *ep;

    while (endpoints) {
        max = -1;
	FD_ZERO(&fdset);
	for (ep = endpoints; ep->host; ep++) {
	    if (ep->socket) {
	        FD_SET(ep->socket, &fdset);
		if (ep->socket > max) max = ep->socket;
	    }
	}

	if (max == -1) {
	    return;
	}

        if (select(1 + max, NULL, &fdset, NULL, NULL) == -1) {
            fprintf(stderr, "%s: select failed: %s\n",
                    progname, strerror(errno));
            exit(EXIT_FAILURE);
        }

	(void) gettimeofday(&tv, NULL);

	for (ep = endpoints; ep->host; ep++) {
	    if (ep->socket && FD_ISSET(ep->socket, &fdset)) {

		if (-1 == getsockopt(ep->socket, SOL_SOCKET, SO_ERROR,
				     &soerror, &soerrorlen)) {
		    fprintf(stderr, "%s: getsockopt: %s\n",
			    progname, strerror(errno));
		    exit(EXIT_FAILURE);
		}

		if (! soerror) {
		    /* calculate stats */
		    timersub(&tv, &ep->tvs, &td);
		    ep->cnt++;
		    ep->cur = td.tv_sec*1000000 + td.tv_usec;
		    ep->sum += ep->cur;
		    if (ep->cnt == 1 || ep->cur < ep->min) {
			ep->min = ep->cur;
		    }
		    if (ep->cnt == 1 || ep->cur > ep->max) {
			ep->max = ep->cur;
		    }
		}

	        (void) close(ep->socket);
	        ep->socket = 0;
	    }
	}
    }
}

/*
 * Report the results. For each endpoint, we show the min, avg, max
 * time measured to establish a connection. We sort the results for
 * the endpoints associated with a certain host / port pair by the
 * measured average connection time. There is also a more compact
 * semicolon separated output format intended for other programs to
 * read and process it.
 */

static int
cmp(const void *a, const void *b)
{
    endpoint_t *pa = (endpoint_t *) a;
    endpoint_t *pb = (endpoint_t *) b;
    int aa, ab;

    if (! pa->cnt) return 1;
    if (! pb->cnt) return -1;

    aa = pa->sum/pa->cnt;
    ab = pb->sum/pb->cnt;
    
    if (aa < ab) {
	return -1;
    } else if (aa > ab) {
	return 1;
    }
    return 0;
}

static void
report(void)
{
    int n, cnt, len;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    endpoint_t *ep, *prev_ep;
    time_t now;

    if (! endpoints) {
        return;
    }

    /* Lets first sort the results for each host / port combination by
     * passing proper slices of the endpoints array to qsort(). */

    for (prev_ep = NULL, ep = endpoints; ep->host; ep++) {
	if (!prev_ep || strcmp(ep->host, prev_ep->host) != 0
	    || strcmp(ep->port, prev_ep->port) != 0) {
	    if (prev_ep) {
		qsort(prev_ep, cnt, sizeof(*ep), cmp);
	    }
	    prev_ep = ep;
	    cnt = 1;
	} else {
	    cnt++;
	}
    }
    if (prev_ep) {
	qsort(prev_ep, cnt, sizeof(*ep), cmp);
    }

    /* Ready for producing human readable output. */

    if (smode) {
	now = time(NULL);
    }

    for (prev_ep = NULL, ep = endpoints; ep->host; ep++) {

        if (!prev_ep || strcmp(ep->host, prev_ep->host) != 0
	    || strcmp(ep->port, prev_ep->port) != 0) {
	    if (smode) {
		printf("%sHAPPY.0;%lu;OK;%s;%s", prev_ep ? "\n" : "",
		       now, ep->host, ep->port);
	    } else {
		if (prev_ep) {
		    printf("\n");
		}
		printf("%s:%s%n", ep->host, ep->port, &len);
		printf("%*s  MIN ms   AVG ms   MAX ms\n", (48-len), "");
	    }
	    prev_ep = ep;
	}
      
	n = getnameinfo((struct sockaddr *) &ep->addr, 
			ep->addrlen,
			host, sizeof(host), serv, sizeof(serv),
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (n) {
  	    fprintf(stderr, "%s: getnameinfo: %s\n",
		    progname, gai_strerror(n));
	    return;
	}
	printf("%s%s%n", (smode) ? ";" : " ", host, &len);
	if (smode) {
	    if (!ep->cnt) {
		printf(";;;");
	    } else {
		printf(";%u.%03u;%u.%03u;%u.%03u",
		       ep->min/1000, ep->min%1000,
		       (ep->sum/ep->cnt)/1000, (ep->sum/ep->cnt)%1000,
		       ep->max/1000, ep->max%1000);
	    }
	} else {
	    if (! ep->cnt) {
		printf("%*s%8s %8s %8s\n", 
		       (48-len), "", "-", "-", "-");
	    } else {
		printf("%*s%4u.%03u %4u.%03u %4u.%03u\n", 
		       (48-len), "",
		       ep->min/1000, ep->min%1000, 
		       (ep->sum/ep->cnt)/1000, (ep->sum/ep->cnt)%1000, 
		       ep->max/1000, ep->max%1000);
	    }
	}
    }
    if (smode) {
	if (prev_ep) {
	    printf("\n");
	}
    }
}

int
main(int argc, char *argv[])
{
    int i, j, c, p = 0;
    int nqueries = 3;
    char *def_ports[] = { "80", 0 };
    char **usr_ports = NULL;
    char **ports = def_ports;

    while ((c = getopt(argc, argv, "p:q:hs")) != -1) {
	switch (c) {
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
		  exit(1);
		}
	    }
	    break;
	case 's':
	    smode = 1;
	    break;
	case 'h':
	default: /* '?' */
	    fprintf(stderr, "Usage: %s [-p port] [-q nqueries] "
		    "hostname...\n", progname);
	    exit(EXIT_FAILURE);
	}
    }

    for (i = optind; i < argc; i++) {
        for (j = 0; ports[j]; j++) {
	    expand(argv[i], ports[j]);
	}
    }

    for (i = 0; i < nqueries; i++) {
	prepare();
	collect();
    }
    report();

    if (endpoints) {
        (void) free(endpoints);
    }

    if (usr_ports) {
	(void) free(usr_ports);
    }
    
    return EXIT_SUCCESS;
}
