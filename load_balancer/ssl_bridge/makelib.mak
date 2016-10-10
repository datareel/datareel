USE_OPEN_SSL = YES

all:
	cd ../../build; make -f makefile_ssl

install:
	cd ../../build; make -f makefile_ssl install

clean:
	cd ../../build; make -f makefile_ssl clean 

all_install:
	cd ../../build; make -f makefile_ssl
	cd ../../build; make -f makefile_ssl install
	cd ../../build; make -f makefile_ssl clean
