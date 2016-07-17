all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrcm.mak

clean:
	make -f makedrcm.mak clean

install:
	cp -fv drcm_server ../../bin/drcm_server
	chmod 755 ../../bin/drcm_server

install_root:
	mkdir -pv /etc/drcm_server
	chmod 755 /etc/drcm_server
	mkdir -pv /var/log/drcm_server
	chmod 755 /var/log/drcm_server
	cp -fv drcm_server /sbin/drcm_server
	chmod 755 /sbin/drcm_server



