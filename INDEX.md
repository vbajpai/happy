# happy
- - - -

`happy` is a simple TCP happy eyeballs probing tool. It uses non‐blocking
`connect(...)` calls to establish connections concurrently to a number of
possible endpoints. This tool is particularly useful to determine
whether happy eyeball [RFC 6555] applications will use IPv4 or IPv6 if
both are available.

### Features

- uses `getaddrinfo(...)` to resolve service names to endpoints.
- uses non-blocking `connect(...)` to connect to all endpoints of a service.
- uses a short delay between connection attempts to avoid `SYN` floods (configurable)
- the service name resolution time is not accounted in the output.
- capability to read the input service names list from a file.
- file locking capability when `stdout` points to a regular file.
- can produce either human-readable or machine-readable output.
- sort the output for all endpoints for a service name.

### Installation and Usage

The build environment uses `cmake` and hence the code should be fairly
portable. However, patches that improve portability of fix bugs are
always welcome.


### Invited Talks

[Measuring the Effectiveness of Happy Eyeballs &rarr;](https://ripe66.ripe.net/archives/video/1208)  
Vaibhav Bajpai and Jürgen Schönwälder  
RIPE 66, Dublin, May 2013

### Authors

- Jürgen Schönwälder [j.schoenwaelder@jacobs-university.de](j.schoenwaelder@jacobs-university.de)

### License
<pre>

Copyright (c) 2013, Juergen Schoenwaelder, Jacobs University Bremen
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and
documentation are those of the authors and should not be
interpreted as representing official policies, either expressed or
implied, of the Leone Project or Jacobs University Bremen.

</pre>
