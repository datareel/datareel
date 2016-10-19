all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlbldm.mak

clean:
	make -f makedrlbldm.mak clean
	rm -f core.*
	rm -f vgcore.*

clean_lib:
	rm -fv ../../lib/libdatareel.a
	rm -fv ../../lib/libdatareel.so
	rm -fv ../../lib/libdatareel.so.5.0

install:
	cp -fv drlb_ldm_server ../../bin/drlb_ldm_server
	chmod 755 ../../bin/drlb_ldm_server

install_root:
	mkdir -pv /etc/drlb
	chmod 755 /etc/drlb
	mkdir -pv /var/log/drlb
	chmod 755 /var/log/drlb
	mkdir -pv /var/run/drlb
	chmod 755 /var/run/drlb
	cp -fv drlb_ldm_server /usr/sbin/drlb_ldm_server
	chmod 755 /usr/sbin/drlb_ldm_server
	if [ ! -f /etc/drlb/ldm_cluster.cfg ]; then cp -fv ../etc/ldm_cluster.cfg  /etc/drlb/ldm_cluster.cfg; fi
	chmod 644 /etc/drlb/ldm_cluster.cfg
	if [ ! -f /etc/drlb/ldm_cluster_rules.cfg ]; then cp -fv ../etc/ldm_cluster_rules.cfg  /etc/drlb/ldm_cluster_rules.cfg; fi
	chmod 644 /etc/drlb/ldm_cluster_rules.cfg
	if [ ! -f /etc/sysconfig/drlb_ldm_server ]; then cp -fv ../etc/drlb_ldm_server_sysconfig  /etc/sysconfig/drlb_ldm_server; fi
	chmod 644 /etc/sysconfig/drlb_ldm_server
	cp -fv ../system5_init/drlb_ldm_server /etc/rc.d/init.d/drlb_ldm_server
	chmod 755 /etc/rc.d/init.d/drlb_ldm_server
	if [ ! -d /etc/systemd/system ]; then chkconfig --add drlb_ldm_server; fi
	if [ ! -d /etc/systemd/system ]; then chkconfig drlb_ldm_server off; fi
	if [ -d /etc/systemd/system ]; then cp -fv ../systemd_init/drlb_ldm_server.service  /etc/systemd/system/drlb_ldm_server.service; fi
	if [ -d /etc/systemd/system ]; then chmod 644 /etc/systemd/system/drlb_ldm_server.service; fi
	if [ -d /etc/systemd/system ]; then systemctl daemon-reload; fi
	mkdir -p /usr/share/doc/drlb_ldm
	chmod 755 /usr/share/doc/drlb_ldm
	cp -fv ../README.txt /usr/share/doc/drlb_ldm/README.txt
	chmod 644 /usr/share/doc/drlb_ldm/README.txt
	cp -fv README.txt /usr/share/doc/drlb_ldm/README_ldm.txt
	chmod 644 /usr/share/doc/drlb_ldm/README_ldm.txt
	cp -fv ../COPYING /usr/share/doc/drlb_ldm/COPYING
	chmod 644 /usr/share/doc/drlb_ldm/COPYING
	cp -fv version.txt /usr/share/doc/drlb_ldm/version.txt
	chmod 644 /usr/share/doc/drlb_ldm/version.txt
	gzip -c ../man/drlb_ldm_server.8 > /usr/share/man/man8/drlb_ldm_server.8.gz
	chmod 644  /usr/share/man/man8/drlb_ldm_server.8.gz

