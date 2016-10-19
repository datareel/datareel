DataReel Load Balancer (DRLB) 1.X Readme File
DataReel Copyright (c) 2016 DataReel Software Development

https://github.com/datareel/datareel/tree/master/load_balancer

Contents:
--------
* Overview
* Features
* Documented Usage
* Building
* RPM Install
* Installing from src build
* Building a default configuration file
* Testing DRLB configurations files
* Running DRLB as a service
* Configuration Changes
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
* Setting Open File Limits
* Linux Kernel Tuning
* Testing with Apache
* HTTP IP address forwarding
* Remaining work on this project
* Support and Bug Tracking

Overview
--------
The DataReel load balancer (LB) is a software based TCP load balancing
solution designed to run on Linux operating systems. The DRLB project
was designed to handle any type of TCP connection, acting as a front
end server distributing high volumes of TCP connections to multiple
back end servers. Load balancing protects your back end servers from
saturation when you have more clients connections than any one server
can handle. The DRLB server can be used as a high availability
resource to provide seamless server fail over. In signal server
environments the DRLB server can provide connection limits and cached
buffing.

The DRLB source code is distributed as an open source project under
the GNU general public license and can used by freely by commercial,
Government, and academic institutions.

Fetures
-------
* Round robin: Distribute connections equally to server nodes in
  order received.

* Weighted: Distribute connections based on a weight value assigned
  to each server node.

* Distributed: Distribute connections based on a load percentage
  assigned to each server node.

* Assigned: Assign connections to specific server nodes with the
  option to use round robin, weighted, or distributed load balancing
  for fail over and unassigned connections.

* Connection throttling: Prevent server saturation by throttling
  connections based on preset connections values or number of
  connection per second.

* Statistics: Option to log connection information and monitor
  connections per second in real time.

* Connection limits: Option to set connections per server node or
  all server nodes.

* Caching: Buffer server connections with option to set custom
  buffer sizes.

* Buffer overflow detection: Option to detect buffer overflows for
  specific applications.

Documented Usage
----------------
* All commands prefixed with the $ prompt indicate the command is
  executed from a user account or from a service account.

* All commands prefixed with the # prompt indicate the command is
  executed as root.

* In a configuration file example a ... symbol indicates omitted
  content for brevity.

Building
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
comments. A set of example config files in the etc directory:

$ ls -l $HOME/git/datareel/load_balancer/etc

Once your configuration is set up, run a test with debug and logging
enabled:

$ ./drlb_server --verbose --debug --config-file=/tmp/lb_test_config.cfg --log-file=/tmp/lb_test.log

RPM Install
-----------
System administrators, follow the instructions below to build an RPM
for distribution:

$ cd $HOME; mkdir -pv git; cd $HOME/git
$ git clone https://github.com/datareel/datareel
$ cd ${HOME}/git/datareel/load_balancer/rpm_builder

RHEL7/CENTOS7:
$ ./make_rhel7_rpm.sh
$ sudo su root -c "yum -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_server-1.58-1.el7.x86_64.x86_64.rpm"

RHEL6/CENTOS6:
$ ./make_rhel6_rpm.sh
sudo su root -c "yum -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_server-1.58-1.el6.x86_64.x86_64.rpm"

Useful RPM command to verify package contents:

List package: rpm -qf /usr/sbin/drlb_server
List files: rpm -ql drlb_server
List configs: rpm -qc drlb_server
List docs: rpm -qd drlb_server

To remove package:

$ sudo su root -c "yum -y remove drlb_server"

Installing from src build
-------------------------
The development version can be installed on CENTOS6/RHEL6 or
CENTOS7/RHEL7 from the makefile:

$ cd $HOME/git/datareel/load_balancer/drlb_server
$ sudo su root -c 'make install_root'

Building a default configuration file
-------------------------------------
The DRLB server is deigned as generic load balancer and can handle
many LB configurations. To build a configuration template, run the
following command:

# /usr/sbin/drlb_server --check-config --config-file=/etc/drlb/http_lb.cfg

