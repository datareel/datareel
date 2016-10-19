BuildArch: x86_64
Name: drlb_server
Version: 1.58        
Release: 1.el7.x86_64
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
Requires: systemd-units  
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
%config %attr(0644, root, root) "/etc/systemd/system/drlb_server.service"

%post -p /bin/sh

if [ $1 -eq 1 ] ; then 
    # Initial installation 
    systemctl preset drlb_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drlb_server.service > /dev/null 2>&1 || : 
    systemctl stop drlb_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drlb_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for potential ABRT exit from safe_close call on closed file descriptor
* Sun Oct 02 2016 Datareel <datareel.com>
- Initial RPM build 
