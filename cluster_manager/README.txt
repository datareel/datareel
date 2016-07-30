DataReel Cluster Manager (DRCM) 1.X Readme File
Copyright (c) 2016 DataReel Software Development

https://github.com/datareel/datareel/tree/master/cluster_manager

Contents:
--------
* Overview
* Documented Usage
* Prerequisites
* Building and Installing
* DRCM Auth Tokens
* Building and Installing From Source Code
* Testing CM Configuration Files
* CM Node Hostname and IP Settings
* CM Global Configuration Settings
* Setting Up Global Cluster Resources
* Linking Global Cluster Resources to Nodes
* Floating IP Address Interface Configuration
* Running the DRCM Server
* DRCM Resource Scripts 
* Running Applications:
* Remaining Work on This Project
* Support and Bug Tracking

Overview:
--------
The DataReel cluster manager (DRCM) software is used to build high
availability Linux server clusters. High availability clusters manage
system resources with automatic fail over in event of one or more
server failures. The DRCM project was designed to support multiple
cluster nodes in a non-hierarchical arrangement. Cluster resources are
distributed between the nodes with backup rolls assigned. The DRCM
project requirements were based on the need to replace existing
cluster software with multiple dependency issues and cluster software
that did not scale well more than 2 cluster nodes. A  Linux server
cluster is typically 2 servers with equal capacity, where one server
provides primary services and the other server provides backup
services. In the event of a primary server failure the secondary
server automatically assumes all cluster resources. In data centers
utilizing virtualization technology, services are distributed between
many virtual machines (VMs). In a VM environment clustering multiple
VM nodes running on multiple bare metal servers is a very effective
way to manage services in the event of a bare metal server
failure. The DRCM multiple node concept can scale between nodes in one
or more data centers in the same or different geographical locations.

Documented Usage:
----------------
* All commands prefixed with the $ prompt indicate the command is
  executed from a user account or from a service account. 

* All commands prefixed with the # prompt indicate the command is
  execute as root. 

* In configuration file example a ... symbol indicates omitted content for
  brevity.   

Prerequisites:
-------------
DRCM resources consist of crontabs, floating IP addresses, services,
applications, and file systems. When you plan your cluster setup,
consider the following: 

* Number of nodes per cluster
* Assigning crons based on CPU, memory, disk space and bandwidth requirements
* Floating IP address requirements, where a service is accessed via one IP address 
* System services: http, mysql, postgresql, etc. Which node(s) needs to run services
* File systems: NFS, cifs, etc. Which node(s) require file system mounts
* Applications: Programs not part of the OS with user programs and scripting
* SSH keys: keyed authentication for file synchronization and automation

Building and Installing From Source Code:
----------------------------------------
Developers and system admins, follow the instructions below to build
and install the DRCM server from the source code distro:

$ ssh username@cmnode1
$ mkdir -pv ~/git
$ cd ~/git
$ git clone https://github.com/datareel/datareel.git
$ cd ~/git/datareel/cluster_manager/drcm_server
$ make
$ sudo su root -c 'make install_root'

The default configuration directory is: /etc/drcm

NOTE: All nodes in the same cluster must have identical “/etc/drcm”
directories. It's recommended to design your initial configuration on a
node you designate as a cluster entry point. Then push all “/etc/drcm”
updates to all the nodes in the cluster.  

DRCM Auth Tokens:
----------------
All nodes in a cluster must have the same auth tokens. To generate
auth tokens for the first time or to replace existing tokens: 

# mkdir -pv /etc/drcm/.auth
# chmod 700 /etc/drcm/.auth
# dd if=/dev/urandom bs=1024 count=2 > /etc/drcm/.auth/authkey
# sha1sum /etc/drcm/.auth/authkey | cut -b1-40 > /etc/drcm/.auth/authkey.sha1
# chmod 600 /etc/drcm/.auth/authkey /etc/drcm/.auth/authkey.sha1

Building CM Configuration Files:
-------------------------------
The default CM configuration is: /etc/drcm/cm.cfg
To build a CM configuration template run the following command: 

# /usr/sbin/drcm_server --check-config

To build a CM configuration template in another location:

# /usr/sbin/drcm_server --config-file=/tmp/test1.cfg --check-config

Testing CM Configuration Files:
------------------------------
After editing the CM configuration file you must check your CM
configuration files for errors before starting the DRCM server: 

# /usr/sbin/drcm_server --check-config | grep -i -E 'error|warn|fail|info'
 
If you have any errors or warnings, review and edit your configuration file:

# /usr/sbin/drcm_server --check-config 

