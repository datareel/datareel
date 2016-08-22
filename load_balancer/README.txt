DataReel Load Balancer (DRLB) 1.X Readme File
DataReel Copyright (c) 2016 DataReel Software Development

https://github.com/datareel/datareel/tree/master/load_balancer

Contents:
--------
* Overview
* Why Another Load Balancer
* Building
* Installing
* Linux Kernel Tuning
* Testing with Apache
* Remaining work on this project
* Support and Bug Tracking

Overview:
--------
The DataReel load balancer (LB) is a software based TCP load balancing
solution designed to run on Linux operating systems. The DRLB project
was designed to handle a large number of persistent TCP connections,
specifically for the unidata Local Data Manager (LDM) protocol. The
DRLB source code is distributed as an open source project under the
GNU general public license. DRLB is an adaptive load balancing
environment where the source code can be modified to meet the needs of
your server and network environment. 

Why Another Load Balancer:
-------------------------
Most of the readily available load balancing packages are designed for
HTTP traffic, serving Web pages. DRLB was initially designed for
persistent TCP connections where the client/server connections must
remain connected and transfer bi-directional data of varying
size. HTTP connections frequently connect and disconnect and work well
with round robin and weighted round robin load balancing. In the case
of persistent connections the load balancer must be able to
dynamically assign  nodes to specific clients and distribute traffic
based on the percent of persistent connections. DRLB allows systems
administrators to dynamically route IP addresses to specific backend
nodes, with the ability to automatically fail over to other nodes. The
second design phase of DRLB was targeted at HTTP servers used as Web
page and data servers. Many data sources on FTP servers were converted
to HTTP or HTTPS downloads. In cases where existing HTTP servers were
used due to limited public IP address space, many contention issues
with server resources resulted. Web page users experienced massive
slow downs rendering Web pages while competing with data downloads on
the same server. Using DRLB in distributed/assigned mode allows the
data downloads to be isolated from the Web pages, still using only one
public Web server IP address.

Building:
--------
Developers, follow the instructions below to build the DRLB server:

$ cd $HOME
$ mkdir -pv git
$ cd $HOME/git
$ git clone https://github.com/datareel/datareel
$ source $HOME/git/datareel/env/gnu_env.sh
$ cd $HOME/git/datareel/load_balancer/drlb_server
$ make

Build a default configuration to test with:

$ ./drlb_server --verbose --debug --config-file=/tmp/lb_test_config.cfg

The default configuration file is set up for 4 back end nodes running
Apache and the frontend LB listening on port 8080. NOTE: For testing
port 8080 is used so the LB server can be started without root
privileges. The default configuration file is documented with
comments. As set of example config files in the etc directory: 

$ ls -l $HOME/git/datareel/load_balancer/etc

Once your configuration is set up, run a test with debug and logging enabled:

$ ./drlb_server --verbose --debug --config-file=/tmp/lb_test_config.cfg --log-file=/tmp/lb_test.log

Installing:
----------
The development version can be installed on CENTOS6/RHEL6 or
CENTOS7/RHEL7 from the makefile:

$ cd $HOME/git/datareel/load_balancer/drlb_server
$ sudo su -c 'make install_root'

Linux Kernel Tuning:
-------------------
Developers  and system administrators see the following document:

$ less $HOME/git/datareel/load_balancer/docs/linux_kernel_tuning.txt

Load balancing will require kernel tuning based the volume of traffic
and type of TCP service you are load balancing. NOTE: The values in
the "linux_kernel_tuning.txt" are based on extremely high
volume. Depending on your application you may not need to set many of
the parameters listed. But please keep in mind that you will need to
tune the kernel for any server acting as a load balancer.

Testing with Apache:
-------------------
A simple way to test your DRLB server to use Apache on KVM virtual
machines. Once you have Apache running on a one or more virtual
machines copy the following PHP scripts to the Web root on each VM,
for example 

$ cd $HOME/git/datareel/load_balancer/apache_test
$ chmod 644 *.php

$ for s in 192.168.122.111 192.168.122.112 192.168.122.113 192.168.122.114 
> do
> scp -p *.php root@${s}:/var/www/html/.
> done

$ cd $HOME/git/datareel/load_balancer/drlb_server
$ ./drlb_server --config-file=../etc/http_rr_test.cfg --log-file=/tmp/http_rr.log > /tmp/drlb_start.log &

To connect using wget, assuming the DRLB server is listening on port
8080 on the bare metal host:

$ cd /tmp
$ wget -O index.html http://192.168.122.1:8080

Persistent TCP connection:

$ wget -O loop1.txt http://192.168.122.1:8080

NOTE: If you testing using Firefox, you will be limited to 6
persistent session. By default Firefox only allow 6 simultaneous
downloads.

Remaining work on this project:
------------------------------ 
The first DRLB release was successfully tested at National Weather
Service (NWS), Southern Region Headquarters (SRH). The implementation
phase at NWS/SRH include LDM and HTTP services using the DRLB server
to handle a high volume of TCP traffic. During the implementation
phase all source code changes and bug fixes will be release in this
open source distribution.  

Remaining work to be added to this open source project:

* RPM install for CENTOS/RHEL 6.X and 7.X
* HTML Documentation

Support and Bug Tracking:
------------------------

https://github.com/datareel/datareel/issues

