PROJECT = drlb_ssl_server
USE_OPEN_SSL = YES
CXXFLAGS := $(CXXFLAGS) -I../drlb_server
include ../../env/linux.mak