Running DRLB configuration check and exiting
Loading or building config file http_lb.cfg
DRLB configuration file not found. Building default file and exiting

Testing DRLB configurations files
---------------------------------
After editing a DRLB configuration file you must check your
configuration file for errors before starting or restarting the DRLB
server:

# /usr/sbin/drlb_server --check-config --config-file=/etc/drlb/http_lb.cfg | grep -i -E 'error|warn|fail|info|notice'

If you have any errors or warnings, review and edit your configuration
file:

# /usr/sbin/drlb_server --check-config --config-file=/etc/drlb/http_lb.cfg

Running DRLB as a service
-------------------------
DRLB is designed to run multiple load balancer configurations on
single Linux server. When the drlb_server service is started, the
service initialization script checks the following file for active
server configurations:

/etc/drlb/active_configs.list

To add configurations, edit the active configs list:

# vi /etc/drlb/active_configs.list

# List of active DRLB configurations

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
configuration list, start the DRLB server and make the DRLB service
persistent:

RHEL 7/CENTOS 7:

# systemctl start drlb_server
# systemctl status drlb_server
# systemctl enable drlb_server

RHEL 6/CENTOS 6:

# service drlb_server start
# service drlb_server status
# chkconfig drlb_server on

Configuration Changes
---------------------
All changes to a DRLB configuration file or assignment rules will
require a server restart:

RHEL 7/CENTOS 7:

# systemctl restart drlb_server

RHEL 6/CENTOS 6:

# service drlb_server restart

For DRLB configurations running more than one LB server you can
manually restart the each server instance:

# ps -ef | grep drlb_server | grep -v grep

root 17058 1 0 23:57 ? 00:00:00 /usr/sbin/drlb_server --config-file=/etc/drlb/http_rr_test.cfg
root 17075 1 0 23:57 ? 00:00:00 /usr/sbin/drlb_server --config-file=/etc/drlb/ldm_cluster.cfg

In the example above you can restart only the http_rr_test.cfg
instance by manually stopping and starting process ID 17058:

# kill  17058
# /usr/sbin/drlb_server –config-file=/etc/drlb/http_rr_test.cfg &
# ps -ef | grep drlb_server | grep -v grep

Setting the listening port
--------------------------
The DRLB server listens for incoming TCP connections on a port number
specified in the global server configuration. The DRLB server
distributes incoming TCP connections to back end server nodes. In the
DRLB configuration you are setting up for a load balanced service you
must specify the front end listening port in the global [LBSERVER]
config section. For each back end node you must specify the back end
listening port for each's node [LBNODE] config section. For example,
if you created a configuration template named "http_lb.cfg" make the
following entries to set the listening ports. NOTE: The port numbers
used in the example below must be substituted with the port number you
are actually using. The use of 4 LBNODE sections is just an
example. Your setup can contain any number of LBNODE sections
depending on how many back end server nodes you have available.

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

NOTE: For developers, if your [LBSERVER] port is less than 1024 you
will require root privileges to start the DRLB program for testing.
NOTE: For system administrators, you are still required to adhere to
all your required service security settings on each back end server
node.

Round robin configuration
-------------------------
The round robin load balancing scheme distributes TCP connections
equally to all back end server nodes. For round robin to work
efficiently on bare metal servers, each back end server should have
the same or very similar hardware specifications. For virtual machines
each back end server should be setup with the same amount of CPU,
memory, and disk image parameters. To select round robin, set the
[LBSERVER] scheme to LB_RR. The [LBNODE] sections require a node name,
port number, and the node's IP address. For example:

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

NOTE: The port numbers in the above example must be substituted with
the port number you are actually using. The use of 4 LBNODE sections
is just an example. Your setup may contain any number of LBNODE
sections depending on how many back end server nodes you have
available.

Weighted configuration
----------------------
The weighed round robin load balancing scheme distributes TCP
connections based on a weight value you assign to each back end server
node. Weighted round robin allows you to use servers with different
hardware configurations, where one or more back end server cannot
handle the same load as other servers. The DRLB weight value
represents a number of connections per node. The nodes with the
highest weight values will get the most connections. The nodes with
the lowest weight values will get the lease number of connections. To
select weighted round robin, set the [LBSERVER] scheme to
LB_WEIGHTED. The [LBNODE] sections require a node name, port number,
the node's IP address, and a weight value.