CM Node Hostname and IP Settings:
--------------------------------
After generating the cluster configuration file you must edit the file
and set up your global cluster configuration and node
configuration. For nodes you include in a cluster you must have the 
following information for each node: 

Fully qualified hostname matching: uname -n
IP address used for cluster keep alive and CM message handling. 

# vi /etc/drcm/cm.cfg

[CM_NODE]
nodename = cmnode1  # This can be any name you will recognize this node as
hostname = cmnode1.example.com # uname -n
keep_alive_ip = 192.168.122.183   
...

[CM_NODE]
nodename = cmnode2                                                                                                                                            
hostname = cmnode2.example.com
keep_alive_ip = 192.168.122.184
...

CM Global Configuration Settings:
--------------------------------
The CM_SERVER configuration setting sets values used by the DRCM
service and all cluster nodes. Before starting the DRCM service, set
the CM port number and host-based firewall rules: 

# Global cluser configuration                                                                                                                                                                     
[CM_SERVER]
udpport = 53897
tcpport = 53897
...

Each cluster node must have host-based firewall rules allowing the
DRCM server access to the TCP and UDP ports set in CM global
configuration:

# iptables -N CLUSTERMANAGER
# iptables -I INPUT 3 -j CLUSTERMANAGER
# iptables -A CLUSTERMANAGER -p UDP --dport 53897 -s 192.168.122.183/32 -j ACCEPT 
# iptables -A CLUSTERMANAGER -p UDP --dport 53897 -s 192.168.122.184/32 -j ACCEPT 
# iptables -A CLUSTERMANAGER -p TCP --dport 53897 -s 192.168.122.183/32 -j ACCEPT 
# iptables -A CLUSTERMANAGER -p TCP --dport 53897 -s 192.168.122.184/32 -j ACCEPT 

Setting Up Global Cluster Resources:
-----------------------------------
All nodes in a cluster has the ability to run a global cluster
resource as a primary function or as a backup function. DRCM has 5
global resource categories:

CM_CRONTABS: crons to run on a node with fail over to secondary nodes
CM_IPADDRS: floating IP addresses to run on a node with fail over to secondary nodes
CM_SERVICES: System services to run on all nodes or single nodes
CM_APPLICATIONS: Applications to run on all nodes or single nodes
CM_FILESYSTEMS: File system to mount on all nodes or single nodes

Under each resource section you must provide a CSV list for each
entry. All resources subsections start with a nickname. A nickname can
be any unique name you will recognize the resource as. Nicknames are
used by the cluster nodes to identify the global resource. 

[CM_CRONTABS]
# CSV format: nickname, template_file, install_location, resource_script                                                                                                                     
system, /etc/drcm/crontabs/system_crons, /etc/cron.d, /etc/drcm/resources/crontab.sh
user, /etc/drcm/crontabs/user_crons, /etc/cron.d, /etc/drcm/resources/crontab.sh
apps1, /etc/drcm/crontabs/apps_package1, /etc/cron.d, /etc/drcm/resources/crontab.sh
apps2, /etc/drcm/crontabs/apps_package2, /etc/cron.d, /etc/drcm/resources/crontab.sh

[CM_IPADDRS]
# CSV format: nickname, Floating IP, netmask, Ethernet interface, ip takeover script                                                                                                         
cmnodef, 192.168.122.180, 255.255.255.0, eth0:1, /etc/drcm/resources/ipv4addr.sh

[CM_SERVICES]
# CSV format: nickname, service_name, resource_script                                                                                                                                        
web, httpd, /etc/drcm/resources/service.sh

[CM_APPLICATIONS]
# CSV format: nickname, user:group, start_program, stop_program                                                                                                                              
# CSV format: nickname, user:group, start_program, stop_program, ensure_script                                                                                                               
ldm, ldm:users, ~/util/ldm_ensure.sh, ldmadmin stop, ~/util/ldm_ensure.sh

[CM_FILESYSTEMS]
# CSV format: nickname, source, target, resource_script                                                                                                                                      
data, 192.168.122.1:/data, /data, /etc/drcm/resources/nfs.sh
archive, 192.168.122.1:/archive, /archive, /etc/drcm/resources/nfs.sh
webshare, //192.168.122.225/users/web, /home/users/web, /etc/drcm/resources/cifs.sh

Linking Global Cluster Resources to Nodes:
-----------------------------------------
The global cluster resource for a node is set in each CM_NODE
section. Below is a 2 node cluster example of multiple crontabs with
primary and secondary rolls. A single floating IP address for cluster
users to access this cluster. In this example system services, file
systems, and applications will be ran on both nodes.   

