# Top level makefile, the real shit is at src/Makefile

default: all

.DEFAULT:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

mydebug:
	make clean
	make CFLAGS="-g -O0" MALLOC=jemalloc

.PHONY: install
