include $(ROOTDIR)/coustom
THISDIR = $(shell pwd)
#export GO111MODULE=on
#export GOPROXY=https://goproxy.bj.bcebos.com/
XRAY_VERSION := 1.8.6
XRAY_URL := https://codeload.github.com/XTLS/Xray-core/tar.gz/v$(XRAY_VERSION)
XRAY_DIR = xray-core/Xray-core-$(XRAY_VERSION)/main
ifeq ($(GITHUB_ACTION),n)

all: download build_extract build

download:
	( if [ ! -f $(THISDIR)/Xray-core-$(XRAY_VERSION).tar.gz ]; then \
	curl --create-dirs -L $(XRAY_URL) -o $(THISDIR)/Xray-core-$(XRAY_VERSION).tar.gz ; \
	fi )

build_extract:
	mkdir -p $(THISDIR)/xray-core
	mkdir -p $(THISDIR)/bin
	( if [ ! -d $(THISDIR)/xray-core/Xray-core-$(XRAY_VERSION) ]; then \
	rm -rf $(THISDIR)/xray-core/* ; \
	tar zxf $(THISDIR)/Xray-core-$(XRAY_VERSION).tar.gz -C $(THISDIR)/xray-core ; \
	fi )

build:
	( cd $(THISDIR)/$(XRAY_DIR); \
	if [ $(GOPROXY_ON) = "y" ]; then \
	go env -w GOPROXY=https://goproxy.cn,direct ; \
	fi ; \
	GOOS=linux GOARCH=mipsle go build -gcflags=all="-l" -ldflags "-w -s -buildid=" -trimpath -o $(THISDIR)/bin/xray; \
	)
endif
clean:
	rm -rf $(THISDIR)/xray-core
	rm -rf $(THISDIR)/bin

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/xray /usr/bin/xray