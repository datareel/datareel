all:
	nmake -f server.vc all
	nmake -f client.vc all

clean:
	nmake -f server.vc clean
	nmake -f client.vc clean

