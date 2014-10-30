# happy
- - - -

`happy` is a simple TCP happy eyeballs probing tool. It uses
non‐blocking `connect(...)` calls to concurrently establish connections
to a number of possible endpoints. This tool is particularly useful to
determine whether applications implementing the happy eyeball algorithm
[RFC 6555] will use IPv4 or IPv6 if both are available.

![](http://i.imgur.com/WeGzIZ7.png) 
<br/>

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
    $ make install
    $ happy -ac www.ietf.org www.bing.com
    www.ietf.org:80
     www.ietf.org.cdn.cloudflare.net > 2400:cb00:2048:1::6814:155
     www.ietf.org.cdn.cloudflare.net > 2400:cb00:2048:1::6814:55
     www.ietf.org.cdn.cloudflare.net > 104.20.1.85
     www.ietf.org.cdn.cloudflare.net > 104.20.0.85

    www.bing.com:80
     any.edge.bing.com > 204.79.197.200 > a-0001.a-msedge.net

    www.ietf.org:80
     2400:cb00:2048:1::6814:155                   8.297    8.411    8.759
     2400:cb00:2048:1::6814:55                    8.573    8.767    8.573
     104.20.1.85                                 12.824   14.677   14.584
     104.20.0.85                                 12.794   14.573   12.746

    www.bing.com:80
     204.79.197.200                              24.434   26.091   26.309

### Releases

You can either fork the bleeding edge development version or download a
release from [here &rarr;](https://github.com/vbajpai/happy/releases)

### Measurement Trials

We are currently running `happy` from a small sample of vantage points
distributed all around the globe. In order to participate in this
experiment, send us an email and we will ship you a SamKnows probe that
in addition to the standard SamKnows tests will also perform the `happy`
test from your vantage point.

![](http://i.imgur.com/WEisgJg.jpg)
<br/>

### Publications

[Measuring TCP Connection Establishment Times of Dual-Stacked Web
Services &rarr;](http://vaibhavbajpai.com/documents/papers/proceedings/dualstack-tcp-cnsm-2013.pdf)  
Vaibhav Bajpai and Jürgen Schönwälder  
9th International Conference on Network and Service Management (CNSM 2013)  
Zurich, October 2013  

[Measuring the Effects of Happy Eyeballs
&rarr;](http://tools.ietf.org/html/draft-bajpai-happy-01)  
Vaibhav Bajpai and Jürgen Schönwälder  
Internet Draft (I-D), July 2013

[Evaluating the Effectiveness of Happy Eyeballs
&rarr;](https://labs.ripe.net/Members/vaibhav_bajpai/evaluating-the-effectiveness-of-happy-eyeballs)  
Vaibhav Bajpai and Jürgen Schönwälder  
RIPE Labs, June 2013

### Talks

[Measuring TCP Connection Establishment Times of Dual-Stacked Web
Services
&rarr;](http://www.ietf.org/proceedings/interim/2013/10/14/nmrg/slides/slides-interim-2013-nmrg-1-10.pdf)  
Vaibhav Bajpai and Jürgen Schönwälder  
IRTF NMRG Workshop on Large-Scale Measurements, Zürich, October 2013  

[Measuring the Effects of Happy Eyeballs
&rarr;](http://www.ietf.org/proceedings/87/slides/slides-87-v6ops-8.pdf)  
Vaibhav Bajpai and Jürgen Schönwälder  
IETF 87, Berlin, July 2013

<iframe src="//player.vimeo.com/video/71407427" width="500" height="376"
frameborder="0" webkitallowfullscreen mozallowfullscreen
allowfullscreen></iframe>

<br/>

[Measuring the Effectiveness of Happy Eyeballs
&rarr;](https://ripe66.ripe.net/archives/video/1208)  
Vaibhav Bajpai and Jürgen Schönwälder  
RIPE 66, Dublin, May 2013

<iframe src="//player.vimeo.com/video/83236147" width="500" height="375"
frameborder="0" webkitallowfullscreen mozallowfullscreen
allowfullscreen></iframe>

<br/>

### Authors

- Jürgen Schönwälder [j.schoenwaelder@jacobs-university.de](j.schoenwaelder@jacobs-university.de)
- Vaibhav Bajpai [v.bajpai@jacobs-university.de](v.bajpai@jacobs-university.de)

### License
<pre>
Copyright (c) 2013-2014, Juergen Schoenwaelder, Jacobs University Bremen
Copyright (c) 2014, Vaibhav Bajpai, Jacobs University Bremen
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
