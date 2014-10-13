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
    $ ./happy ‐s www.google.com www.bing.com www.yahoo.com
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

    www.yahoo.com:80
     2a00:1288:f006:1fe::3000                    16.270   17.734   16.828
     2a00:1288:f006:1fe::3001                    16.915   18.187   16.778
     2a00:1288:f00e:1fe::3000                    23.652   24.379   23.620
     2a00:1288:f00e:1fe::3001                    24.201   23.568   23.079
     87.248.122.122                              29.487   24.177   29.546
     87.248.112.181                              42.895   48.629   42.784

Options:
--------

    $ bin/happy -h
    Usage: happy [-p port] [-q nqueries] [-t timeout] [-d delay ] \
                 [-f file] [-s] [-m] hostname

The description of each option is available in the man page:

    $ nroff -man happy.1 | less

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

License:
--------

    Copyright (c) 2013-2014, Juergen Schoenwaelder, Jacobs University Bremen
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

