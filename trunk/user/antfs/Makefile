# source from http://osp.avm.de/fritzbox/fritzbox-7560/source-files-FRITZ.Box_7560-07.11.tar
# 07.29  from http://osp.avm.de/fritzbox/fritzbox-7590/source-files-FRITZ.Box_7590-grx5-07.29.tar.gz
SRC_NAME = source-files-FRITZ.Box_7590-grx5-07.29
SRC_URL = http://osp.avm.de/fritzbox/fritzbox-7590/source-files-FRITZ.Box_7590-grx5-07.29.tar.gz
THISDIR = $(shell pwd)

all: download_test extract_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)/build

download_test:
	( if [ ! -f $(THISDIR)/$(SRC_NAME).tar.gz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )
	
extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar -zxf $(SRC_NAME).tar.gz; \
	fi )
#all:
ifeq ($(CONFIG_WITHOUT_KERNEL),y)
	@echo "antfs would not build!"
else
	$(MAKE) -C $(SRC_NAME)
	cp -f $(SRC_NAME)/antfs.ko .
endif

romfs:

clean:
	$(MAKE) -C $(SRC_NAME) clean
	rm -f *.ko