In the example below, lbnode4 will carry 4 times the amount of
connections than lbnode3, lbnode1 and lbnode2. Lbnode3 will carry 3
times the amount of connections than lbnode1 and lbnode2. Lbnode1 and
lbnode2 will get the lease amount of connections. For example:

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

NOTE: The port numbers in the above example must be substituted with
the port number you are actually using. The use of 4 LBNODE sections
is just an example. Your setup may contain any number of LBNODE
sections depending on how many back end server nodes you have
available.

Distributed configuration
-------------------------
The distributed load balancing scheme distributes TCP connections
based on a percentage value you assign to each back end server
node. Similar to weighted round robin, distributed load balancing
allows you to use servers with different hardware configurations,
where one or more back end server cannot handle the same load as other
servers. The main difference is load distribution is based on a
percentage rather than a weight value. To select distributed load
balancing, set the [LBSERVER] scheme to LB_DISTRIB. The [LBNODE]
sections require a node name, port number, the node's IP address, and
a percentage value. The nodes with the highest percent values will get
the most connections. The nodes with the lowest percent values will
get the lease number of connections.

In the example below, lbnode4 will carry 40% of total
connections. Lbnode3 will carry 30% of the total connections. Lbnode1
and lbnode2 will carry 15% of the total connections. For example:

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

NOTE: The port numbers in the above example must be substituted with
the port number you are actually using. The use of 4 LBNODE sections
is just an example. Your setup may contain any number of LBNODE
sections depending on how many back end server nodes you have
available.

Assigned configuration
----------------------
The assigned load balancing scheme allows you to assign TCP
connections to specific back end nodes based on the IP address of the
incoming connection. Incoming connections that do not have assignment
rules can be configured to use round robin, weighted, or distributed
load balancing schemes. The same load balancing scheme used for
unassigned connections will be applied to down server nodes handling
connection assignment rules. To select assigned load balancing, set
the [LBSERVER] scheme to LB_ASSIGNED. The [LBNODE] sections require a
node name, port number, and the node's IP address. In the [LBSERVER]
section, the assigned_default can be set to:

LB_RR
LB_DISTRIB
LB_WEIGHTED
LB_NONE

If you select LB_DISTRIB, each [LBNODE] section will require a
percentage value set for the weight value. If you select LB_WEIGHTED,
each [LBNODE] section will require a weight value.

The LB_NONE setting will not connection assigned connections to
another node if the node handling the assignment rules is
down. LB_NONE will use round robin for all incoming connections with
no assignment rules.

The assigned load balancing scheme requires a rules configuration
file, set in the [LBSERVER] section's rules_config_file setting, for
example:

rules_config_file = /etc/drlb/http_lb_rules.cfg

If the rules file does not exist, a default file will be created for
you when you test your server configuration or start the DRLB server.

Below is an assigned load balance configuration using round robin as
the default for 4 back end server nodes:

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

NOTE: The port numbers in the above example must be substituted with
the port number you are actually using. The use of 4 LBNODE sections
is just an example. Your setup may contain any number of LBNODE
sections depending on how many back end server nodes you have
available.

After modifing the DRLB configuration, add your assignment rules to
the rules configuration file. Each assignment rule is designated be
the route key word followed by the IP address of the incoming
connection and the name of the back end server node handling the
assignment. The IP address for the incoming connections can be an IP
address or hostname as a single value or an extended regular
expression. To setup rules, edit the rules file:

# vi /etc/drlb/http_lb_rules.cfg

route 192.168.122.1 lbnode1
route ^10.* lbnode2
route ^172\.31\.1.* lbnode3

In the examples above, the first route rule directs all traffic from
the 192.168.122.1 IP address to lbnode1. The second example directs
all traffic from the 10.0.0.0/8 network to lbnode2. The third example
directs all traffic from the 172.31.1.0/24 network to lbnode3. All
other incoming traffic will use round robin connections to node 1
through 4.

