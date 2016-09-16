all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlb.mak

clean:
	make -f makedrlb.mak clean
	rm -f core.*
	rm -f vgcore.*

clean_lib:
	rm -fv ../../lib/libdatareel.a
	rm -fv ../../lib/libdatareel.so
	rm -fv ../../lib/libdatareel.so.5.0

install:
	cp -fv drlb_server ../../bin/drlb_server
	chmod 755 ../../bin/drlb_server

install_root:
	mkdir -pv /etc/drlb
	chmod 755 /etc/drlb
	mkdir -pv /var/log/drlb
	chmod 755 /var/log/drlb
	mkdir -pv /var/run/drlb
	chmod 755 /var/run/drlb
	cp -fv drlb_server /usr/sbin/drlb_server
	chmod 755 /usr/sbin/drlb_server
	if [ ! -f /etc/sysconfig/drlb_server ]; then cp -fv ../etc/drlb_server_sysconfig  /etc/sysconfig/drlb_server; fi
	chmod 644 /etc/sysconfig/drlb_server
	if [ ! -f /etc/drlb/active_configs.list ]; then cp -fv ../etc/active_configs.list_template /etc/drlb/active_configs.list; fi
	chmod 644 /etc/drlb/active_configs.list
	if [ ! -f /etc/drlb/http_rr_test.cfg ]; then cp -fv ../etc/http_rr_test.cfg /etc/drlb/http_rr_test.cfg; fi
	chmod 644 /etc/drlb/http_rr_test.cfg
	cp -fv ../system5_init/drlb_server /etc/rc.d/init.d/drlb_server
	chmod 755 /etc/rc.d/init.d/drlb_server
	if [ ! -d /etc/systemd/system ]; then chkconfig --add drlb_server; fi
	if [ ! -d /etc/systemd/system ]; then chkconfig drlb_server off; fi
	if [ -d /etc/systemd/system ]; then cp -fv ../systemd_init/drlb_server.service  /etc/systemd/system/drlb_server.service; fi
	if [ -d /etc/systemd/system ]; then chmod 644 /etc/systemd/system/drlb_server.service; fi
	if [ -d /etc/systemd/system ]; then systemctl daemon-reload; fi
	mkdir -p /usr/share/doc/drlb
	chmod 755 /usr/share/doc/drlb
	cp -fv ../README.txt /usr/share/doc/drlb/README.txt
	chmod 644 /usr/share/doc/drlb/README.txt
	cp -fv ../COPYING /usr/share/doc/drlb/COPYING
	chmod 644 /usr/share/doc/drlb/COPYING
	cp -fv ../version.txt /usr/share/doc/drlb/version.txt
	chmod 644 /usr/share/doc/drlb/version.txt
	gzip -c ../man/drlb_server.8 > /usr/share/man/man8/drlb_server.8.gz
	chmod 644  /usr/share/man/man8/drlb_server.8.gz

