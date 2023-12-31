SRC_NAME=libwebsockets-4.3.3
SRC_URL=https://github.com/warmcat/libwebsockets/archive/v4.3.3.tar.gz

all: download_test extract_test config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)/build

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar zxf $(SRC_NAME).tar.gz; \
		patch -p1 -d $(SRC_NAME) < $(SRC_NAME).patch ; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( mkdir -p $(SRC_NAME)/build && cd $(SRC_NAME)/build ; \
	cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$(STAGEDIR) \
	-DCMAKE_TOOLCHAIN_FILE=$(CONFIG_CMAKE_TOOLCHAIN_FILE) \
	-DLWS_WITH_LIBUV=ON \
	-DLWS_WITHOUT_BUILTIN_SHA1=ON \
	-DLWS_WITHOUT_TESTAPPS=ON \
	-DLWS_WITH_CUSTOM_HEADERS=ON \
	-DLWS_IPV6=ON \
	-DLWS_ROLE_RAW_FILE=OFF \
	-DLWS_WITH_HTTP2=OFF \
	-DLWS_WITH_HTTP_BASIC_AUTH=OFF \
	-DLWS_WITH_UDP=OFF \
	-DLWS_WITHOUT_CLIENT=ON \
	.. \
	)

clean:
	rm -rf $(SRC_NAME)/build
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME)/build install DESTDIR=""

romfs:
	cp -fP $(DESTDIR)/lib/libwebsockets.so* $(ROMFSDIR)/lib
	cp -fP $(DESTDIR)/lib/libwebsockets-evlib_uv.so $(ROMFSDIR)/lib
