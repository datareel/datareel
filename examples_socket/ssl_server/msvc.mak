PROJECT = server
# 10/24/2026: SSL is built into the gxcode library. Uses native Schannel
# by default. Set USESSL = 1 to build against the OpenSSL gxcode library.
# The OpenSSL build must use an OpenSSL 3.5.x LTS release.
#USESSL = 1
!include ../../env/msvc.mak
