SRC_NAME = dnsproxy
THISDIR = $(shell pwd)

all: 
	echo "fake compile"

clean:
	echo "fake clean"

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME) /usr/bin/$(SRC_NAME)
