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
	cp -fv drlb_server /sbin/drlb_server
	chmod 755 /sbin/drlb_server



