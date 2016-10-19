all:
	if [ ! -f ../../lib/libdatareel_ssl.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlbssl.mak

clean:
	make -f makedrlbssl.mak clean
	rm -f core.*
	rm -f vgcore.*

clean_lib:
	rm -fv ../../lib/libdatareel_ssl.a
	rm -fv ../../lib/libdatareel_ssl.so
	rm -fv ../../lib/libdatareel_ssl.so.5.0

install:
	cp -fv drlb_ssl_server ../../bin/drlb_ssl_server
	chmod 755 ../../bin/drlb_ssl_server

install_root:
	mkdir -pv /etc/drlb
	chmod 755 /etc/drlb
	mkdir -pv /var/log/drlb
	chmod 755 /var/log/drlb
	mkdir -pv /var/run/drlb
	chmod 755 /var/run/drlb
	mkdir -pv /etc/drlb/ssl/private /etc/drlb/ssl/certs
	chmod -R 755 /etc/drlb/ssl/private /etc/drlb/ssl/certs
	cp -fv drlb_ssl_server /usr/sbin/drlb_ssl_server
	chmod 755 /usr/sbin/drlb_ssl_server
	if [ ! -f /etc/drlb/ssl_bridge.cfg ]; then cp -fv ../etc/ssl_bridge.cfg  /etc/drlb/ssl_bridge.cfg; fi
	chmod 644 /etc/drlb/ssl_bridge.cfg
	if [ ! -f /etc/sysconfig/drlb_ssl_server ]; then cp -fv ../etc/drlb_ssl_server_sysconfig  /etc/sysconfig/drlb_ssl_server; fi
	chmod 644 /etc/sysconfig/drlb_ssl_server
	cp -fv ../system5_init/drlb_ssl_server /etc/rc.d/init.d/drlb_ssl_server
	chmod 755 /etc/rc.d/init.d/drlb_ssl_server
	if [ ! -d /etc/systemd/system ]; then chkconfig --add drlb_ssl_server; fi
	if [ ! -d /etc/systemd/system ]; then chkconfig drlb_ssl_server off; fi
	if [ -d /etc/systemd/system ]; then cp -fv ../systemd_init/drlb_ssl_server.service  /etc/systemd/system/drlb_ssl_server.service; fi
	if [ -d /etc/systemd/system ]; then chmod 644 /etc/systemd/system/drlb_ssl_server.service; fi
	if [ -d /etc/systemd/system ]; then systemctl daemon-reload; fi
	mkdir -p /usr/share/doc/drlb_ssl_bridge
	chmod 755 /usr/share/doc/drlb_ssl_bridge
	cp -fv ../README.txt /usr/share/doc/drlb_ssl_bridge/README.txt
	chmod 644 /usr/share/doc/drlb_ssl_bridge/README.txt
	cp -fv README.txt /usr/share/doc/drlb_ssl_bridge/README_ssl.txt
	chmod 644 /usr/share/doc/drlb_ssl_bridge/README_ssl.txt
	cp -fv ../COPYING /usr/share/doc/drlb_ssl_bridge/COPYING
	chmod 644 /usr/share/doc/drlb_ssl_bridge/COPYING
	cp -fv version.txt /usr/share/doc/drlb_ssl_bridge/version.txt
	chmod 644 /usr/share/doc/drlb_ssl_bridge/version.txt
	gzip -c ../man/drlb_ssl_server.8 > /usr/share/man/man8/drlb_ssl_server.8.gz
	chmod 644  /usr/share/man/man8/drlb_ssl_server.8.gz
