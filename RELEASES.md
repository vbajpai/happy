v0.1

- updated cmake to roperly handle installation of man pages
- added option -b to measure the speed in which HTTP requests are responded to.
- updated the man page with a description of the -b option. 

v0.0

- uses getaddrinfo(...) to resolve service names to endpoints.
- uses non-blocking connect(...) to connect to all endpoints of a service.
- uses a short delay between connection attempts to avoid SYN floods (configurable).
- the service name resolution time is not accounted in the output.
- capability to read the input service names list from a file.
- file locking capability when stdout points to a regular file.
- can produce either human-readable or machine-readable output.
- can sort the output for all endpoints for a service name.
- connection timeouts have status OK and measured times are on the negative scale.
- small negative timeouts indicate a pending error on the socket (SO_ERROR <> 0).
- status is FAIL when happy runs out of socket descriptors.
