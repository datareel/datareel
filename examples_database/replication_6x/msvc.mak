all:
	nmake -f writer.vc all
	nmake -f check.vc all

clean:
	nmake -f writer.vc clean
	nmake -f check.vc clean
