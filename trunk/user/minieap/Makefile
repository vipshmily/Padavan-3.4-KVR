SRC_NAME = minieap-0.93.7
SRC_URL = https://codeload.github.com/AutoCONFIG/minieap/zip/refs/tags/0.93.7
THISDIR = $(shell pwd)

all: download_test extract_test
	    $(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) LIBICONV_STANDALONE=true

download_test: 
	( if [ ! -f $(THISDIR)/$(SRC_NAME).zip ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).zip $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(THISDIR)/$(SRC_NAME) ]; then \
		unzip -qq $(SRC_NAME).zip; \
		cat ./patch/*.patch | patch -d $(SRC_NAME) -p1 ; \
	fi )

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/minieap /usr/bin/minieap
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/minieap.conf /etc_ro/minieap.conf

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) clean ; \
	fi ; \
