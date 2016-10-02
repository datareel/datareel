DataReel Cluster Manager (DRCM) 1.X Readme File
Copyright (c) 2016 DataReel Software Development

https://github.com/datareel/datareel/tree/master/cluster_manager

Contents:
--------
* Overview
* Documented Usage
* Prerequisites
* RPM Install
* Building and Installing From Source Code
* DRCM Auth Tokens
* Testing CM Configuration Files
* CM Node Hostname and IP Settings
* CM Global Configuration Settings
* Setting Up Global Cluster Resources
* Linking Global Cluster Resources to Nodes
* Floating IP Address Interface Configuration
* Running the DRCM Server
* DRCM Resource Scripts 
* Running Applications
* Cluster Monitor and Reports
* 2-way file replication
* Setting up cluster alerts
* Remaining Work on This Project
* Support and Bug Tracking

Overview:
--------
The DataReel cluster manager (DRCM) software is used to build high
availability Linux server clusters. High availability clusters manage
system resources with automatic fail over in event of one or more
server failures. 

The DRCM project was designed to support multiple cluster nodes in a
non-hierarchical arrangement. Cluster resources are distributed
between the nodes with backup rolls assigned. The DRCM project
requirements were based on the need to replace existing cluster
software with multiple dependency issues and cluster software that did
not scale well more than 2 cluster nodes. 

A  Linux server cluster is typically 2 servers with equal capacity,
where one server provides primary services and the other server
provides backup services. In the event of a primary server failure the
secondary server automatically assumes all cluster resources. In data
centers utilizing virtualization technology, services are distributed
between many virtual machines (VMs). In a VM environment clustering
multiple VM nodes running on multiple bare metal servers is a very
effective way to manage services in the event of a bare metal server 
failure.

The DRCM multiple node concept can scale between nodes in one or more
data centers in the same or different geographical locations. 

Documented Usage:
----------------
* All commands prefixed with the $ prompt indicate the command is
  executed from a user account or from a service account. 

* All commands prefixed with the # prompt indicate the command is
  executed as root. 

* In a configuration file example a ... symbol indicates omitted content
  for brevity.   

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

RPM Install
-----------
System administrators, follow the instructions below to build an RPM
for distribution:

$ cd $HOME; mkdir -pv git; cd $HOME/git
$ git clone https://github.com/datareel/datareel
$ cd ${HOME}/git/datareel/cluster_manager/rpm_builder

RHEL7/CENTOS7:

$ ./make_rhel7_rpm.sh
$ sudo su root -c "yum -y install ${HOME}/rpmbuild/RPMS/x86_64/drcm_server-1.37-1.el7.x86_64.x86_64.rpm"

RHEL6/CENTOS6:

$ ./make_rhel6_rpm.sh
$ sudo su root -c "yum -y install ${HOME}/rpmbuild/RPMS/x86_64/drcm_server-1.37-1.el6.x86_64.x86_64.rpm"

Useful RPM command to verify package contents:

List package: rpm -qf /usr/sbin/drcm_server
List files: rpm -ql drcm_server
List configs: rpm -qc drcm_server
List docs: rpm -qd drcm_server

To remove package:

$ sudo su root -c 'yum -y remove drcm_server'

Building and Installing From Source Code:
----------------------------------------
Developers and system admins, follow the instructions below to build
and install the DRCM server from the source code distro:

$ ssh username@cmnode1
$ mkdir -pv ~/git
$ cd ~/git
$ git clone https://github.com/datareel/datareel.git
$ cd ~/git/datareel/env
$ source gnu_env.sh
$ cd ~/git/datareel/cluster_manager/drcm_server
$ make
$ sudo su root -c 'make install_root'

The default configuration directory is: /etc/drcm

NOTE: All nodes in the same cluster must have identical "/etc/drcm"
directories. It's recommended to design your initial configuration on a
node you designate as a cluster entry point. Then push all "/etc/drcm"
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
bind_to_keep_alive_ip = 1
...

