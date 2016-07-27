all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrcm.mak

clean:
	make -f makedrcm.mak clean

install:
	cp -fv drcm_server ../../bin/drcm_server
	chmod 755 ../../bin/drcm_server

install_root:
	mkdir -pv /etc/drcm
	chmod 755 /etc/drcm
	mkdir -pv /etc/drcm/resources
	chmod 755 /etc/drcm/resources
	cp -fpv ../etc/resources/* /etc/drcm/resources/.
	chmod 755 /etc/drcm/resources/*
	mkdir -pv /etc/drcm/crontabs
	chmod 755 /etc/drcm/crontabs
	if [ ! -f /etc/drcm/crontabs/apps_package1 ]; then cp -fv ../etc/crontabs/apps_package1 /etc/drcm/crontabs/apps_package1; fi	
	if [ ! -f /etc/drcm/crontabs/apps_package2 ]; then cp -fv ../etc/crontabs/apps_package2 /etc/drcm/crontabs/apps_package2; fi	
	if [ ! -f /etc/drcm/crontabs/system_crons ]; then cp -fv ../etc/crontabs/system_crons /etc/drcm/crontabs/system_crons; fi	
	if [ ! -f /etc/drcm/crontabs/user_crons ]; then cp -fv ../etc/crontabs/user_crons /etc/drcm/crontabs/user_crons; fi	
	chmod 644 /etc/drcm/crontabs/*
	mkdir -pv /etc/.auth
	chmod 700 /etc/.auth
	mkdir -pv /var/log/drcm
	chmod 755 /var/log/drcm
	mkdir -pv /var/run/drcm
	chmod 755 /var/run/drcm
	mkdir -pv /var/spool/drcm
	chmod 755 /var/spool/drcm
	cp -fv drcm_server /sbin/drcm_server
	chmod 755 /sbin/drcm_server

