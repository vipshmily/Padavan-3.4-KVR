SRC_NAME = dnsproxy-1.1.1
THISDIR = $(shell pwd)

all: 
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) CC="$(CC)"

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) clean ; \
	fi ; \
	rm -f config_done

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/src/dnsproxy /usr/bin/dnsproxy
