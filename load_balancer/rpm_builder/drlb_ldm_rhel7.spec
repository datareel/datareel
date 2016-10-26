BuildArch: x86_64
Name: drlb_ldm_server
Version: 1.62        
Release: 1.el7.x86_64
Summary: Datareel LDM Load Balancer RPM
Group: System Environment/Daemons
License: GPL       
Distribution: Datareel Open Source
URL: http://datareel.com/drlb/ldm.html
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
DataReel load balancer (DRLB) for LDM protocol.

%install
tar xvf drlb_ldm_server.tar.gz -C $RPM_BUILD_ROOT

%files
%config(noreplace) %dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0755, root, root) "/var/log/drlb"
%dir %attr(0755, root, root) "/var/run/drlb"
%attr(0755, root, root) "/usr/sbin/drlb_ldm_server"
%config %attr(0644, root, root) "/etc/sysconfig/drlb_ldm_server"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/ldm_cluster.cfg"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/ldm_cluster_rules.cfg"
%config %attr(0644, root, root) "/etc/sysconfig/drlb_ldm_server"
%config %attr(0755, root, root) "/etc/rc.d/init.d/drlb_ldm_server"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/README_ldm.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man8/drlb_ldm_server.8.gz"
%config %attr(0644, root, root) "/etc/systemd/system/drlb_ldm_server.service"

%post -p /bin/sh

if [ $1 -eq 1 ] ; then 
    # Initial installation 
    systemctl preset drlb_ldm_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drlb_ldm_server.service > /dev/null 2>&1 || : 
    systemctl stop drlb_ldm_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drlb_ldm_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Tue Oct 24 2016 Datareel <datareel.com>
- Fix for joined feed types
* Mon Oct 24 2016 Datareel <datareel.com>
- Fix to reduce zero byte disconnects
- Thread Connection count fix
* Thu Oct 20 2016 Datareel <datareel.com>
- Fix for ldmsend file pattern name read
- Backward compatibility for LDM 6.6.5 clients
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for ABRT exit from safe_close call on closed file descriptor 
* Wed Oct 12 2016 Datareel <datareel.com>
- Fix for notifyme closing connection before completing
* Sun Oct 02 2016 Datareel <datareel.com>
- Initial RPM build 
