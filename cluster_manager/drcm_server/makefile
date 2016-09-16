all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrcm.mak

clean:
	make -f makedrcm.mak clean

clean_lib:
	rm -fv ../../lib/libdatareel.a
	rm -fv ../../lib/libdatareel.so
	rm -fv ../../lib/libdatareel.so.5.0

install:
	cp -fv drcm_server ../../bin/drcm_server
	chmod 755 ../../bin/drcm_server

install_root:
	mkdir -pv /etc/drcm
	chmod 755 /etc/drcm
	chown root:root /etc/drcm
	cp -fv ../etc/cm_2_node_example.cfg /etc/drcm/.
	chmod 644 /etc/drcm/cm_2_node_example.cfg
	cp -fv ../etc/cm_4_node_example.cfg /etc/drcm/.
	chmod 644 /etc/drcm/cm_4_node_example.cfg
	chown root:root /etc/drcm/cm_2_node_example.cfg /etc/drcm/cm_4_node_example.cfg
	mkdir -pv /etc/drcm/file_system_sync
	chmod 755 /etc/drcm/file_system_sync
	cp -fv ../etc/file_system_sync/* /etc/drcm/file_system_sync/.
	chmod 755 /etc/drcm/file_system_sync/*
	chown root:root /etc/drcm/file_system_sync/*
	mkdir -pv /etc/drcm/utils
	chmod 755 /etc/drcm/utils
	cp -fv ../etc/utils/* /etc/drcm/utils/.
	chmod 755  /etc/drcm/utils/*
	chown root:root /etc/drcm/utils/*
	mkdir -pv /etc/drcm/my_cluster_conf
	chmod 755 /etc/drcm/my_cluster_conf
	if [ ! -f /etc/drcm/my_cluster_conf/my_cluster_info.sh ]; then cp -fv ../etc/my_cluster_conf/my_cluster_info.sh /etc/drcm/my_cluster_conf/my_cluster_info.sh; fi
	chmod 755 /etc/drcm/my_cluster_conf/my_cluster_info.sh
	cp -fv ../etc/my_cluster_conf/my_cluster_info_example1.sh /etc/drcm/my_cluster_conf/.
	chmod 644 /etc/drcm/my_cluster_conf/my_cluster_info_example1.sh
	cp -fv ../etc/my_cluster_conf/testfs_sync_list.sh /etc/drcm/my_cluster_conf/.
	chmod 644 /etc/drcm/my_cluster_conf/testfs_sync_list.sh
	chown root:root /etc/drcm/my_cluster_conf/*
	mkdir -pv /etc/drcm/system_templates
	chmod 755 /etc/drcm/system_templates
	cp -fv ../etc/system_templates/* /etc/drcm/system_templates/.
	chmod 644 /etc/drcm/system_templates/*
	chown root:root /etc/drcm/system_templates/*
	mkdir -pv /etc/drcm/resources
	chmod 755 /etc/drcm/resources
	cp -fv ../etc/resources/* /etc/drcm/resources/.
	chmod 755 /etc/drcm/resources/*
	mkdir -pv /etc/drcm/crontabs
	chmod 755 /etc/drcm/crontabs
	if [ ! -f /etc/drcm/crontabs/apps_package1 ]; then cp -fv ../etc/crontabs/apps_package1 /etc/drcm/crontabs/apps_package1; fi	
	if [ ! -f /etc/drcm/crontabs/apps_package2 ]; then cp -fv ../etc/crontabs/apps_package2 /etc/drcm/crontabs/apps_package2; fi	
	if [ ! -f /etc/drcm/crontabs/apps_package3 ]; then cp -fv ../etc/crontabs/apps_package3 /etc/drcm/crontabs/apps_package3; fi	
	if [ ! -f /etc/drcm/crontabs/apps_package4 ]; then cp -fv ../etc/crontabs/apps_package4 /etc/drcm/crontabs/apps_package4; fi	
	if [ ! -f /etc/drcm/crontabs/system_crons ]; then cp -fv ../etc/crontabs/system_crons /etc/drcm/crontabs/system_crons; fi	
	if [ ! -f /etc/drcm/crontabs/user_crons ]; then cp -fv ../etc/crontabs/user_crons /etc/drcm/crontabs/user_crons; fi	
	chmod 644 /etc/drcm/crontabs/*
	chown root:root /etc/drcm/crontabs/*
	mkdir -pv /etc/drcm/.auth
	chmod 700 /etc/drcm/.auth
	chown root:root /etc/drcm/.auth
	mkdir -pv /var/log/drcm
	chmod 755 /var/log/drcm
	mkdir -pv /var/run/drcm
	chmod 755 /var/run/drcm
	mkdir -pv /var/spool/drcm
	chmod 755 /var/spool/drcm
	cp -fv drcm_server /usr/sbin/drcm_server
	chmod 755 /usr/sbin/drcm_server
	if [ ! -f /etc/sysconfig/drcm_server ]; then cp -fv ../etc/system_templates/drcm_server_sysconfig /etc/sysconfig/drcm_server; fi
	chmod 644 /etc/sysconfig/drcm_server
	cp -fv ../system5_init/drcm_server /etc/rc.d/init.d/drcm_server
	chmod 755 /etc/rc.d/init.d/drcm_server
	if [ ! -d /etc/systemd/system ]; then chkconfig --add drcm_server; fi
	if [ ! -d /etc/systemd/system ]; then chkconfig drcm_server off; fi
	if [ -d /etc/systemd/system ]; then cp -fv ../systemd_init/drcm_server.service  /etc/systemd/system/drcm_server.service; fi
	if [ -d /etc/systemd/system ]; then chmod 644 /etc/systemd/system/drcm_server.service; fi
	if [ -d /etc/systemd/system ]; then systemctl daemon-reload; fi
	mkdir -p /usr/share/doc/drcm
	chmod 755 /usr/share/doc/drcm
	cp -fv ../README.txt /usr/share/doc/drcm/README.txt
	chmod 644 /usr/share/doc/drcm/README.txt
	cp -fv ../COPYING /usr/share/doc/drcm/COPYING
	chmod 644 /usr/share/doc/drcm/COPYING
	cp -fv ../version.txt /usr/share/doc/drcm/version.txt
	chmod 644 /usr/share/doc/drcm/version.txt
	gzip -c ../man/drcm_server.1 > /usr/share/man/man1/drcm_server.1.gz
	gzip -c ../man/drcm_server.8 > /usr/share/man/man8/drcm_server.8.gz
	chmod 644  /usr/share/man/man1/drcm_server.1.gz
	chmod 644 /usr/share/man/man8/drcm_server.8.gz


