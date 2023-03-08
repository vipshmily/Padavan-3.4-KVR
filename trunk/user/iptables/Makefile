SRC_NAME=iptables-1.8.9

CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

THISDIR = $(shell pwd)

all: extract_test config_test
	    $(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xf $(SRC_NAME).tar.xz ; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	./autogen.sh ; \
	./configure \
		--prefix= \
		--enable-static \
		--disable-shared \
		--disable-nftables \
		--disable-connlabel \
		--with-xtlibdir=/usr/lib/xtables \
		--with-kernel=$(ROOTDIR)/$(LINUXDIR) \
		--with-xt-lock-name="/var/lock/xtables.lock" \
		$(if $(CONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

romfs:
	mkdir -p $(ROMFSDIR)/usr/lib/xtables
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/iptables/xtables-legacy-multi /bin/xtables-multi
	$(ROMFSINST) -s xtables-multi /bin/iptables
	$(ROMFSINST) -s xtables-multi /bin/iptables-restore
	$(ROMFSINST) -s xtables-multi /bin/iptables-save
ifdef CONFIG_IPV6
	$(ROMFSINST) -s xtables-multi /bin/ip6tables
	$(ROMFSINST) -s xtables-multi /bin/ip6tables-restore
	$(ROMFSINST) -s xtables-multi /bin/ip6tables-save
endif