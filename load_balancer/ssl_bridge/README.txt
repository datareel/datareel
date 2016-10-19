README file for DRLB SSL Bridge
Last Modified: 10/19/2016

Contents:
--------
* Overview
* RPM Install
* Building and installing from source
* Setting up the SSL bridge service
* SSL certificates
* Self-signed SSL certificates
* SSL encryption protocol
* Remaining work on this project
* Support and Bug Tracking

Overview
--------
The DRLB SSL bridge is used to decrypt incoming SSL traffic on TCP
port 443 and send encrypted or unencrypted traffic to back-end web
servers. 

Load balancing SSL connections cannot forward IP address information
HTTP headers unless the SSL traffic is decrypted and re-encrypted with
the connection information inserted into the HTTP headers. The DRLB
SSL bridge has options to load balance front-end connections as an
HTTPS server and distribute web traffic to back-end HTTP or HTTPS
servers.

Decrypting front-end SSL traffic and re-encrypting allows you to
forward IP address information to back-end HTTPS servers. Both the
front-end and back-end web traffing remains encrypted during
transport.     

Sending decrypted SSL to back-end web servers over TCP port 80 allows
you to use existing back-end HTTP web services. The load balanced
front-end remains encrypted. The back-end web services can utilize
existing web configurations with having to be reconfigured to run SSL.  

RPM Install
-----------
System administrators, follow the instructions below to build an RPM
for distribution:

$ cd $HOME; mkdir -pv git; cd $HOME/git
$ git clone https://github.com/datareel/datareel
$ cd ${HOME}/git/datareel/load_balancer/rpm_builder

RHEL7/CENTOS7:
$ ./make_ssl_rpm_package.sh rhel7
$ sudo su root -c "yum -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_ssl_server-1.58-1.el7.x86_64.x86_64.rpm"

RHEL6/CENTOS6:
$ ./make_ssl_rpm_package.sh rhel6
$ sudo su root -c "yum -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_ssl_server-1.58-1.el6.x86_64.x86_64.rpm"

Useful RPM command to verify package contents:

List package: rpm -qf /usr/sbin/drlb_ssl_server
List files: rpm -ql drlb_ssl_server
List configs: rpm -qc drlb_ssl_server
List docs: rpm -qd drlb_ssl_server

To remove package:

$ sudo su root -c "yum -y remove drlb_ssl_server"

Building and installing from source
-----------------------------------
Follow the instructions below to build and install the DRLB SSL bridge
from source code: 

$ cd $HOME
$ mkdir -pv git
$ cd $HOME/git
$ git clone https://github.com/datareel/datareel
$ source $HOME/git/datareel/env/gnu_env.sh
$ cd $HOME/git/datareel/load_balancer/ssl_bridge
$ make
$ sudo su root -c 'make install_root'

Setting up the SSL bridge service
---------------------------------
The default LB configuration file for the SSL bridge is:

/etc/drlb/ssl_bridge.cfg

In the LB configuration file, refer to the following section for all
settings specific to the SSL config: 

[SSLCONFIG]
# Default SSL proto is set to SSLv2, SSLv3, TLSv1, TLSv1.1 and TLSv1.2
ssl_protocol = SSLv23  
ssl_key_file = /etc/drlb/ssl/private/ca.key
ssl_cert_file =  /etc/drlb/ssl/certs/ca.crt
#ssl_use_dhparms = 0
#ssl_dhparms_file =  /etc/drlb/ssl/certs/dhparams.pem
#ssl_encrypt_backend = 0
#ssl_backend_hostname = www.example.com
#ssl_verify_backend_cert = 0
#ssl_backend_ca_list_file = /etc/pki/tls/certs/ca-bundle.crt

By default back-end traffic is set to unencrypted, HTTP. If you
encrypt back-end traffic for HTTPS you have the option to verify the
back-end SSL certificates. The ssl_backend_hostname setting must match
the CN name of the issuer’s certificate. You must also provide a
trusted CA list using the ssl_backend_ca_list_file setting. 

For all other DRLB setting please refer to the DRLB README file:

# more /usr/share/doc/drlb_ssl_bridge/README.txt 

or online: 

http://datareel.com/drlb/

The default LB scheme used with the SSL bridge is round robin.

After setting up your LB and SSL config, start the
service:

RHEL 7/CENTOS 7:

# systemctl start drlb_ssl_server
# systemctl status drlb_ssl_server

RHEL 6/CENTOS 6:

# service drlb_ssl_server start
# service drlb_ssl_server status

To watch the log file:

# tail -f /var/log/drlb/ssl_bridge.log

To watch LB stats:

# tail -n 25 -f /var/log/drlb/ssl_bridge_stats.log

To make the LB service persistent: 

RHEL 7/CENTOS 7:

# systemctl enable drlb_ssl_server

RHEL 6/CENTOS 6:

# chkconfig drlb_ssl_server on

SSL certificates
----------------
Before starting the SSL bridge you must install or set the location to
your private key and certificate file:

# vi /etc/drlb/ssl_bridge.cfg

[SSLCONFIG]
...
ssl_key_file = /etc/drlb/ssl/private/ca.key
ssl_cert_file =  /etc/drlb/ssl/certs/ca.crt
ssl_use_dhparms = 1
ssl_dhparms_file =  /etc/drlb/ssl/certs/dhparams.pem

If you do not have an SSL certificate issued and verified by a trusted
Certificate Authority you can use a self-signed certificate for
non-production or internal HTTPS services. 

Self-signed SSL certificates
----------------------------
Follow the instructions below to create a self-signed certificate for
non-production or internal HTTPS services:

# mkdir -pv /etc/drlb/ssl/private /etc/drlb/ssl/certs
# openssl dhparam -out /etc/drlb/ssl/certs/dhparams.pem 2048
# openssl genrsa -out /etc/drlb/ssl/private/ca.key  2048
# chmod 600 /etc/drlb/ssl/private/ca.key

# openssl req -new -key /etc/drlb/ssl/private/ca.key -out /etc/drlb/ssl/certs/ca.csr

Country Name (2 letter code) [XX]:
State or Province Name (full name) []:
Locality Name (eg, city) [Default City]:
Organization Name (eg, company) [Default Company Ltd]:
Organizational Unit Name (eg, section) []:
Common Name (eg, your name or your server's hostname) []:
Email Address []: 

# openssl x509 -req -days 365 -in /etc/drlb/ssl/certs/ca.csr -signkey /etc/drlb/ssl/private/ca.key -out /etc/drlb/ssl/certs/ca.crt

# vi /etc/drlb/ssl_bridge.cfg

[SSLCONFIG]
...
ssl_key_file = /etc/drlb/ssl/private/ca.key
ssl_cert_file =  /etc/drlb/ssl/certs/ca.crt
ssl_use_dhparms = 1
ssl_dhparms_file =  /etc/drlb/ssl/certs/dhparams.pem

SSL encryption protocol
-----------------------
The SSL encryption protocol is set by the ssl_protocol setting in the
SSLCONFIG section in the LB configuration file: 

[SSLCONFIG]
...
ssl_protocol = SSLv23

The SSLv23 default setting uses SSL protocols SSLv2, SSLv3, TLSv1,
TLSv1.1 and TLSv1.2. Other protocol options are: 

SSLv2 - SSL version 2 
SSLv3 - SSL version 3  
TLSv1 - TLS version 1 
TLSv1_1 - TLS version 1.1
TLSv1_2 - TLS version 1.2

Remaining work on this project
------------------------------

Support and Bug Tracking
------------------------

https://github.com/datareel/datareel/issues