NOTE: Name lookups for hostnames can cause performance issues. If you
need to disable name lookups, edit your DRLB configuration file and
change the following:

[LBSERVER]
...
resolve_assigned_hostnames = 0

Once disabled, the route rules will only use the IP address of the
incoming connection.

The assignment rules file is read when the DRLB server starts or is
restarted. Changes to the rules file requires a service restart to
take affect.

Throttling configuration
------------------------
Connection throttling can be used to prevent server saturation by
placing incoming connections into a wait queue. Throttling can be
based on a preset connection count or based on the number of incoming
connections per second.

To setup a preset connection throttle, edit your DRLB configuration
file and add the following to the [LBSERVER] global configuration
section:

[LBSERVER]
...
enable_throttling = 1
throttle_every_connections = 10
throttle_wait_secs = 1
throttle_wait_usecs = 0

In the example above every 10 connections will be throttled with a
wait queue time of 1 second. If throttle_every_connections is set to
1, every incoming connection will be throttled.

To setup connections per second throttling, edit your DRLB
configuration file an add the following to the [LBSERVER] global
configuration section:

[LBSERVER]
...
enable_throttling = 1
throttle_apply_by_load = 1
throttle_connections_per_sec = 10
throttle_every_connections = 10
throttle_wait_secs = 1
throttle_wait_usecs = 0

In the example above every 10 connections will be throttled when the
incoming server load reaches 10 connections per second. When the load
drops below 10 connections per second, throttling will be disabled
until the load spikes again to 10 per connections second. By default,
statistics collection is every 5 minutes. You can increase or decrease
stat collection time by setting the following global configuration
values:

stats_min = 5
stats_secs = 0

Log configuration
-----------------
DRLB server logging is disabled by default. To get maximum server
performance under extreme load conditions you may want to leave
logging disabled or set the log level to zero. To enable logging, edit
your DRLB configuration file an add the following to the [LBSERVER]
global configuration section:

[LBSERVER]
...
enable_logging = 1
logfile_name = /var/log/drlb/http_lb.log
num_logs_to_keep = 3
max_log_size = 5000000
clear_log = 0
log_level = 0
service_name = http_lb

In the above example the DRLB server will log to http_lb.log file. The
maximum log file size will be 5MB and we will keep the current log
file and 3 previous copies. The required disk space for log files is
equal to:

max_log_size * (num_logs_to_keep +1)

The log_level can be set to 0 (lowest) level or 5 (highest) level. The
higher the log level, the more messages will be logged. The
service_name is an optional setting you can use to customize log file
entries.

To increase server performance you have the option to adjust the
advanced DRLB log settings:

log_queue_size = 2048
log_queue_debug_size = 4096
log_queue_proc_size = 255

The log queue size equals the number of log messages the server will
spool. Memory usage is equal to:

log_queue_size * 255

Spooling more messages to memory will decrease the consecutive number
of log file writes. The debug queue size only applies if debugging is
enabled. The proc queue size is used for process messages and does not
need to be very large as process messages are written during server
stops, starts, restarts, or process error conditions.

To watch the log in real time:

# tail -f /var/log/drlb/http_lb.log


Statistics collection
---------------------
DRLB server statistics collection is disabled by default. To enable
stat collection, edit your DRLB configuration file an add the
following to the [LBSERVER] global configuration section:

[LBSERVER]
...
enable_stats = 1
stats_file_name = /var/log/drlb/http_lb_stats.log
max_stats_size = 5000000
num_stats_to_keep = 3
stats_min = 5
stats_secs = 0

In the above example the DRLB server will collect server stats every 5
minutes and write the stat messages to log to http_lb_stats.log
file. The maximum stat file size will be 5MB and we will keep the
current stats file and 3 previous copies. The required disk space for
stat files is equal to:

max_stats_size * (num_stats_to_keep +1)

You can increase or decrease stat collection times by adjusting the
stats_min and stats_secs values.

