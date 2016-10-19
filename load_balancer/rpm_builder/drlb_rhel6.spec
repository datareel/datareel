BuildArch: x86_64
Name: drlb_server
Version: 1.58        
Release: 1.el6.x86_64
Summary: Datareel Load Balancer RPM
Group: System Environment/Daemons
License: GPL       
Distribution: Datareel Open Source
URL: http://datareel.com/drlb
Vendor: Datareel Open Source
Requires: libpthread.so.0()(64bit)
Requires: libstdc++.so.6()(64bit)
Requires: libm.so.6()(64bit)
Requires: libgcc_s.so.1()(64bit)
Requires: libc.so.6()(64bit)
Requires: ld-linux-x86-64.so.2()(64bit)
Requires: chkconfig  
Requires: /bin/sh  
Requires: /bin/bash

%description
The DataReel load balancer (DRLB) is a software based TCP 
load balancing. Designed to handle any type of TCP connection, 
acting as a front end server distributing high volumes of TCP 
connections to multiple back end servers.

%install
tar xvf drlb_server.tar.gz -C $RPM_BUILD_ROOT

%files
%config(noreplace) %dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0755, root, root) "/var/log/drlb"
%dir %attr(0755, root, root) "/var/run/drlb"
%attr(0755, root, root) "/usr/sbin/drlb_server"
%config %attr(0644, root, root) "/etc/sysconfig/drlb_server"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/active_configs.list"
%config %attr(0644, root, root) "/etc/drlb/http_rr_test.cfg"
%config %attr(0755, root, root) "/etc/rc.d/init.d/drlb_server"
%doc %attr(0644, root, root) "/usr/share/doc/drlb/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drlb/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man8/drlb_server.8.gz"

%post -p /bin/sh
/sbin/chkconfig --add drlb_server
/sbin/chkconfig drlb_server off

%preun -p /bin/sh
if [ "$1" = 0 ]
then
        /sbin/service drlb_server stop > /dev/null 2>&1 || :
        /sbin/chkconfig --del drlb_server
fi
%postun -p /bin/sh
/sbin/service drlb_server restart > /dev/null 2>&1 || :

%changelog
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for potential ABRT exit from safe_close call on closed file descriptor
* Sun Oct 02 2016 Datareel <datareel.com>
- Initial RPM build 
