`happy` is a simple TCP happy eyeballs probing tool. It uses
non‐blocking `connect()`  calls  to  establish connections concurrently
to a number of possible endpoints. This  tool  is  particularly  useful
to  determine whether  happy  eyeball [RFC 6555] applications will use
IPv4 or IPv6 if both are available.

The build environment uses cmake and hence the code should be fairly
portable. However, patches that improve portability of fix bugs are
always welcome.

Build and Usage:
----------------

    $ mkdir builddir
    $ cd builddir
    $ cmake ..
    $ make
    $ make install

    $ happy -ac www.ietf.org www.facebook.com www.bing.com
    www.ietf.org:80
     www.ietf.org.cdn.cloudflare.net > 2400:cb00:2048:1::6814:155
     www.ietf.org.cdn.cloudflare.net > 2400:cb00:2048:1::6814:55
     www.ietf.org.cdn.cloudflare.net > 104.20.1.85
     www.ietf.org.cdn.cloudflare.net > 104.20.0.85

    www.facebook.com:80
     star.c10r.facebook.com > 2a03:2880:f01c:d01:face:b00c::1 >
    edge-star6-shv-14-fra3.facebook.com
     star.c10r.facebook.com > 31.13.93.241 >
    edge-star-shv-14-fra3.facebook.com

    www.bing.com:80
     any.edge.bing.com > 204.79.197.200 > a-0001.a-msedge.net

    www.ietf.org:80
     2400:cb00:2048:1::6814:155                   8.297    8.411    8.759
     2400:cb00:2048:1::6814:55                    8.573    8.767    8.573
     104.20.1.85                                 12.824   14.677   14.584
     104.20.0.85                                 12.794   14.573   12.746

    www.facebook.com:80
     2a03:2880:f01c:d01:face:b00c::1              8.678    8.471    8.902
     31.13.93.241                                 9.268    8.441    8.285

    www.bing.com:80
     204.79.197.200                              24.434   26.091   26.309


Options:
--------

    % happy -h
    Usage: happy [-a] [-b] [-c] [-p port] [-q nqueries] [-t timeout] [-d
    delay ] [-f file] [-s] [-m] hostname...


The description of each option is available in the man page:

    $ man happy

Limitations:
-----------

The  tool  does  not  check whether the endpoints of a given target all
provide the same service. Hence, it is possible to impact  the results
by setting up ’fake’ servers that do not provide the service tested and
which are designed and deployed with the only purpose to provide  fast
connection setup times.

Author:
-------

- Juergen Schoenwaelder <j.schoenwaelder AT jacobs‐university.de>
- Vaibhav Bajpai <v.bajpai AT jacobs‐university.de>

License:
--------

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

