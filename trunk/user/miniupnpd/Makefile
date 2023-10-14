SRC_NAME=miniupnpd-2.x
SRC_URL=http://miniupnp.free.fr/files/download.php?file=$(SRC_NAME).tar.gz

# reference to iptables package
IPT_VERSION:=iptables-1.8.10

# disable support IPv6 yet, because no profit w/o IGDv2
# e.g. Windows not supported IGDv2 and UPnP not worked
ENABLE_IPV6:=0

IPTABLESPATH=$(ROOTDIR)/user/iptables/$(IPT_VERSION)

all: extract_test
#	cd $(SRC_NAME) && ./genconfig.sh
	$(MAKE) -f Makefile.linux -j$(HOST_NCPU) -C $(SRC_NAME) IPTABLESPATH=$(IPTABLESPATH) ENABLE_IPV6=$(ENABLE_IPV6)

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
	    tar -xf $(SRC_NAME).tar.xz; \
		cd $(SRC_NAME) && ./genconfig.sh ; \
	fi )

clean:
	$(MAKE) -f Makefile.linux -C $(SRC_NAME) clean
	rm -f miniupnpd

romfs:
	cp $(SRC_NAME)/miniupnpd .
	$(ROMFSINST) /usr/bin/miniupnpd