BuildArch: x86_64
Name: drcm_server
Version: 1.37        
Release: 1.el7.x86_64
Summary: Datareel Cluster Manager RPM        
Group: System Environment/Daemons
License: GPL       
Distribution: Datareel Open Source
URL: http://datareel.com/drcm
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
The DataReel cluster manager (DRCM) software is used to build high
availability Linux server clusters. High availability clusters manage
system resources with automatic fail over in event of one or more
server failures. 

%install
tar xvf drcm_server.tar.gz -C $RPM_BUILD_ROOT

%files
%config(noreplace) %dir %attr(0755, root, root) "/etc/drcm"
%config %attr(0644, root, root) "/etc/drcm/cm_2_node_example.cfg"
%config %attr(0644, root, root) "/etc/drcm/cm_4_node_example.cfg"
%config(noreplace) %dir %attr(0755, root, root) "/etc/drcm/file_system_sync"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/backup_to_shared_storage.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_cifs.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_file_system_include.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_mysql.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_nfs.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_other.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_pgsql.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_testfs.sh"
%config %attr(0755, root, root) "/etc/drcm/file_system_sync/sync_www.sh"
%config(noreplace) %dir %attr(0755, root, root) "/etc/drcm/utils"
%config %attr(0755, root, root) "/etc/drcm/utils/calc_runtime.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/clean_rsync_procs.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/cm_stat"
%config %attr(0755, root, root) "/etc/drcm/utils/fs_has_failedover.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/is_backup_fs.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/is_primary_fs.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/manual_fs_failover.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/process_lock_net.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/process_lock.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/send_alert.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/test_process_lock_net.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/test_process_lock.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/watch_cluster.sh"
%config %attr(0755, root, root) "/etc/drcm/utils/watch_service.sh"
%config(noreplace) %dir %attr(0755, root, root) "/etc/drcm/my_cluster_conf"
%config(noreplace) %attr(0755, root, root) "/etc/drcm/my_cluster_conf/my_cluster_info.sh"
%config %attr(0755, root, root) "/etc/drcm/my_cluster_conf/my_cluster_info_example1.sh"
%config %attr(0755, root, root) "/etc/drcm/my_cluster_conf/testfs_sync_list.sh"
%config(noreplace) %dir %attr(0755, root, root) "/etc/drcm/resources"
%config(noreplace) %attr(0755, root, root) "/etc/drcm/resources/cifs.sh"
%config(noreplace) %attr(0755, root, root) "/etc/drcm/resources/crontab.sh"
%config(noreplace) %attr(0755, root, root) "/etc/drcm/resources/ipv4addr.sh"
%config(noreplace) %attr(0755, root, root) "/etc/drcm/resources/nfs.sh"
%config(noreplace) %attr(0755, root, root) "/etc/drcm/resources/service.sh"
%config(noreplace) %dir %attr(0755, root, root) "/etc/drcm/crontabs"
%config(noreplace) %attr(0644, root, root) "/etc/drcm/crontabs/apps_package1"
%config(noreplace) %attr(0644, root, root) "/etc/drcm/crontabs/apps_package2"
%config(noreplace) %attr(0644, root, root) "/etc/drcm/crontabs/apps_package3"
%config(noreplace) %attr(0644, root, root) "/etc/drcm/crontabs/apps_package4"
%config(noreplace) %attr(0644, root, root) "/etc/drcm/crontabs/system_crons"
%config(noreplace) %attr(0644, root, root) "/etc/drcm/crontabs/user_crons"
%config(noreplace) %dir %attr(0700, root, root) "/etc/drcm/.auth"
%dir %attr(0755, root, root) "/var/log/drcm"
%dir %attr(0755, root, root) "/var/spool/drcm"
%dir %attr(0755, root, root) "/var/run/drcm"
%attr(0755, root, root) "/usr/sbin/drcm_server"
%config %attr(0755, root, root) "/etc/rc.d/init.d/drcm_server" 
%config %attr(0755, root, root) "/etc/sysconfig/drcm_server"
%docdir %attr(0755, root, root) "/usr/share/doc/drcm"
%doc %attr(0644, root, root) "/usr/share/doc/drcm/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drcm/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drcm/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man1/drcm_server.1.gz"
%doc %attr(0644, root, root) "/usr/share/man/man8/drcm_server.8.gz"
%config %attr(0644, root, root) "/etc/systemd/system/drcm_server.service" 

%post -p /bin/sh

if [ $1 -eq 1 ] ; then 
    # Initial installation 
    systemctl preset drcm_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drcm_server.service > /dev/null 2>&1 || : 
    systemctl stop drcm_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drcm_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Sat Oct 01 2016 Datareel <datareel.com>
- Initial RPM build 