[CM_NODE]
nodename = cmnode1
...
node_crontabs = user,system,apps1
node_backup_crontabs = cmnode2:apps2
node_floating_ip_addrs = cmnodef
node_services = web
node_filesystems = data,arhive,webshare
node_applications = ldm

[CM_NODE]
nodename = cmnode2
...
node_crontabs = apps2
node_backup_crontabs = cmnode1:user,cmnode1:system
node_backup_floating_ip_addrs = cmnode1:cmnodef
node_services = web
node_filesystems = data,arhive,webshare
node_applications = ldm

Floating IP Address Interface Configuration:
-------------------------------------------
If your cluster has one or more floating IP addresses you must
configure a sub-interface for each floating IP. A floating IP address
is set in the global resource configuration  CM_IPADDRS section, for
example:

[CM_IPADDRS]
cmnodef, 192.168.122.180, 255.255.255.0, eth0:1, /etc/drcm/resources/ipv4addr.sh

In the example above, we need to setup a sub-interface for eth0:

# cd /etc/sysconfig/network-scripts
# vi ifcfg-eth0:1

DEVICE=eth0:1
BOOTPROTO=none
ONBOOT=yes

Save changes and exit VI.

IPADDR and PREFIX or NETMASK will be set by DRCM server when the node
resource is activated.

# ifup eth0:1

Running the DRCM Server:
-----------------------
After setting up your cluster configuration and mirroring the
“/etc/drcm” directory to all nodes in the cluster, start the DRCM
server on each node. 

RHEL 7/CENTOS 7:

# systemctl start drcm_server
# systemctl status drcm_server

RHEL 6/CENTOS 6:

# service drsm_server start
# service status drcm_server

View the cluster status monitor:

# drcm_server --client --command=cm_stat

View the cluster logs:

# tail -f /var/drcm/drcm.log

To test resource fail over:

# systemctl stop drcm_server
# drcm_server --client --command=cm_stat

To test resource fail back:

# systemctl start drcm_server
# drcm_server --client --command=cm_stat

After testing make the DRCM service persistent:

RHEL 7/CENTOS 7:

# systemctl enable drcm_server

RHEL 6/CENTOS 6:

# chkconfig drsm_server on

DRCM Resource Scripts:
---------------------
The default DRCM resource script location is: /etc/drcm/resources

The default installation include system resource script to manage
crontabs, floating IP addresses, system services, NFS and CIFS files
systems. The NFS and CIFS file system resource script will check for
stale mounts and automatically remount if a mount is stale.  

Which resource scripts used is set in the global resource
configuration. Cluster administrators can use custom resource scripts,
provided custom scripts accept the same input arguments as the
resource script provided with the distribution. 

Running Applications:
--------------------
Applications refer to programs that are not part of the operating
system (OS) or integrated into the OS framework. Managing user
applications in a cluster can be difficult. The DRCM server does not
manage user applications from resource scripts. This allows
flexibility to run applications from a user or service account and use
programs and scripting unique to each application. Applications are
defined in the global CM_APPLICATIONS section: 
    
[CM_APPLICATIONS]

There are 2 formats to define a user application:

# CSV format: nickname, user:group, start_program, stop_program                                                                                                                              
# CSV format: nickname, user:group, start_program, stop_program, ensure_script                                                                                                               

The “user:group” must be set to the username that will run the
application and the group associated with application.  

The “start_program” is the program used to start the application, with
or without input arguments. This can be multiple entries separated by
semicolons ; 

The “stop_program” is the program used to start the application, with
or without input arguments. This can be multiple entries separated by
semicolons ; 

The “ensure_script” is a script or program that checks to ensure the
application is running. If an ensure script is provided the DRCM
server will use the “ensure_script” to watch the application. 

NOTE: The user environment will be sourced so you do not have to use
absolute paths to programs if the program path is defined in the user
environment.   

Some examples:

ldm, ldm:users, ldmadmin start, ldmadmin stop

ldm, ldm:users, ldmadmin clean; ldmadmin mkqueue; ldmadmin start, ldmadmin stop; ldmadmin delqueue

ldm, ldm:users, ldmadmin start, ldmadmin stop, if [ -f ~/util/ldm_ensure.sh ]; then ~/util/ldm_ensure.sh; fi

Remaining Work on This Project:
------------------------------ 
The first DRCM release was successfully tested on VM and bare metal
clusters. Implementation will continue on many data systems. All
source code changes and bug fixes will be release in this open source
distribution.    

Remaining work to be added to this open source project:

* RPM install for CENTOS/RHEL 6.X and 7.X
* HTML Documentation

Support and Bug Tracking:
------------------------

https://github.com/datareel/datareel/issues