To watch the stats in real time:

# tail -f /var/log/drlb/http_lb_stats.log

Setting connection and thread limits
------------------------------------
To protect low resource servers from saturation you set connection
limits. Connection limits will stop serving connections when the
maximum number connections is reached. Connection limits can be set
for all back end server nodes or for individual back end server
nodes. To set connection limits for all back end nodes edit your DRLB
server configuration and add the following to the [LBSERVER] section:

[LBSERVER]
...
max_connections = 1000

This will limit the total number of concurrent connections to
1000. When the 1000 connection limit is reached the DRLB server will
not allow any more connections until the total number of connection
drops below 1000.

To set the number of connections for individual back end nodes:

[LBNODE]
node_name = lbnode1
max_connections = 1000

[LBNODE]
node_name = lbnode2
max_connections = 200

[LBNODE]
node_name = lbnode3
max_connections = 200

[LBNODE]
node_name = lbnode4
max_connections = 200

The will limit lbnode1 to 1000 concurrent connections and lbnode2
through lbnode4 to 200 concurrent connections each.

To limit the number of threads the DRLB server process can generate
you can set the max_threads limit in the [LBSERVER] section:

[LBSERVER]
...
max_threads = 1000

Limiting the number of threads the DRLB process can generate will
limit all server connection depending on how many client threads are
active. Thread limiting protects server resources when there is a
large number of persistent client connections.

Setting buffer and caching options
----------------------------------
Caching is used by default to buffer incoming connections as data is
read from TCP socket connections. In certain network environments it
may be necessary to disable cache buffering or change the default
buffer size. Lowing the buffer size will result in more reads per
connection. Increasing the buffer size will decrease the number of
reads per connection. To set the cache or buffer size settings for all
nodes, edit your configuration file and modify the following
parameters in the [LBSERVER] section:

[LBSERVER]
...
use_buffer_cache = 1
buffer_size = 1500

To set the buffer size for individual nodes:

[LBNODE]
node_name = lbnode1
buffer_size = 1500
...

[LBNODE]
node_name = lbnode2
buffer_size = 1500
...

[LBNODE]
node_name = lbnode3
buffer_size = 1500
...

[LBNODE]
node_name = lbnode4
buffer_size = 1500
...

Dead node retry settings
------------------------
When a back end server node is down for maintenance or hardware
failures you can change the number of retires and the wait time for
retries. To change the retry values, edit your configuration file and
modify the following parameters in the [LBSERVER] section:

[LBSERVER]
...
retries = 3
retry_wait = 1


Buffer overflow settings
------------------------
When cache buffer is enabled you can enable buffer overflow
protection, provided you know the packet sizes your application
uses. With buffer overflow protection enabled, the DRLB server will
deny a connection if the number of bytes read on a front end socket
connection exceeds the buffer overflow size. To enable buffer overflow
protection edit your configuration file and modify the following
parameters in the [LBSERVER] section:

[LBSERVER]
...
enable_buffer_overflow_detection = 1
buffer_overflow_size = 5000

This will limit incoming socket reads to 5000 bytes per socket read.

Advanced socket settings
------------------------
Each of the setting below are set in the [LBSERVER] global section in
your DRLB server configuration file.

To listen on a single Ethernet interface:

[LBSERVER]
...
listen_ip_addr = 192.168.122.1

The default is to listen on all interfaces. 

To change the maximum number of back logged connections, set the
somaxconn parameter based on the kernel setting:

# cat  /proc/sys/net/core/somaxconn 

Typically the default value 128 and the maximum value is 1024. If this
value is higher than 128 on your server you can adjust your DRLB
configuration:

[LBSERVER]
...
somaxconn = 1024

Depending on your application you may need to adjust the DRLB socket
options to prevent CPU usage spikes. To adjust the socket options edit
the following options in your configuration file:

[LBSERVER]
...
# Idle time for blocking sockets
max_idle_count = 60000
idle_wait_secs = 0
idle_wait_usecs = 500

# Use non-blocking sockets
use_nonblock_sockets = 0