By default the DRCM server will only listen for cluster messages on
each node's keep alive IP address. To listen on all interfaces change
the bind_to_keep_alive_ip to 0. 

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
webshare, /192.168.122.225/users/web, /home/users/web, /etc/drcm/resources/cifs.sh

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

IPADDR and PREFIX or NETMASK will be set by the DRCM server when the node
resource is activated.

# ifup eth0:1

Running the DRCM Server:
-----------------------
After setting up your cluster configuration and mirroring the
"/etc/drcm" directory to all nodes in the cluster, start the DRCM
server on each node. 

RHEL 7/CENTOS 7:

# systemctl start drcm_server
# systemctl status drcm_server

RHEL 6/CENTOS 6:

# service drcm_server start
# service drcm_server status

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

# chkconfig drcm_server on

DRCM Resource Scripts:
---------------------
The default DRCM resource script location is: /etc/drcm/resources

The default installation includes system resource script to manage
crontabs, floating IP addresses, system services, NFS and CIFS files
systems. The NFS and CIFS file system resource script will check for
stale mounts and automatically remount if a mount is stale.  

Which resource scripts used is set in the global resource
configuration. Cluster administrators can use custom resource scripts,
provided custom scripts accept the same input arguments as the
resource scripts included in the distribution. 

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

The "user:group" must be set to the username that will run the
application and the group associated with application.  

The "start_program" is the program used to start the application, with
or without input arguments. This can be multiple entries separated by
semicolons ; 

The "stop_program" is the program used to start the application, with
or without input arguments. This can be multiple entries separated by
semicolons ; 

The "ensure_script" is a script or program that checks to ensure the
application is running. If an ensure script is provided the DRCM
server will use the "ensure_script" to watch the application. 

NOTE: The user environment will be sourced so you do not have to use
absolute paths to programs if the program path is defined in the user
environment.   

Some examples:

ldm, ldm:users, ldmadmin start, ldmadmin stop

ldm, ldm:users, ldmadmin clean; ldmadmin mkqueue; ldmadmin start, ldmadmin stop; ldmadmin delqueue

ldm, ldm:users, ldmadmin start, ldmadmin stop, if [ -f ~/util/ldm_ensure.sh ]; then ~/util/ldm_ensure.sh; fi

Cluster Monitor and Reports
---------------------------
DRCM provides a formatted reporting tool and raw stats you can use to
build custom reports. To view the detailed cluster report, from any
cluster node:

/usr/sbin/drcm_server --client --command=cm_stat

To create custom reports use the rstats command from any cluster node, for example:

for CMNODE in cmnode1 cmnode2 cmnode3 cmnode4
> do
> echo $CMNODE
> /usr/sbin/drcm_server --client --command=rstats | sed -n "/$CMNODE/,/$CMNODE/p" | grep -v $CMNODE
> echo ""
> done

2-way file replication:
----------------------
The DRCM distributions include several utilities to synchronize file
systems between a primary and secondary cluster node. This is
typically used when a primary node is acting as a NFS server for other
servers and/or workstations. Or for primary nodes replicating database
directories to a cold spare.  

NOTE: File replication requires public SSH key authentication for the
backup user account that will run the replication scripts from the
user’s crontab. NOTE: If you are using "root" to run system backups
you can limit access to key SSH authentication to run specific
commands. Typically the back user is a special user account used just
for running file system backups. 

NOTE: Your backup user must have read/write access to the following
directory: 

/etc/drcm/my_cluster_conf

Following the DRCM installation, the file system utilities are located
in the following directory: 

/etc/drcm/file_system_sync

To configure a designated primary node and a designated backup node
you must set you node configuration in the following file: 

/etc/drcm/my_cluster_conf/my_cluster_info.sh

In the "my_cluster_info.sh" file set the following variables:

# Set a simple cluster name, any name you will recognize
# this cluster as, no spaces.
export CLUSTER_NAME="NFS-CLUSTER1"

# Set IP or hostname of online storage server for shared backups
# This is only need if you plan to backup the NFS or database 
# directories to a backup storage severs.
export BACKUP_STORAGE_SERVER="192.168.122.1"

# Set the destination directory on the storage server
export BACKUP_STORAGE_LOCATION="/backups"

# Set the user name that runs you backups.
# NOTE: This user name must have keyed SSH authentication access
# NOTE: to the cluster nodes and backup storage server
export BACKUP_USER="root"

#
# Set the following for 2-way file system replication
#
# IP address and Ethernet Interface of the cluster node with primary
# file systems. File systems on this node will be replicated to the
# backup file system in the event of a fail over to the backup node.
export PRIMARY_FILESYSTEMS_IP="192.168.122.111"
export PRIMARY_FILESYSTEMS_ETH="eth0"
#
# IP address and Ethernet Interface of the backup for this cluster
# node,
# where the primary file systems are mirrored too. When the primary
# node is down this node will host the file systems.
export BACKUP_FILESYSTEMS_IP="192.168.122.112"
export BACKUP_FILESYSTEMS_ETH="eth0"

# Allow RSYNC on a diffent IP/Interface if high bandwidth required to sync file systems
# These settings are used to move high bandwidth consumption to another Ethernet interface 
# and/or network.                                                         
## export PRIMARY_FILESYSTEMS_RSYNC_IP=""
## export BACKUP_FILESYSTEMS_RSYNC_IP=""

Once you setup your configuration, edit the following file to run a
test between the primary and backup node. As the backup user on the
primary node: 

$ vi /etc/drcm/my_cluster_conf/testfs_sync_list.sh

RsyncMirror /testfs

NOTE: Mirroring replicates an exact copy of the /testfs file
system. This means files will be copied and deleted as new files are
added or removed. If you want to keep copies of files removed on the
backup system use the following line 

RsyncCopy /testfs

NOTE: Prior to testing you must create the /testfs directory on both
cluster nodes and make sure your backup user has read/write access to
the /testfs directory.

Make sure to copy all changes in the /etc/drcm directory to the backup
node before you do any testing. To test, as the backup user on the
primary node, execute the following script: 

$ /etc/drcm/file_system_sync/sync_testfs.sh

This will replicate the contents of the /testfs directory to the
backup node. As the backup user on the backup node, execute the same
script: 

$ /etc/drcm/file_system_sync/sync_testfs.sh

You should see the following output on the backup node:

Checking to see if failback sync flag exists
We have no sync flag for testfs files
No file sync to primary cluster node will be performed

If the primary node is down the backup node will update the /testfs
directory until the primary node is back online. Once the primary node
is back online, all files on the backup will be replicated back to the
primary node. Once the back node has finished replication, the primary
node replicates to the back node. To test a primary to backup fail
over and fail back, execute the following script on the primary node,
as root or an admin account with sudo rights: 

# /etc/drcm/utils/manual_fs_failover.sh

To fail back, start the drcm_server on the primary node. 

After verifying 2-way file replication is work between the primary and
backup node, add the following line to the backup users crontab. On
the primary node as the backup user: 

$ crontab -e

*/5 * * * *  /etc/drcm/file_system_sync/sync_testfs.sh > /dev/null 2>&1

On the backup node:

$ crontab -e

* * * * * /etc/drcm/file_system_sync/sync_testfs.sh > /dev/null 2>&1

This will replicate /testfs to the backup node every 5 minutes. The
backup node will watch for primary node outages every minute. In the
event of a fail back, the backup node will replicate /testfs back to
primary within one minte. 

The DRCM distribution includes pre-built file replication scripts you
can add to the back user’s crontab: 

On primary node:

