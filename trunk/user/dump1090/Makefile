SRC_NAME = dump1090-master
SRC_URL = https://github.com/antirez/dump1090/archive/master.zip

THISDIR = $(shell pwd)

all: download_test extract_test
	    $(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).zip ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).zip $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		unzip -qq $(SRC_NAME).zip; \
	fi )

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) clean ; \
	fi

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/dump1090 /usr/bin/dump1090
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/gmap.html /usr/bin/gmap.html
	$(ROMFSINST) -d $(THISDIR)/$(SRC_NAME)/tools/debug.html /www/dump1090_debug.html
	ln -sf /tmp/frames.js $(INSTALLDIR)/www/frames.js