The idle time setting adjustments are most effective for a large
number of persistent connections. For connections that stay open for
long periods without transferring data, increasing the values will
reduce CPU usage. Decreasing the idle times or using non-blocking
sockets is most affective for connections that rapidly open and close.

For applications that rely on timing for socket reads you can set your
DRLB configuration to use blocking reads with timeout values:

[LBSERVER]
...
use_buffer_cache = 0
use_timeout = 1
timeout_secs = 300
timeout_usecs = 0

This will disable the buffer cache and set the read timeout value to
300 seconds. If the socket connection does not receive data within 300
seconds the connection will close.

DRLB process monitoring
-----------------------
Use the shell commands below to watch DRLB CPU, memory, file, and
thread resources.

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

BASH script to log key process parms in real time:

#!/bin/bash

numsecs=9999
pids=$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')

echo "Watching:" | tee -a /tmp/watch_drlb_proc.log
ps -ef | grep drlb_server | grep -v grep | tee -a /tmp/watch_drlb_proc.log
echo "" | tee -a /tmp/watch_drlb_proc.log

i=0
while [ $i -le $numsecs ]; do
    let i=i+1
    echo "Num open files:" | tee -a /tmp/watch_drlb_proc.log
    for p in $pids; do ls -l --color=none /proc/$p/fd 2>/dev/null | wc -l; done | tee -a /tmp/watch_drlb_proc.log
    echo "" | tee -a /tmp/watch_drlb_proc.log
    echo "Memory usage:" | tee -a /tmp/watch_drlb_proc.log
    for p in $pids; do cat /proc/$p/status | grep -i vmsize; done | tee -a /tmp/watch_drlb_proc.log
    echo "" | tee -a /tmp/watch_drlb_proc.log
    echo "Num threads " | tee -a /tmp/watch_drlb_proc.log
    for p in $pids; do cat /proc/$p/status | grep -i threads; done | tee -a /tmp/watch_drlb_proc.log
    echo "" | tee -a /tmp/watch_drlb_proc.log
    sleep 1
done

Setting Open File Limits
------------------------
NOTICE: Any setting you make to change default kernel limits and/or
user limits must be implemented with caution. All changes should be
thoroughly tested on development servers whenever possible. If you are
not careful when adjusting default limits you can render your server
inoperable.

In RHEL6/CENTOS6 and RHEL7/CENTOS7 the default number of file,
including sockets, is 1024. For load balancer applications with a high
volume of connections you will need to increase the default value.

For RHEL7/CENTOS7, the global ulimit for nofiles is ignored for
systemd services. The nofile limit is now set in the service file:

# more /etc/systemd/system/drlb_server.service

[Service]
LimitNOFILE=65535

For RHEL7/CENTOS7 runtime testing you can change the nofile limit for
a process using the prlimit command:

# ps -ef | grep drlb_server | grep -v grep
# prlimit --pid [pid] --nofile=32767:65535

For RHEL6/CENTOS6, you can increase the max number of open files by
increasing the ulimit value:

# vi /etc/security/limits.d/nofile.conf

*    soft    nofile 32767
*    hard    nofile 65535

This change will require a reboot.

For RHEL6/CENTOS6 runtime testing you can manually set the nofile
limits and start the DRLB server the same shell:

# ulimit -n 32767
# ulimit -H -n 65535

To check the file limits for the DRLB process(es):

# pids=$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')
# for p in $pids; do cat /proc/$p/limits | grep -i file; done

The Max open files setting should match your ulimit settings for soft
and hard limits. To see how many files the process(es) have open:

# for p in $pids; do ls -l --color=none /proc/$p/fd; done

Linux Kernel Tuning
-------------------
NOTICE: Any setting you make to change default kernel settings and/or
user limits must be implemented with caution. All changes should be
thoroughly tested on development servers whenever possible. If you are
not careful when adjusting default kernel settings you can render your
server inoperable.

Load balancing will require kernel tuning based the volume of traffic
and type of TCP service you are load balancing. NOTE: The values in
listed below are based on extremely high volume. Depending on your
application you may not need to set many of the parameters listed. But
please keep in mind that you will need to tune the kernel for any
server acting as a load balancer.

