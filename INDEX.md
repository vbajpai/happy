# happy
- - - -

`happy` is a simple TCP happy eyeballs probing tool. It uses
non‐blocking `connect(...)` calls to concurrently establish connections
to a number of possible endpoints. This tool is particularly useful to
determine whether applications implementing the happy eyeball algorithm
[RFC 6555] will use IPv4 or IPv6 if both are available.

![](http://i.imgur.com/WeGzIZ7.png)

### Features

- uses `getaddrinfo(...)` to resolve service names to endpoints.
- uses non-blocking `connect(...)` to connect to all endpoints of a service.
- uses a short delay between connection attempts to avoid `SYN` floods (configurable)
- the service name resolution time is not accounted in the output.
- capability to read the input service names list from a file.
- file locking capability when `stdout` points to a regular file.
- can produce either human-readable or machine-readable output.
- can sort the output for all endpoints for a service name.

### Installation and Usage

The build environment uses `cmake`. An accompanying man page describes
all the options available in detail.
 
    $ make
    $ bin/happy ‐s www.google.com www.bing.com 
    www.google.com:80
     2a00:1450:4008:c01::69                       9.255   10.124    9.502
     173.194.69.147                               8.918    9.493    9.253
     173.194.69.99                                9.879    9.139   10.099
     173.194.69.104                               9.544    9.503    9.802
     173.194.69.105                               9.245    8.989   10.108
     173.194.69.106                              10.111    9.533    9.798
     173.194.69.103                               9.514    9.553    8.882

    www.bing.com:80
     2001:638:a:2::d4c9:6487                      4.010    3.409    4.019
     2001:638:a:2::d4c9:6490                      3.093    3.706    3.756
     212.201.100.135                              3.400    3.397    4.004
     212.201.100.144                              2.785    3.401    4.324

### Publications 

[Measuring TCP Connection Establishment Times of Dual-Stacked Web
Services &rarr;](http://vaibhavbajpai.com/documents/papers/proceedings/dualstack-tcp-cnsm-2013.pdf)  
Vaibhav Bajpai and Jürgen Schönwälder  
9th International Conference on Network and Service Management (CNSM 2013)  
Zurich, October 2013  

### Technical Articles

[Evaluating the Effectiveness of Happy Eyeballs
&rarr;](https://labs.ripe.net/Members/vaibhav_bajpai/evaluating-the-effectiveness-of-happy-eyeballs)  
Vaibhav Bajpai and Jürgen Schönwälder  
RIPE Labs, June 2013

### Internet Drafts

[Measuring the Effects of Happy Eyeballs
&rarr;](http://tools.ietf.org/html/draft-bajpai-happy-01)  
Vaibhav Bajpai and Jürgen Schönwälder  
Internet Draft (I-D), July 2013

### Invited Talks

[Measuring TCP Connection Establishment Times of Dual-Stacked Web
Services
&rarr;](http://www.ietf.org/proceedings/interim/2013/10/14/nmrg/slides/slides-interim-2013-nmrg-1-10.pdf)  
Vaibhav Bajpai and Jürgen Schönwälder  
IRTF NMRG Workshop on Large-Scale Measurements, Zürich, October 2013  

[Measuring the Effects of Happy Eyeballs
&rarr;](http://www.ietf.org/proceedings/87/slides/slides-87-v6ops-8.pdf)  
[Video Recording &rarr;](https://vimeo.com/71407427)  
Vaibhav Bajpai and Jürgen Schönwälder  
IETF 87, Berlin, July 2013

[Measuring the Effectiveness of Happy Eyeballs
&rarr;](https://ripe66.ripe.net/archives/video/1208)  
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
