BuildArch: x86_64
Name: drlb_ssl_server
Version: 1.58        
Release: 1.el7.x86_64
Summary: Datareel SSL Load Balancer RPM
Group: System Environment/Daemons
License: GPL       
Distribution: Datareel Open Source
URL: http://datareel.com/drlb/ssl.html
Vendor: Datareel Open Source

Requires: openssl-libs
Requires: openssl
Requires: libdl.so.2()(64bit)
Requires: libssl.so.10()(64bit)
Requires: libcrypto.so.10()(64bit)
Requires: libgssapi_krb5.so.2()(64bit)
Requires: libkrb5.so.3()(64bit)
Requires: libcom_err.so.2()(64bit)
Requires: libk5crypto.so.3()(64bit)
Requires: libz.so.1()(64bit)
Requires: libkrb5support.so.0()(64bit)
Requires: libkeyutils.so.1()(64bit)
Requires: libresolv.so.2()(64bit)
Requires: libselinux.so.1()(64bit)
Requires: libpcre.so.1()(64bit)
Requires: liblzma.so.5()(64bit)
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
DataReel load balancer (DRLB) for SSL protocol.

%install
tar xvf drlb_ssl_server.tar.gz -C $RPM_BUILD_ROOT

%files
%config(noreplace) %dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0700, root, root) "/etc/drlb/ssl/private"
%dir %attr(0755, root, root) "/etc/drlb/ssl/certs"
%dir %attr(0755, root, root) "/var/log/drlb"
%dir %attr(0755, root, root) "/var/run/drlb"
%attr(0755, root, root) "/usr/sbin/drlb_ssl_server"
%config %attr(0644, root, root) "/etc/sysconfig/drlb_ssl_server"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/ssl_bridge.cfg"
%config %attr(0644, root, root) "/etc/sysconfig/drlb_ssl_server"
%config %attr(0755, root, root) "/etc/rc.d/init.d/drlb_ssl_server"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/README_ssl.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man8/drlb_ssl_server.8.gz"
%config %attr(0644, root, root) "/etc/systemd/system/drlb_ssl_server.service"

%post -p /bin/sh

if [ $1 -eq 1 ] ; then 
    # Initial installation 
    systemctl preset drlb_ssl_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drlb_ssl_server.service > /dev/null 2>&1 || : 
    systemctl stop drlb_ssl_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drlb_ssl_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for potential ABRT exit from safe_close call on closed file descriptor
* Sun Oct 10 2016 Datareel <datareel.com>
- Initial RPM build 
