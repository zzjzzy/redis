# Top level makefile, the real shit is at src/Makefile

default: all

.DEFAULT:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

mydebug:
	make clean
	make CFLAGS="-g -O0" MALLOC=jemalloc

myhiredis:
	cd deps && make hiredis && cd ..
#   如果按照下面命令报错【'jemalloc/jemalloc.h' file not found】，就单独把jemalloc make一次试试
#    make hiredis lua fpconv hdr_histogram jemalloc linenoise

cli:
	./src/redis-cli

cli1:
	./src/redis-cli -p 6380

cli2:
	./src/redis-cli -p 6381

sent:
	./src/redis-cli -p 26379

sent1:
	./src/redis-cli -p 26380

sent2:
	./src/redis-cli -p 26381

.PHONY: install
