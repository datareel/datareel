all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlb.mak

clean:
	make -f makedrlb.mak clean

install:
	cp -fv drlb_server ../../bin/drlb_server
	chmod 755 ../../bin/drlb_server

install_root:
	mkdir -pv /etc/drlb_server
	chmod 755 /etc/drlb_server
	mkdir -pv /var/log/drlb_server
	chmod 755 /var/log/drlb_server
	cp -fv drlb_server /usr/sbin/drlb_server
	chmod 755 /usr/sbin/drlb_server
	if [ ! -f /etc/sysconfig/drlb_server ]; then cp -fv ../etc/drlb_server_sysconfig /etc/sysconfig/drlb_server; fi
	chmod 644 /etc/sysconfig/drlb_server
	if [ ! -f /etc/drlb_server/active_configs.list ]; then cp -fv ../etc/active_configs.list_template /etc/drlb_server/active_configs.list; fi
	chmod 644 /etc/drlb_server/active_configs.list
	if [ ! -f /etc/drlb_server/http_rr_test.cfg ]; then cp -fv ../etc/http_rr_test.cfg /etc/drlb_server/http_rr_test.cfg; fi
	chmod 644 /etc/drlb_server/http_rr_test.cfg
	cp -fv ../system5_init/drlb_server /etc/rc.d/init.d/drlb_server
	chmod 755 /etc/rc.d/init.d/drlb_server
	if [ ! -d /etc/systemd/system ]; then chkconfig --add drlb_server; fi
	if [ -d /etc/systemd/system ]; then cp -fv ../systemd_init/drlb_server.service  /etc/systemd/system/drlb_server.service; fi
	if [ -d /etc/systemd/system ]; then chmod 644 /etc/systemd/system/drlb_server.service; fi
	if [ -d /etc/systemd/system ]; then systemctl daemon-reload; fi

