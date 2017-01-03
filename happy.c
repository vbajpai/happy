/*
 * happy.c --
 *
 * Copyright (c) 2013, Juergen Schoenwaelder, Jacobs University Bremen
 * Copyright (c) 2014, Vaibhav Bajpai, Jacobs University Bremen
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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

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
    char *canonname;
    char *reversename;

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

static int dmode = 0;
static int pmode = 0;
static int cmode = 0;
static int smode = 0;
static int skmode = 0;
static int nqueries = 3;
static int timeout = 2000;		/* in ms */
static unsigned int delay = 25;		/* in ms */

static int pump_timeout = 2000;		/* in ms */

static int target_valid(target_t *tp) {
    return (tp && tp->host && tp->port);
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
 * A realloc() that exits if we run out of memory.
 */

static void*
xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
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
 * Handler to parse DNS response messages; particularly CNAME
 * answers. This is used because getaddrinfo(...) and AI_CANONNAME
 * cannot be used with the glibc version on SamKnows probes. The
 * version returns PTR entries when asked for a CNAME. Therefore
 * explicit CNAME query request needs to be made and a handler
 * function is needed to parse CNAME responses.
 */

static char*
parse_cname_response(const u_char* const answer, const int answerlen)
{

    /* initialize data structure to store the parsed response */
    ns_msg handle;
    if (
	ns_initparse(
	    answer,     /* answer buffer */
	    answerlen,  /* true answer length */
	    &handle     /* data structure filled in by ns_initparse */
	    ) < 0
	) {
	herror("ns_initparse(...)");
	exit(EXIT_FAILURE);
    }
    
    /* Ideally one would iterate over each resource record returned by
     * ns_msg_count(...). However, this is a specific parse function for CNAME
     * records and because a RR cannot have any other records along with a
     * CNAME [RFC 1912], there is no need to iterate in a loop, but only once,
     * therefore rrnum is set to 0 */
    
    ns_rr rr; int rrnum = 0;
    
    /* parse the answer section of the resource record */
    if (
	ns_parserr(
	    &handle, /* data structure filled by ns_initparse */
	    ns_s_an, /* resource record answer section */
	    rrnum,   /* resource record index in this section */
	    &rr      /* data structure filled in by ns_parseerr */
	    ) < 0
	) {
	
	/* continue to the next resource records if this cannot be parsed */
	if (errno != ENODEV) {
	    herror("ns_parserr(...)");
	    exit(EXIT_FAILURE);
	}
    }
    
    char* dst = NULL;
    switch (ns_rr_type(rr)) {
	
	/* type: CNAME record */
    case ns_t_cname:
	dst = (char *) xcalloc (1, NI_MAXHOST);
	
	/* Uncompress the DNS string */
	if (
	    ns_name_uncompress(
		ns_msg_base(handle) /* Message Start */
		, ns_msg_end(handle)  /* Message End */
		, ns_rr_rdata(rr)     /* Message Position */
		, dst                 /* Result */
		, NI_MAXHOST          /* Result Size */
		) < 0
	    ) {
	    (void) fprintf(stderr, "ns_name_uncompress failed\n");
	    free(dst); dst = NULL;
	    exit(EXIT_FAILURE);
	}
	
	break;
	
	/* ignore all the other types */
    default:
	break;
    }
    
    return dst;
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
    char* canonname = NULL;
    int n;
    target_t *tp;
    endpoint_t *ep;

    assert(host && port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (dmode) {
	
	/*  In order to get the CNAME, one can set the AI_CANONNAME
	 *  flag in hints.ai_flags. However it appears glibc tends to
	 *  return PTR entries when this flag is turned on (see:
	 *  http://marc.info/?l=glibc-alpha&m=109239199429843&w=2). The
	 *  newer versions of glibc have been patched, however the
	 *  version running on SamKnows probes still has this
	 *  behavior. As a workaround, we use BIND functions to
	 *  explicitly send a CNAME query and parse the DNS response
	 *  ourselves. The answer string is then plugged back into the
	 *  result structures used below.
	 */

	/* hints.ai_flags |= AI_CANONNAME; */

	/* list to keep a chain of CNAME strings */
	short dstset_num = 0;
	char** dstset = NULL;
	const char* query_name = host;
	while (1) {
	    
	    /* send a DNS query for CNAME record of the input service name */
	    u_char answer[NS_PACKETSZ];
	    int answerlen = res_search (
		query_name        /* domain name */
		, ns_c_in           /* class type, see: arpa/nameserv.h */
		, ns_t_cname        /* rr type, see: arpa/nameserv.h */
		, (u_char *) answer /* answer buffer */
		, NS_PACKETSZ       /* answer buffer length */
		);
	    if (answerlen == -1) {
		break;
	    } else {
		
		/* parse the received response */
		canonname = parse_cname_response (
		    answer     /* received response */
		    , answerlen  /* true response len */
		    );
	    }
	    
	    /* list to keep a chain of CNAME strings */
	    if (canonname != NULL) {
		if (dstset_num == 0) {
		    dstset = xcalloc(dstset_num + 1, sizeof(char*));
		} else {
		    dstset = xrealloc(dstset, (dstset_num+1) * sizeof(char*));
		}
		dstset[dstset_num] = canonname;
		dstset_num += 1;
	 	query_name = canonname;
	    }
	}
	
	/* create a string representation of CNAME chains */
	canonname = NULL;
	for (int i = 0; i < dstset_num; i++){
	    char* dangler = canonname;
	    char* dst = dstset[i];
	    if (i == 0) {
		asprintf(&canonname, "%s", dst);
		free(dst);
		continue;
	    }
	    if (i == 1) {
		asprintf(&canonname, "%s >", dangler);
		free(dangler); dangler = canonname;
	    }
	    if ((i + 1) == dstset_num) {
		asprintf(&canonname, "%s %s",dangler,dst);
	    } else {
		asprintf(&canonname, "%s %s >", dangler, dst);
	    }
	    if (dst != NULL) { free(dst); dst = NULL; }
	    if (dangler !=NULL) { free(dangler); dangler = NULL; }
	}
	free(dstset); dstset = NULL;
    }

    tp = xcalloc(1, sizeof(target_t));
    tp->host = strdup(host);
    tp->port = strdup(port);

    n = getaddrinfo(host, port, &hints, &ai_list);
    if (n != 0) {
        fprintf(stderr, "%s: getaddrinfo: %s (skipping %s port %s)\n",
                progname, gai_strerror(n), host, port);
	return tp;
    }

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
	if (dmode) {
	    char revname[NI_MAXHOST];
	    int n;
	    revname[0] = 0;
	    if (canonname != NULL) {
		ep->canonname = strdup(canonname);
	    } else {
		ep->canonname = strdup(host);
	    }

	    n = getnameinfo(ai->ai_addr, ai->ai_addrlen,
			    revname, sizeof(revname), NULL, 0,
			    NI_NAMEREQD);
	    if (n && n != EAI_NONAME) {
		fprintf(stderr, "%s: getnameinfo: %s\n",
			progname, gai_strerror(n));
	    } else {
		if (strlen(revname)) {
		    ep->reversename = strdup(revname);
		}
	    }
	}
    }

    freeaddrinfo(ai_list);
    free(canonname); canonname = NULL;

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
 * Report the dns results. For each endpoint of a target, we show the
 * canonical name and the reverse name.
 */

static void
report_dns(target_t *targets)
{
    int n, len;
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
	    printf(" %s > %s%n", ep->canonname, host, &len);
	    if (ep->reversename) {
		printf(" > %s", ep->reversename);
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

	if (! tp->endpoints) {
            printf("HAPPY.0.4;%lu;%s;%s;%s\n",
                   now, "FAIL", tp->host, tp->port);
	}

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

            printf("HAPPY.0.4;%lu;%s;%s;%s;%s",
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

    now = time(NULL);

    for (tp = targets; target_valid(tp); tp = tp->next) {

	if (! tp->endpoints) {
            printf("PUMP.0.4;%lu;%s;%s;%s\n",
                   now, "FAIL", tp->host, tp->port);
	}

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

            printf("PUMP.0.4;%lu;%s;%s;%s;%s",
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
 * Report the dns results. This function produces a more compact
 * semicolon separated output format intended for consumption by other
 * programs.
 */

static void
report_dns_sk(target_t *targets)
{
    int n;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    target_t *tp;
    endpoint_t *ep;
    time_t now;

    assert(targets);

    now = time(NULL);

    for (tp = targets; target_valid(tp); tp = tp->next) {

	if (! tp->endpoints) {
            printf("DNS.0.4;%lu;%s;%s;%s\n",
                   now, "FAIL", tp->host, tp->port);
	}

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

	    printf("DNS.0.4;%lu;%s;%s;%s;%s;%s",
		   now, ep->cnt ? "OK" : "FAIL", tp->host, host,
		   ep->canonname ? ep->canonname : "",
		   ep->reversename ? ep->reversename : "");
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
	    if (ep->canonname) {
		(void) free(ep->canonname);
	    }
	    if (ep->reversename) {
		(void) free(ep->reversename);
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

    /* ignore SIGPIPE, handle locally the returned EPIPE error */
    signal(SIGPIPE, SIG_IGN);

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
                ssize_t sent = 0;
                ssize_t received = 0;
                rc = select(1 + ep->socket, &rfds, &wfds, NULL, NULL);
                if (rc == -1) {
                    fprintf(stderr, "%s: select failed: %s\n",
                            progname, strerror(errno));
                    exit(EXIT_FAILURE);
                }

                if (FD_ISSET(ep->socket, &rfds)) {
                    received = recv(ep->socket, buffer, sizeof(buffer), 0);
                    if(received<0) {
                        fprintf(stderr, "recverr (%s): %s\n", tp->host, strerror(errno));
                        if (errno == EPIPE) break;
                    } else {
                        ep->rcvd += received;
                    }
                }

                if (FD_ISSET(ep->socket, &wfds)) {
                    sent = send(ep->socket, msg, strlen(msg), 0);
                    if(sent<0) {
                        fprintf(stderr, "senderr (%s): %s\n", tp->host, strerror(errno));
                        if (errno == EPIPE) break;
                    } else {
                        ep->send += sent;
                    }
                }

                (void) gettimeofday(&tn, NULL);
                timersub(&tn, &ts, &td);
                us = td.tv_sec*1000000 + td.tv_usec;
            }

            if (ep->socket) {
                (void) close(ep->socket);
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

    while ((c = getopt(argc, argv, "abcd:p:q:f:hmst:")) != -1) {
	switch (c) {
	case 'a':
	    dmode = 1;
	    break;
	case 'b':
	    pmode = 1;
	    break;
	case 'c':
	    cmode = 1;
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
		    "Usage: %s [-a] [-b] [-c] [-p port] [-q nqueries] "
		    "[-t timeout] [-d delay ] [-f file] [-s] [-m] "
		    "hostname...\n", progname);
	    exit(EXIT_FAILURE);
	}
    }
    argc -= optind;
    argv += optind;

    if (! cmode && ! pmode && ! dmode) {
	cmode = 1;
    }

    for (i = 0; i < argc; i++) {
        for (j = 0; ports[j]; j++) {
            append(expand(argv[i], ports[j]));
        }
    }

    if (targets) {
	if (smode || pmode) {
	    for (i = 0; i < nqueries; i++) {
		prepare(targets);
		collect(targets);
	    }
	}
	if (smode) {
	    sort(targets);
	}
	if (pmode) {
	    pump(targets);
	}
	lock(stdout);
	if (dmode) {
	    if (skmode) {
		report_dns_sk(targets);
	    } else {
		report_dns(targets);
	    }
	}
	if (cmode) {
	    if (skmode) {
		report_sk(targets);
	    } else {
		if (dmode) {
		    printf("\n");
		}
		report(targets);
	    }
	}
	if (pmode) {
	    if (skmode) {
		report_pump_sk(targets);
	    } else {
		if (cmode) {
		    printf("\n");
		}
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
