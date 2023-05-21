SRC_NAME=pptpd-1.4.0
THISDIR=$(shell pwd)

PPPD_DIR=$(ROOTDIR)/user/pppd
PPPD_VER=2.4.7

CFLAGS  += -O2
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

all: extract_test config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xf $(SRC_NAME).tar.xz; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	./reconf ; \
	export ac_cv_header_libintl_h=no ; \
	export ac_cv_lib_intl_gettext=no ; \
	./configure \
		KDIR=$(ROOTDIR)/$(LINUXDIR) \
		PPPDIR=$(PPPD_DIR) \
		PPPVER=$(PPPD_VER) \
		--prefix=/usr \
		--enable-bcrelay \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/pptpd /usr/sbin/pptpd
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/pptpctrl /usr/sbin/pptpctrl
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/bcrelay /usr/sbin/bcrelay