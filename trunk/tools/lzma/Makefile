SRC_NAME=lzma1900

all:
	make -j$(HOST_NCPU) -C $(SRC_NAME)/CPP/7zip/Bundles/LzmaCon -f makefile.gcc ;\
	cp $(SRC_NAME)/CPP/7zip/Bundles/LzmaCon/lzma ./lzma

clean:
	make -C $(SRC_NAME)/CPP/7zip/Bundles/LzmaCon -f makefile.gcc clean
	rm -f lzma
