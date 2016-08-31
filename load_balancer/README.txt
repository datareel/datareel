DataReel Load Balancer (DRLB) 1.X Readme File
DataReel Copyright (c) 2016 DataReel Software Development

https://github.com/datareel/datareel/tree/master/load_balancer

Contents:
--------
* Overview
* Features
* Documented Usage
* Building
* Installing
* Building a default configuration file
* Test DRLB configurations files
* Running DRLB as a service
* Setting the listening port
* Round robin configuration 
* Weighted configuration
* Distributed configuration
* Assigned configuration
* Throttling configuration
* Log configuration 
* Statistics collection
* Setting connection and thread limits
* Setting buffer and caching options
* Dead node retry settings
* Buffer overflow settings
* Advanced socket settings
* DRLB process monitoring
* Linux Kernel Tuning
* Testing with Apache
* Remaining work on this project
* Support and Bug Tracking

Overview:
--------
The DataReel load balancer (LB) is a software based TCP load balancing
solution designed to run on Linux operating systems. The DRLB project
was designed to handle any type of TCP connection, acting as a front
end server distributing high volumes of TCP connections to multiple
back end servers. Load balancing protects your back end servers from
saturation when you have more clients connection than any one server
can handle. The DRLB server can be used as a high availability
resources to provide seamless server fail over. In signal server
environments the DRLB server can provide connection limits and cached
buffing.   

The DRLB source code is distributed as an open source project under
the GNU general public license and can used by freely by commercial,
Government, and academic institutions.   

Features:
--------
* Round robin: Distribute connections equally to server nodes in order
  received. 

* Weighted: Distribute connections based on weight value assigned to
  each server node.

* Distributed: Distribute connections based on a load percentage
  assigned to each server node.

* Assigned: Assign connections to specific server nodes with the option
  to use round robin, weighted, or distributed load balancing for fail
  over and unassigned connections.

* Connection throttling: Prevent server saturation by throttling
  connections based on preset connections values or number of connection
  per second.

* Statistics: Option to log connection information and monitor
  connections per second in real time.

* Connection limits: Option to set connections per server node or all
  server nodes.

* Caching: Buffer server connections with option to set custom buffer
  sizes.

* Buffer overflow detection: Option to detect buffer overflows for
  specific applications.  

Documented Usage:
----------------
* All commands prefixed with the $ prompt indicate the command is
  executed from a user account or from a service account. 

* All commands prefixed with the # prompt indicate the command is
  execute as root. 

* In a configuration file example a ... symbol indicates omitted content
  for brevity.   

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

Building a default configuration file:
-------------------------------------
The DRLB server is deigned as generic load balancer and can handle
many LB configurations. To build an LB configuration start with a
generic template. To build a configuration template, run the following
command: 

# /usr/sbin/drlb_server --check-config --config-file=/etc/drlb/http_lb.cfg

Running DRLB configuration check and exiting
Loading or building config file http_lb.cfg
DRLB configuration file not found. Building default file and exiting

Test DRLB configurations files:
------------------------------
After editing a DRLB configuration file you must check your
configuration file for errors before starting the DRLB server:

# /usr/sbin/drlb_server --check-config --config-file=/etc/drlb/http_lb.cfg | grep -i -E 'error|warn|fail|info|notice'

If you have any errors or warnings, review and edit your configuration file:

# /usr/sbin/drlb_server --check-config --config-file=/etc/drlb/http_lb.cfg

Running DRLB as a service:
-------------------------
DRLB is designed to run multiple load balancer configurations on
single Linux server. When the drlb_server service is started, the
service initialization script checks the following file for active
server configurations:

/etc/drlb/active_configs.list

To add configurations, edit the active configs list:

# vi /etc/drlb/active_configs.list

# List of active DRLB configurations
# Below is an example config file. Remove line below after testing.

/etc/drlb/http_rr_test.cfg
/etc/drlb/ldm_cluster.cfg

# List all the LB configs you need to start on this server.
# All entries must use an absolute path to the .cfg file.

Save your changes and exit vi.

If you want to use a single configuration or override the DRLB service 
defaults, edit the following file:

# vi /etc/sysconfig/drlb_server

# File used to override the init script VARs

# Set to change the name of the server binary
##LBPROCNAME="drlb_server"

# Set to change the path to the server binary
##LBPROC="/usr/sbin/${LBPROCNAME}"