*/5 * * * *  /etc/drcm/file_system_sync/sync_mysql.sh > /dev/null 2>&1
*/5 * * * *  /etc/drcm/file_system_sync/sync_pgsql.sh > /dev/null 2>&1
*/6 * * * *  /etc/drcm/file_system_sync/sync_cifs.sh > /dev/null 2>&1
*/7 * * * *  /etc/drcm/file_system_sync/sync_nfs.sh > /dev/null 2>&1
*/8 * * * *  /etc/drcm/file_system_sync/sync_www.sh > /dev/null 2>&1
*/11 * * * * /etc/drcm/file_system_sync/sync_other.sh > /dev/null 2>&1

On backup node:

* * * * * /etc/drcm/file_system_sync/sync_mysql.sh > /dev/null 2>&1
* * * * * /etc/drcm/file_system_sync/sync_pgsql.sh > /dev/null 2>&1
* * * * * /etc/drcm/file_system_sync/sync_cifs.sh > /dev/null 2>&1
* * * * * /etc/drcm/file_system_sync/sync_nfs.sh > /dev/null 2>&1
* * * * * /etc/drcm/file_system_sync/sync_www.sh > /dev/null 2>&1
* * * * * /etc/drcm/file_system_sync/sync_other.sh > /dev/null 2>&1

All the configuration files will be autogened in the following:

/etc/drcm/my_cluster_conf

NOTE: Your backup user must have write access to the "my_cluster_conf"
directory. 

Edit the "/etc/drcm/my_cluster_conf/*sync_list.sh" config files to
setup the specific directories to be replicated. 

Setting up cluster alerts:
-------------------------
To setup an hourly cluster monitor with email and/or text message
alerts, add the following line to root’s crontab: 

00 * * * * /etc/drcm/utils/watch_cluster.sh > /dev/null 2>&1

The watch_cluster.sh script will automatically generate the following
an alert list configuration file for you: 

/etc/drcm/my_cluster_conf/cm_alert_list.sh

NOTE: If you run the watch_cluster.sh script as another user, the user
must have read access to the /etc/drcm/.auth directory and auth keys.

In your "cm_alert_list.sh" file set up the email accounts and/or SMS
accounts:

# vi /etc/drcm/my_cluster_conf/cm_alert_list.sh

# Set your comma delimited alert lists here
export EMAILlist="root,name@example.com"
export TEXTlist="555-555-5555@mobile_domain.com"

NOTE: Each mobile carrier has a domain for sending an email as a text
message, for example to send SMS messages to a Verizon number: 

555-555-5555@vtext.com

By default the DRCM alerts are limited to send messages every 4 hours,
once an the first message is sent. This is done to avoid a flood email
and/or text messages during periods of server outages or server
maintenance.  

If your clusters nodes do not have postfix mail configured, you need
to determine which Linux server in your data center is setup to relay
messages. To setup postfix mail to connect to relay host: 

# vi /etc/postfix/main.cf

relayhost = 192.168.122.1

Restart the postfix service and watch the mail log:

# tail -f /var/log/maillog

To test the alerting function:

$ vi ~/test_alert.sh 

#!/bin/bash

export ALERTTIMESPANHOURS="0"
SUBJECT="[!ACTION!] Cluster Alert Test Message"
BODY="This is an cluster manager alert test message"
source /etc/drcm/utils/send_alert.sh
send_alert "${SUBJECT}" "${BODY}"

Save file and exit VI.

$ chmod 755 ~/test_alert.sh
$ ~/test_alert.sh

NOTE: The DRCM alert functions can be use by other system admin
functions by writing scripts similar to the above example. 

If you do not have a Linux system setup up as a relay host, you can
setup postfix on one cluster node as a smart host and allow other
nodes to relay messages. For more information on setting up postfix
mail as a smart host, search online for: "postfix smarthost setup"

Remaining Work on This Project:
------------------------------ 

* Long term, add TCP cluster protocol for WAN based clusters

Support and Bug Tracking:
------------------------

https://github.com/datareel/datareel/issues