All setting listed below must be application tested and set based on
the number of connections to your frontend LB and backend hosts. For
each of the settings below use the sysctl command to backup the
orginal settings before making any changes, for example:

# sysctl fs.file-max | tee -a /root/sysctl_org_settings.txt

To make the changes persistent, add the setting(s) to the sysctl.conf
file:

CENTOS 5/6 or RHEL 5/6:
# vi /etc/sysctl.conf

CENTOS 7 or RHEL 7:
# vi /usr/lib/sysctl.d/00-system.conf

To load changes CENTOS 5/6 or RHEL 5/6:
# sysctl -p

To load changes CENTOS 7 or RHEL 7:
# sysctl -p /usr/lib/sysctl.d/00-system.conf

File Handle Limits:
------------------
Increase the maximum number of open file descriptors:

fs.file-max = 25000000

IPv4 Socket Tuning:
------------------
For a large numbers of concurent sessions:

net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_tw_recycle = 1
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 60
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_max_syn_backlog = 4096
net.ipv4.tcp_syncookies = 1
net.core.somaxconn = 1024

IPv4 Settings:
-------------
net.ipv4.ip_forward = 1
net.ipv4.ipfrag_max_dist = 4096
net.ipv4.ip_nonlocal_bind=1

Process Scheduler:
-----------------
Increase time for process run time before the kernel will consider
migrating it again to another core: 

CENTOS 5/6 or RHEL 5/6:
kernel.sched_migration_cost = 5000000

CENTOS 7 or RHEL 7:
kernel.sched_migration_cost_ns = 5000000

kernel.sched_autogroup_enabled = 0

Threads:
-------
Increase maximum number of threads that can be created by a process:

kernel.threads-max = 5000000
vm.max_map_count = 128000
kernel.pid_max = 65536

Testing with Apache
-------------------
A simple way to test your DRLB server is to use Apache on KVM virtual
machines. Once you have Apache running on a one or more virtual
machines copy the following PHP scripts to the Web root on each VM,
for example

$ cd $HOME/git/datareel/load_balancer/apache_test
$ chmod 644 *.php

$ for s in 192.168.122.111 192.168.122.112 192.168.122.113
192.168.122.114 
> do
> scp -p *.php username@${s}:/var/www/html/.
> done

$ cd $HOME/git/datareel/load_balancer/drlb_server
$ ./drlb_server --config-file=../etc/http_rr_test.cfg --log-file=/tmp/http_rr.log > /tmp/drlb_start.log &

To connect using wget, assuming the DRLB server is listening on port
8080 on the bare metal host:

$ cd /tmp
$ wget -O index.html http://192.168.122.1:8080

Persistent TCP connection:

$ wget -O loop1.txt http://192.168.122.1:8080

NOTE: If you test using Firefox, you will be limited to 6 persistent
session. By default Firefox only allow 6 simultaneous downloads. To
increase this limit for testing, enter the following URL:

about:config

Search for:

network.http.max-persistent-connections-per-server

Change the default from 6 to a higher number.

HTTP IP address forwarding
--------------------------
Each of the setting below are set in the [LBSERVER] global section in
your DRLB server configuration file. To enable HTTP IP address
forwarding edit your configuration file and add the following:

[LBSERVER]
...
enable_http_forwarding = 1
http_request_strings = GET,HEAD,POST
http_hdr_str = HTTP/
http_forward_for_str = X-Forwarded-For

NOTE: To see forwarded IP addresses in your Apache log entries you
must add the following to your httpd conf:

LogFormat "%h %l %u %t \"%r\" %>s %b %{X-Forwarded-For}i \"%{Referer}i\" \"%{User-Agent}i\"" combined
LogFormat "%h %l %u %t \"%r\" %>s %b" %{X-Forwarded-For}i common

Remaining work on this project
------------------------------
Remaining work for this open source project:

Support and Bug Tracking
------------------------
https://github.com/datareel/datareel/issues

