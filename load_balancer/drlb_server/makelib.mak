all:
	cd ../../build; make

install:
	cd ../../build; make install

clean:
	cd ../../build; make clean 

all_install:
	cd ../../build; make
	cd ../../build; make install
	cd ../../build; make clean
