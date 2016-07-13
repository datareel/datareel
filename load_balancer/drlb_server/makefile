all:
	if [ ! -f ../../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlb.mak

clean:
	make -f makedrlb.mak clean

install:
	cp -fv drlb_server ../../bin/drlb_server
	chmod 755 ../../bin/drlb_server