# Set to change location of active config list
##ACTIVE_CONFIGS="/etc/drlb/active_configs.list"

# NOTE: Set for single config if you wish to run only on LB server
###SINGLECONFIG=/etc/drlb/http_rr_test.cfg

After setting up your DRLB configuration files and your active
configuration list, start the DRCM
server and make the DRCM service persistent:

RHEL 7/CENTOS 7:

# systemctl start drlb_server
# systemctl status drlb_server
# systemctl enable drcm_server

RHEL 6/CENTOS 6:

# service drlb_server start
# service drlb_server status
# chkconfig drcm_server on

Setting the listening port:
--------------------------
Author to complete this section, in progress 08/30/2016

# vi /etc/drlb/http_lb.cfg

[LBSERVER]
port = 8080 # All incoming traffic connections here 
...

[LBNODE]
node_name = lbnode1
port_number = 80 # Port backend host is listening on
...

[LBNODE]
node_name = lbnode2
port_number = 80
...

[LBNODE]
node_name = lbnode3
port_number = 80
...

[LBNODE]
node_name = lbnode4
port_number = 80
...


Round robin configuration: 
-------------------------
Author to complete this section, in progress 08/30/2016

[LBSERVER]
scheme = LB_RR

[LBNODE]
node_name = lbnode1
port_number = 80
hostname = 192.168.122.111

[LBNODE]
node_name = lbnode2
port_number = 80
hostname = 192.168.122.112

[LBNODE]
node_name = lbnode3
port_number = 80
hostname = 192.168.122.113

[LBNODE]
node_name = lbnode4
port_number = 80
hostname = 192.168.122.114

Weighted configuration:
----------------------
Author to complete this section, in progress 08/30/2016

[LBSERVER]
scheme = LB_WEIGHTED

[LBNODE]
node_name = lbnode1
port_number = 80
hostname = 192.168.122.111
weight = 1

[LBNODE]
node_name = lbnode2
port_number = 80
hostname = 192.168.122.112
weight = 1

[LBNODE]
node_name = lbnode3
port_number = 80
hostname = 192.168.122.113
weight = 2

[LBNODE]
node_name = lbnode4
port_number = 80
hostname = 192.168.122.114
weight = 3

Distributed configuration:
-------------------------
Author to complete this section, in progress 08/30/2016

[LBSERVER]
scheme = LB_DISTRIB

[LBNODE]
node_name = lbnode1
port_number = 80
hostname = 192.168.122.111
weight = 15%

[LBNODE]
node_name = lbnode2
port_number = 80
hostname = 192.168.122.112
weight = 15%

[LBNODE]
node_name = lbnode3
port_number = 80
hostname = 192.168.122.113
weight = %30

[LBNODE]
node_name = lbnode4
port_number = 80
hostname = 192.168.122.114
weight = 40%

Assigned configuration:
----------------------
Author to complete this section, in progress 08/30/2016

assigned_default can be set to:

LB_RR
LB_DISTRIB
LB_WEIGHTED
LB_NONE

[LBSERVER]
scheme = LB_ASSIGNED
rules_config_file = /etc/drlb/http_lb_rules.cfg
assigned_default = LB_RR

[LBNODE]
node_name = lbnode1
port_number = 80
hostname = 192.168.122.111

[LBNODE]
node_name = lbnode2
port_number = 80
hostname = 192.168.122.112

[LBNODE]
node_name = lbnode3
port_number = 80
hostname = 192.168.122.113

[LBNODE]
node_name = lbnode4
port_number = 80
hostname = 192.168.122.114

Throttling configuration:
------------------------
Author to complete this section, in progress 08/30/2016

# Enable throttling                                                                                                       
##enable_throttling = 0                                                                                                   
# Apply throttle control only when load reached a certain number of
#connection per second                                 
# If apply by load is set to 0, we will throttle based on the
#connection count                                            
##throttle_apply_by_load = 0                                                                                              
# Set the number of connection per second to start throttling
#connections                                                 
##throttle_connections_per_sec = 10                                                                                       
# Set the number of connections to start throttling                                                                       
# A setting of 1 will throttle every connection                                                                           
##throttle_every_connections = 10                                                                                         
# Set the time in seconds or microseconds to hold a connection in the
#throttle queue                                      
##throttle_wait_secs = 1                                                                                                  
##throttle_wait_usecs = 0      

Log configuration: 
-----------------
Author to complete this section, in progress 08/30/2016

# Log settings
# Keep log file plus last 3. Disk space will be: (max_log_size * (num_logs_to_keep +1))
num_logs_to_keep = 3
# Set max size per log file, max is 2000000000
max_log_size = 5000000 # Default is 5M, max is 2G
log_queue_size = 2048 # Max number of log or console messages to queue
log_queue_debug_size = 4096 # Max number of debug messages to queue
log_queue_proc_size = 255 # Max number of process messages to queue
# Values below can be set here or args when program is launched
##clear_log = 0
##enable_logging = 0
# Log levels 0-5, 0 lowest log level, 5 highest log level 
##log_level = 0
##logfile_name = /var/log/drlb/drlb_server.log

Statistics collection:
---------------------
Author to complete this section, in progress 08/30/2016

# Stats settings
# Keep stats file plus last 3. Disk space will be: (max_stats_size * (num_stats_to_keep +1))
num_stats_to_keep = 3
# Set max size per stats file, max is 2000000000
max_stats_size = 5000000 # Default is 5M, max is 2G
stats_min = 5 # Generate a stats report every 5 minutes
stats_secs = 0 # If min=0 gen a stats report less than every min
##enable_stats = 0
##stats_file_name = /var/log/drlb/drlb_stats.log

Setting connection and thread limits:
------------------------------------
Author to complete this section, in progress 08/30/2016

[LBSERVER]
max_connections = -1 # Not set, if set will limit number of connections this server
max_threads = -1 # Not set, if set will limit number of threads for this process

[LBNODE]
node_name = lbnode1
max_connections = -1 # Not set, if set will limit number of connection to this node

[LBNODE]
node_name = lbnode2
max_connections = -1 # Not set, if set will limit number of connection to this node

[LBNODE]
node_name = lbnode3
max_connections = -1 # Not set, if set will limit number of connection to this node

[LBNODE]
node_name = lbnode4
max_connections = -1 # Not set, if set will limit number of connection to this node

Setting buffer and caching options:
----------------------------------
Author to complete this section, in progress 08/30/2016

[LBSERVER]
use_buffer_cache = 1
buffer_size = 1500

[LBNODE]
node_name = lbnode1
buffer_size = -1 # Use global size

[LBNODE]
node_name = lbnode2
buffer_size = -1 # Use global size

[LBNODE]
node_name = lbnode3
buffer_size = -1 # Use global size

[LBNODE]
node_name = lbnode4
buffer_size = -1 # Use global size

Dead node retry settings:
------------------------
Author to complete this section, in progress 08/30/2016

[LBSERVER]
retries = 3
retry_wait = 1

Buffer overflow settings:
------------------------
Author to complete this section, in progress 08/30/2016

# Buffer overflow detection with using cached reads, disabled by default
enable_buffer_overflow_detection = 0
buffer_overflow_size = 6400000

Advanced socket settings:
------------------------
Author to complete this section, in progress 08/30/2016

# Set max number of back logged connections
# Should match: cat /proc/sys/net/core/somaxconn
somaxconn = 128

# Idle time for blocking sockets
max_idle_count = 60000
idle_wait_secs = 0
idle_wait_usecs = 500

# Use non-blocking sockets
use_nonblock_sockets = 0

# Use timeout on blocking reads
use_buffer_cache = 0
use_timeout = 0
timeout_secs = 300
timeout_usecs = 0

DRLB process monitoring:
-----------------------
Author to complete this section, in progress 08/30/2016

Watch percentage of memory:

$ watch -n 1 'ps -eo %mem,pid,user,args | grep drlb | grep -v grep'

Watch percentage of CPU:

$ watch -n 1 'ps -eo %cpu,pid,user,args | grep drlb | grep -v grep'

Watch percentage of CPU and memory:

$ watch -n 1 'ps -eo %cpu,%mem,pid,etime,args | grep drlb | grep -v grep'

Watch memory usage:

$ pids=$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')
$ for p in $pids; do cat /proc/$p/status | grep -i vmsize; done

Watch number of threads:

$ pids=$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')
$ for p in $pids; do cat /proc/$p/status | grep -i threads; done

Watch number of open files:

$ pids=$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')
$ for p in $pids; do ls -l --color=none /proc/$p/fd; done

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

* RPM install for CENTOS/RHEL 6.X and 7.X
* HTML Documentation

Support and Bug Tracking:
------------------------

https://github.com/datareel/datareel/issues

